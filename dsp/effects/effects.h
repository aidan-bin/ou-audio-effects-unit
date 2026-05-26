/*
 *  Note: Input/output data convention is uint16_t with UIN16_MAX / 2 representing the x-axis
 *  Note: If no MIN_ definition is specified for a parameter, assume it is 0
 *	Note: For memoryless effects, the input buffer can be the output buffer
 */

#ifndef EFFECTS_H
#define EFFECTS_H

#include <stdint.h>
#include <stdlib.h>

/* Data convention definitions */
#define X_AXIS (UINT16_MAX / 2)     // x-axis is the median representable signal

#ifndef FIXED_POINT_Q
#define FIXED_POINT_Q 8   // N in QN for fixed-point numbers
#endif

/* Effects parameters */
typedef struct
{
    size_t level;   // Saturation amplitude of wet signal
    size_t gain;    // Output gain in QN, fraction of level (note: level is for consistency, keep gain at 1.0 and use level to control output)
    size_t tone;    // Tone in QN (lower tone softens saturation effect)
    size_t mix;     // Wet/dry ratio in QN (0.0 = fully dry, 0.5 = equally wet and dry, 1.0 = fully wet)
} OverdriveParam;

#define MAX_OVERDRIVE_GAIN (1U << FIXED_POINT_Q)  // Max gain is 1.0
#define MAX_OVERDRIVE_LEVEL INT16_MAX             // Cannot exceed INT16_MAX
#define MIN_OVERDRIVE_TONE (1U << FIXED_POINT_Q)  // Min tone is 1.0 (below this inverts the signal)
#define MAX_OVERDRIVE_TONE (5U << FIXED_POINT_Q)  // Note: 5.0 is somewhat arbitrary (should be calibrated as desired)
#define MAX_OVERDRIVE_MIX (1U << FIXED_POINT_Q)   // Max mix is 1.0 (equivalent to fully wet)

typedef struct
{
    size_t delay_samples;   // Number of samples of delay for last echo of an input sample (determines max pre-delay and duration of echo)
    size_t pre_delay;       // Number of samples before first echo
    size_t density;         // Discrete, evenly-spaced echoes per sample in QN (after pre-delay, before end of delay samples)
    size_t attack;          // Gain on first echo in QN (subsequent echoes have lower gain)
    size_t decay;           // Amount of gain reduction per subsequent echo in QN (will saturate to 0 gain)
} EchoParam;

#define MAX_ECHO_DELAY_SAMPLES 2207								// Note: At 40.54 kHz, 50 ms is 2027 samples
#define MIN_ECHO_PRE_DELAY 1U											// Min delay before first echo is one unit
#define MAX_ECHO_PRE_DELAY MAX_ECHO_DELAY_SAMPLES	// Max delay before first echo is until end of delay samples (equivalent to a single echo)
#define MAX_ECHO_DENSITY (1U << FIXED_POINT_Q)		// Cannot exceed 1 echo per sample
#define MAX_ECHO_ATTACK (1U << FIXED_POINT_Q)			// Max gain is 1.0 (equivalent to dry signal)
#define MAX_ECHO_DECAY (1U << FIXED_POINT_Q)			// Max gain reduction is 1.0 (equivalent to a single echo)

typedef struct
{
    size_t threshold;	// Amplitude above which to apply gain reduction (NOT uint16_t value, refer to data convention)
    size_t ratio; 		// Amount of gain reduction to apply in QN (0 for no compression, 1 for hard clipping)
} CompressionParam;

#define MAX_COMPRESSION_THRESHOLD UINT16_MAX        // Cannot exceed UINT16_MAX
#define MAX_COMPRESSION_RATIO (1U << FIXED_POINT_Q)	// Max gain reduction is 1.0 (equivalent to clipping)

/* Effects function definitions */
void buf_overdrive(const uint16_t* in_buf, uint16_t* out_buf, size_t num_samples, const OverdriveParam* param);
void buf_echo(const uint16_t* in_buf, uint16_t* out_buf, size_t num_samples, const EchoParam* param);
void buf_compression(const uint16_t* in_buf, uint16_t* out_buf, size_t num_samples, const CompressionParam* param);

/* Moving Averager (previously used for testing)
#define NUM_FIR_TAPS 4				// Number of taps used for FIR filters
#define FIR_AVERAGER_SHIFT 2	// Shift used for FIR averager coefficients (should be log base 2 NUM_FIR_TAPS)

void buf_averager(const uint16_t* in_buf, uint16_t* out_buf, const uint16_t* edge_samples_buf, size_t num_taps, size_t num_copies, size_t shift);
//void buf_gain_stage();
//void buf_fir_high_pass();
*/
#endif
