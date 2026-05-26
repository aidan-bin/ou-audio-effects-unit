#include "effects.h"
#include "fast_math.h"

#include <string.h>

static int32_t saturate_amplitude(int32_t num, size_t max);
static int32_t saturate_min(int32_t num, int32_t min);
static uint16_t saturate_u16(int32_t num);

// #define PARAM_INSTANCE_ENABLE	// Enables instances of parameters

#ifdef PARAM_INSTANCE_ENABLE
volatile OverdriveParam overdrive = {.level = MAX_OVERDRIVE_LEVEL,
                                     .gain = MAX_OVERDRIVE_GAIN,
                                     .tone = MAX_OVERDRIVE_TONE,
                                     .mix = MAX_OVERDRIVE_MIX};

volatile EchoParam echo = {.delay_samples = MAX_ECHO_DELAY_SAMPLES,
                           .pre_delay = MIN_ECHO_PRE_DELAY,
                           .density = MAX_ECHO_DENSITY,
                           .attack = MAX_ECHO_ATTACK,
                           .decay = MAX_ECHO_DECAY};

volatile CompressionParam compression = {.threshold = MAX_COMPRESSION_THRESHOLD,
                                         .ratio = MAX_COMPRESSION_RATIO};
#endif

/* Buffered overdrive/distortion:
 *	- Distortion/overdrive clips input signal to a threshold, which makes it resemble a square
 *wave
 *	- Uses tanh of input
 */
void buf_overdrive(const uint16_t *in_buf, uint16_t *out_buf, size_t num_samples,
                   const OverdriveParam *param) {
    size_t level = param->level;
    size_t gain = param->gain;
    size_t tone = param->tone;
    size_t mix = param->mix;

    if (level > MAX_OVERDRIVE_LEVEL) {
        level = MAX_OVERDRIVE_LEVEL;
    }

    if (gain > MAX_OVERDRIVE_GAIN) {
        gain = MAX_OVERDRIVE_GAIN;
    }

    if (tone < MIN_OVERDRIVE_TONE) {
        tone = MIN_OVERDRIVE_TONE;
    } else if (tone > MAX_OVERDRIVE_TONE) {
        tone = MAX_OVERDRIVE_TONE;
    }

    if (mix > (1U << FIXED_POINT_Q)) {
        mix = (1U << FIXED_POINT_Q);
    }

    for (size_t n = 0; n < num_samples; n++) {
        int32_t input = (int32_t)in_buf[n] - X_AXIS;
        int32_t output;

        size_t wet_amount = mix;
        size_t dry_amount = (1U << FIXED_POINT_Q) - mix;

        // Saturate the input of tanh to avoid overflow
        output =
            (int32_t)level * q_tanh((int16_t)saturate_amplitude((int32_t)tone * input, INT16_MAX));
        output = (int32_t)gain * ((int32_t)wet_amount * output >> FIXED_POINT_Q) >> FIXED_POINT_Q;
        output += (int32_t)dry_amount * input;

        out_buf[n] = (int16_t)(output >> FIXED_POINT_Q) + X_AXIS;
    }
}

/* Buffered echo:
 *	- Echo with max delay of (MAX_DELAY_SAMPLES / Sample rate)
 *  - Note: num_samples is the size of out_buf
 *  - Note: in_buf is assumed to have delay_samples of previous samples, plus num_samples of current
 *samples (those to which to add echo)
 */
void buf_echo(const uint16_t *in_buf, uint16_t *out_buf, size_t num_samples,
              const EchoParam *param) {
    size_t delay_samples = param->delay_samples;
    size_t pre_delay = param->pre_delay;
    size_t density = param->density;
    size_t attack = param->attack;
    size_t decay = param->decay;

    if (delay_samples == 0) {
        memcpy(out_buf, in_buf, num_samples * sizeof(uint16_t));
        return;
    }

    if (pre_delay < MIN_ECHO_PRE_DELAY) {
        pre_delay = MIN_ECHO_PRE_DELAY;
    }

    if (density > MAX_ECHO_DENSITY) {
        density = MAX_ECHO_DENSITY;
    }

    if (attack > MAX_ECHO_ATTACK) {
        attack = MAX_ECHO_ATTACK;
    }

    if (decay > MAX_ECHO_DECAY) {
        decay = MAX_ECHO_DECAY;
    }

    if (num_samples == 0) {
        return;
    }

    // Start of current samples of in_buf
    const uint16_t *in_buf_curr = &in_buf[delay_samples];

    // Copy current input samples to output samples buffer
    memcpy(out_buf, in_buf_curr, num_samples * sizeof(uint16_t));

    if (density == 0 || pre_delay > delay_samples) {
        return;
    }

    size_t echo_spacing = (1U << FIXED_POINT_Q) / density;
    if (echo_spacing == 0) {
        echo_spacing = 1;
    }

    // For each sample of in_buf, add its echo to out_buf
    for (size_t n = 0; n < delay_samples + num_samples - 1; n++) {
        int32_t dry_input = (int32_t)in_buf[n] - X_AXIS;

        size_t curr_echo_gain = attack;

        // Add each echo to the appropriate sample of out_buf
        for (size_t delay = pre_delay; delay <= delay_samples; delay += echo_spacing) {
            // Make sure echo is within bounds of out_buf, ignore otherwise (still decay/reduce
            // gain)
            if (n + delay >= delay_samples && n + delay < delay_samples + num_samples) {
                const size_t out_idx = n + delay - delay_samples;
                int32_t wet = ((int32_t)curr_echo_gain * dry_input) >> FIXED_POINT_Q;
                int32_t mixed = (int32_t)out_buf[out_idx] + wet;
                out_buf[out_idx] = saturate_u16(mixed);
            }

            // Reduce echo gain by decay, saturating to 0
            curr_echo_gain = saturate_min((int32_t)curr_echo_gain - (int32_t)decay, 0);
        }
    }
}

/* Buffered compression:
 *	- Dynamic range compression
 *	- Reduces signal above threshold based on ratio
 * 	- Ratio is in QN
 */
void buf_compression(const uint16_t *in_buf, uint16_t *out_buf, size_t num_samples,
                     const CompressionParam *param) {
    int32_t threshold = (int32_t)param->threshold;
    int32_t ratio = (int32_t)param->ratio;

    if (ratio > (1 << FIXED_POINT_Q)) {
        ratio = (1 << FIXED_POINT_Q);
    }

    // Excess gets scaled by (1 - ratio) before being added back to threshold
    ratio = (1 << FIXED_POINT_Q) - ratio;

    for (size_t n = 0; n < num_samples; n++) {
        int32_t sample = (int32_t)in_buf[n] - X_AXIS;

        if (sample > threshold) {
            // Scale down excess above threshold using ratio
            uint32_t excess = (uint32_t)(sample - threshold);

            sample = (int32_t)((excess * (uint32_t)ratio >> FIXED_POINT_Q) + (uint32_t)threshold);
        } else if (sample < -threshold) {
            // Scale down excess above threshold using ratio (make sure both are at QN)
            uint32_t excess = (uint32_t)(-threshold - sample);

            sample = -threshold - (int32_t)((excess * (uint32_t)ratio) >> FIXED_POINT_Q);
        }

        out_buf[n] = (uint16_t)sample + X_AXIS;
    }
}

// Saturate num to +/- max
static int32_t saturate_amplitude(int32_t num, size_t max) {
    if (num > (int32_t)max)
        return (int32_t)max;
    else if (num < (int32_t)-max)
        return -(int32_t)max;
    else
        return num;
}

// Saturate num to min
static int32_t saturate_min(int32_t num, int32_t min) {
    if (num < min)
        return min;
    else
        return num;
}

static uint16_t saturate_u16(int32_t num) {
    if (num < 0) {
        return 0;
    }

    if (num > (int32_t)UINT16_MAX) {
        return UINT16_MAX;
    }

    return (uint16_t)num;
}
