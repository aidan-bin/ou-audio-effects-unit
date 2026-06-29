#include "effects.h"
#include "fast_math.h"

#include <string.h>

static int32_t saturate_amplitude(int32_t num, size_t max);
static int32_t saturate_min(int32_t num, int32_t min);
static uint16_t saturate_u16(int32_t num);

/* Buffered overdrive/distortion:
 * - Distortion/overdrive clips input signal to a threshold, which makes it resemble a square
 *   wave.
 * - Uses tanh on the input.
 */
void buf_overdrive(const uint16_t *in_buf, uint16_t *out_buf, size_t num_samples,
                   const OverdriveParam *param)
{
    size_t level = clamp_range(param->level, 0, MAX_OVERDRIVE_LEVEL);
    size_t gain = clamp_qn(param->gain);
    size_t tone = clamp_range(param->tone, MIN_OVERDRIVE_TONE, MAX_OVERDRIVE_TONE);
    size_t mix = clamp_qn(param->mix);
    size_t dry_amount = (1U << FIXED_POINT_Q) - mix;

    for (size_t n = 0; n < num_samples; n++)
    {
        int32_t input = (int32_t)in_buf[n] - X_AXIS;
        int32_t output;

        output =
            (int32_t)level * q_tanh((int16_t)saturate_amplitude((int32_t)tone * input, INT16_MAX));
        output = (int32_t)gain * ((int32_t)mix * output >> FIXED_POINT_Q) >> FIXED_POINT_Q;
        output += (int32_t)dry_amount * input;

        out_buf[n] = (int16_t)(output >> FIXED_POINT_Q) + X_AXIS;
    }
}

/* Buffered echo:
 * - Echo with maximum delay of (MAX_DELAY_SAMPLES / sample rate); equivalent to FIR.
 * - Uses ring buffer for multi-tap delay line.
 * - Useful for discrete early reflections, but cannot produce a long tail.
 */
void buf_echo_ring(const uint16_t *ring, size_t ring_len, size_t start_idx, uint16_t *out_buf,
                   size_t num_samples, const EchoParam *param)
{
    if (ring == NULL || out_buf == NULL || param == NULL || ring_len == 0 || num_samples == 0)
    {
        return;
    }

    size_t delay_samples = param->delay_samples;
    size_t pre_delay = clamp_min(param->pre_delay, MIN_ECHO_PRE_DELAY);
    size_t density = clamp_qn(param->density);
    size_t attack = clamp_qn(param->attack);
    size_t decay = clamp_qn(param->decay);

    for (size_t k = 0; k < num_samples; k++)
    {
        out_buf[k] = ring[(start_idx + delay_samples + k) % ring_len];
    }

    if (delay_samples == 0 || density == 0 || pre_delay > delay_samples)
    {
        return;
    }

    size_t echo_spacing = (1U << FIXED_POINT_Q) / density;
    if (echo_spacing == 0)
    {
        echo_spacing = 1;
    }

    for (size_t n = 0; n < delay_samples + num_samples - 1; n++)
    {
        int32_t dry_input = (int32_t)ring[(start_idx + n) % ring_len] - X_AXIS;

        size_t curr_echo_gain = attack;

        for (size_t delay = pre_delay; delay <= delay_samples; delay += echo_spacing)
        {
            if (n + delay >= delay_samples && n + delay < delay_samples + num_samples)
            {
                const size_t out_idx = n + delay - delay_samples;
                int32_t wet = ((int32_t)curr_echo_gain * dry_input) >> FIXED_POINT_Q;
                int32_t mixed = (int32_t)out_buf[out_idx] + wet;
                out_buf[out_idx] = saturate_u16(mixed);
            }

            curr_echo_gain = saturate_min((int32_t)curr_echo_gain - (int32_t)decay, 0);
        }
    }
}

/* Wrapper for buf_echo_ring with contiguous input buffer (special case where no modular indexing is used) */
void buf_echo(const uint16_t *in_buf, uint16_t *out_buf, size_t num_samples,
              const EchoParam *param)
{
    if (in_buf == NULL || out_buf == NULL || param == NULL || num_samples == 0)
    {
        return;
    }

    buf_echo_ring(in_buf, param->delay_samples + num_samples, 0, out_buf, num_samples, param);
}

/* Buffered echo with feedback:
 * - Echo with feedback path (recirculating reflections); equivalent to comb IIR.
 * - Uses one-pole low-pass filter in feedback path for damping.
 * - Useful for long tails.
 */
void buf_echo_feedback(EchoFeedback *fb, const uint16_t *in_buf, uint16_t *out_buf,
                       size_t num_samples, const EchoParam *param)
{
    if (fb == NULL || fb->line == NULL || fb->line_len == 0 || in_buf == NULL ||
        out_buf == NULL || param == NULL || num_samples == 0)
    {
        return;
    }

    size_t feedback = clamp_range(param->feedback, 0, MAX_ECHO_FEEDBACK);
    if (feedback == 0)
    {
        return;
    }

    size_t damping = clamp_range(param->damping, 0, MAX_ECHO_DAMPING);
    size_t fb_delay = clamp_range(param->feedback_delay, 1, fb->line_len);
    int32_t lpf_coeff = (int32_t)((1U << FIXED_POINT_Q) - damping);

    for (size_t n = 0; n < num_samples; n++)
    {
        size_t read_idx = (fb->write_idx + fb->line_len - fb_delay) % fb->line_len;
        int32_t delayed = (int32_t)fb->line[read_idx] - X_AXIS;
        int32_t dry = (int32_t)in_buf[n] - X_AXIS;

        int32_t fb_in = dry + (((int32_t)feedback * delayed) >> FIXED_POINT_Q);

        fb->lpf_state += (lpf_coeff * (fb_in - fb->lpf_state)) >> FIXED_POINT_Q;
        fb->line[fb->write_idx] = saturate_u16(fb->lpf_state + X_AXIS);
        fb->write_idx = (fb->write_idx + 1U) % fb->line_len;

        out_buf[n] = saturate_u16((int32_t)out_buf[n] + delayed);
    }
}

void echo_feedback_reset(EchoFeedback *fb)
{
    if (fb == NULL)
    {
        return;
    }

    if (fb->line != NULL)
    {
        for (size_t i = 0; i < fb->line_len; i++)
        {
            fb->line[i] = (uint16_t)X_AXIS;
        }
    }
    fb->write_idx = 0;
    fb->lpf_state = 0;
}

void echo_state_reset(EchoState *state)
{
    if (state == NULL)
    {
        return;
    }

    if (state->history != NULL)
    {
        for (size_t i = 0; i < state->history_len; i++)
        {
            state->history[i] = (uint16_t)X_AXIS;
        }
    }
    state->history_w = 0;
    echo_feedback_reset(&state->fb);
}

/* Buffered compression:
 * - Dynamic range compression.
 * - Reduces signal above threshold based on ratio.
 * - Ratio is in QN.
 */
void buf_compression(const uint16_t *in_buf, uint16_t *out_buf, size_t num_samples,
                     const CompressionParam *param)
{
    int32_t threshold = (int32_t)clamp_range(param->threshold, 0, X_AXIS);
    int32_t ratio = (int32_t)clamp_qn(param->ratio);

    ratio = (1 << FIXED_POINT_Q) - ratio;

    for (size_t n = 0; n < num_samples; n++)
    {
        int32_t sample = (int32_t)in_buf[n] - X_AXIS;

        if (sample > threshold)
        {
            uint32_t excess = (uint32_t)(sample - threshold);

            sample = (int32_t)((excess * (uint32_t)ratio >> FIXED_POINT_Q) + (uint32_t)threshold);
        }
        else if (sample < -threshold)
        {
            uint32_t excess = (uint32_t)(-threshold - sample);

            sample = -threshold - (int32_t)((excess * (uint32_t)ratio) >> FIXED_POINT_Q);
        }

        out_buf[n] = (uint16_t)sample + X_AXIS;
    }
}

static int32_t saturate_amplitude(int32_t num, size_t max)
{
    if (num > (int32_t)max)
        return (int32_t)max;
    else if (num < (int32_t)-max)
        return -(int32_t)max;
    else
        return num;
}

static int32_t saturate_min(int32_t num, int32_t min)
{
    if (num < min)
        return min;
    else
        return num;
}

static uint16_t saturate_u16(int32_t num)
{
    if (num < 0)
    {
        return 0;
    }

    if (num > (int32_t)UINT16_MAX)
    {
        return UINT16_MAX;
    }

    return (uint16_t)num;
}
