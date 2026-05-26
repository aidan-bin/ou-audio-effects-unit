#include "effects.h"
#include "fast_math.h"

#include <stdio.h>
#include <string.h>
#include <math.h>

int32_t saturate_amplitude(int32_t num, size_t max);
int32_t saturate_min(int32_t num, int32_t min);

//#define PARAM_INSTANCE_ENABLE	// Enables instances of parameters

#ifdef PARAM_INSTANCE_ENABLE
volatile OverdriveParam overdrive = {
	.level = MAX_OVERDRIVE_LEVEL,
	.gain = MAX_OVERDRIVE_GAIN,
	.tone = MAX_OVERDRIVE_TONE,
	.mix = MAX_OVERDRIVE_MIX };

volatile EchoParam echo = {
	.delay_samples = MAX_ECHO_DELAY_SAMPLES,
	.pre_delay = MIN_ECHO_PRE_DELAY,
	.density = MAX_ECHO_DENSITY,
	.attack = MAX_ECHO_ATTACK,
	.decay = MAX_ECHO_DECAY };

volatile CompressionParam compression = {
	.threshold = MAX_COMPRESSION_THRESHOLD,
	.ratio = MAX_COMPRESSION_RATIO };
#endif

/* Buffered overdrive/distortion:
 *	- Distortion/overdrive clips input signal to a threshold, which makes it resemble a square wave
 *	- Uses tanh of input
 */
void buf_overdrive(const uint16_t* in_buf, uint16_t* out_buf, size_t num_samples, const OverdriveParam* param)
{
	size_t level = param->level;
	size_t gain = param->gain;
	size_t tone = param->tone;
	size_t mix = param->mix;

	for (size_t n = 0; n < num_samples; n++)
	{
		int32_t input = (int32_t) in_buf[n] - X_AXIS;
		int32_t output;

		size_t wet_amount = mix;
		size_t dry_amount = (1U << FIXED_POINT_Q) - mix;

		// Saturate the input of tanh to avoid overflow
		output = (int32_t) level * q_tanh(saturate_amplitude((int32_t) tone * input, INT16_MAX));
		output = (int32_t) gain * ((int32_t) wet_amount * output >> FIXED_POINT_Q) >> FIXED_POINT_Q;
		output += (int32_t) dry_amount * input;

		out_buf[n] = (int16_t) (output >> FIXED_POINT_Q) + X_AXIS;
	}
}

/* Buffered echo:
 *	- Echo with max delay of (MAX_DELAY_SAMPLES / Sample rate)
 *  - Note: num_samples is the size of out_buf
 *  - Note: in_buf is assumed to have delay_samples of previous samples, plus num_samples of current samples (those to which to add echo)
 */
void buf_echo(const uint16_t* in_buf, uint16_t* out_buf, size_t num_samples, const EchoParam* param)
{
	size_t delay_samples = param->delay_samples;
	size_t pre_delay = param->pre_delay;
	size_t density = param->density;
	size_t attack = param->attack;
	size_t decay = param->decay;
	
	// Start of current samples of in_buf
	const uint16_t* in_buf_curr = &in_buf[delay_samples];

	// Copy current input samples to output samples buffer
	memcpy(out_buf, in_buf_curr, num_samples * sizeof(uint16_t));

	size_t echo_spacing = (1U << FIXED_POINT_Q) / density;

	// For each sample of in_buf, add its echo to out_buf
	for (size_t n = 0; n < delay_samples + num_samples - 1; n++)
	{
		int32_t dry_input = (int32_t) in_buf[n] - X_AXIS;
		
		size_t curr_echo_gain = attack;

		// Add each echo to the appropriate sample of out_buf
		for (size_t delay = pre_delay; delay <= delay_samples; delay += echo_spacing)
		{
			// Make sure echo is within bounds of out_buf, ignore otherwise (still decay/reduce gain)
			if (n + delay >= delay_samples && n + delay < num_samples)
			{
				out_buf[n + delay - delay_samples] += (int32_t) curr_echo_gain * dry_input >> FIXED_POINT_Q;
			}

			// Reduce echo gain by decay, saturating to 0
			curr_echo_gain = saturate_min((int32_t) curr_echo_gain - (int32_t) decay, 0);
			
		}
	}
}

/* Buffered compression:
 *	- Dynamic range compression
 *	- Reduces signal above threshold based on ratio
 * 	- Ratio is in QN
 */
void buf_compression(const uint16_t* in_buf, uint16_t* out_buf, size_t num_samples, const CompressionParam* param)
{
	size_t threshold = param->threshold;
	size_t ratio = param->ratio;
	
	// Excess gets scaled by (1 - ratio) before being added back to threshold
	ratio = (1L << FIXED_POINT_Q) - ratio;
	
	for (size_t n = 0; n < num_samples; n++)
	{
		int32_t sample = (int32_t) in_buf[n] - X_AXIS;

		if (sample > (int32_t) threshold)
		{
			// Scale down excess above threshold using ratio
			uint32_t excess = sample - (int32_t) threshold;

			sample = (excess * ratio >> FIXED_POINT_Q) + (int32_t) threshold;
		}
		else if (sample < (int32_t) -threshold)
		{
			// Scale down excess above threshold using ratio (make sure both are at QN)
			uint32_t excess = (int32_t) -threshold - sample;

			sample = (int32_t) -threshold - (excess * ratio >> FIXED_POINT_Q);
		}

		out_buf[n] = (uint16_t) sample + X_AXIS;
	}
}

/* Buffered moving averager:
 * 	- Averages over num_taps * num_copies number of samples (plus edge samples)
 *	- Edge samples are those that go just before in_buf (num_taps - 1 in total), for ping-pong buffering
 * 	- Passes linearly over combined buffer [edge_samples_buf:in_buf] in groups of num_taps
 * 	- Each pass computes one output sample using num_taps number of input samples
 *	- shift is the right shift amount of the output (division by log base 2 num_taps)
 * 	- Assumes num_taps is a power of two (output get truncated otherwise, causing a loss of gain)
 *	- Assumes in_buf is at least num_taps * num_copies long
 * TODO:
 * 	- Use saturated arithmetic
 *  - Use uint16_t ONLY (can shift each value individually, rather than at the end)
 */
void buf_averager(const uint16_t* in_buf, uint16_t* out_buf, const uint16_t* edge_samples_buf, size_t num_taps, size_t num_copies, size_t shift)
{
	const size_t edge_samples = num_taps - 1;
	const size_t in_buf_size = num_taps * num_copies;
	
	// Calculate each output sample
	// 	n is the index of the oldest sample in the current filter pass
	// 	Note: starts at n = -edge_samples, where n = 0 is the start of in_buf
	for (int n = -edge_samples; n < (int) in_buf_size - (int) edge_samples; n++)
	{
		uint32_t out = 0;
		
		// Sum the num_taps number of samples starting from in_buf[i]
		// 	offset is number of unit delays current sample is ahead of n
		for (size_t offset = 0; offset < num_taps; offset++)
		{
			if (n + (int) offset < 0)
			{
				// Get samples from edge_sample_buf
				out += edge_samples_buf[n + (int) offset + (int) edge_samples];
			}
			else
			{
				// Get samples from in_buf
				out += in_buf[n + offset];
			}
		}
		
		// Divide value by coefficient (by shifting)
		out_buf[n + edge_samples] = (uint16_t) (out >> shift);
	}
}

// Saturate num to +/- max
int32_t saturate_amplitude(int32_t num, size_t max)
{
	if (num > (int32_t) max)
        return max;
	else if (num < (int32_t) -max)
		return -max;
	else
		return num;
}

// Saturate num to min
int32_t saturate_min(int32_t num, int32_t min)
{
	if (num < min)
        return min;
	else
		return num;
}
