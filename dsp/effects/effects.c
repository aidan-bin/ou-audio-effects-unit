#include "effects.h"
#include "fast_math.h"
#include "ring_buffer.h"

#include <string.h>

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
    size_t dry_amount = QN_ONE - mix;

    for (size_t n = 0; n < num_samples; n++)
    {
        int32_t input = sample_center(in_buf[n]);
        int32_t output;

        // Make sure q_tanh arg does not overflow for larger N
        int64_t drive = (int64_t)tone * input;
        output = (int32_t)level * q_tanh(saturate_i32(drive));
        output = (int32_t)gain * ((int32_t)mix * output >> FIXED_POINT_Q) >> FIXED_POINT_Q;
        output += (int32_t)dry_amount * input;

        out_buf[n] = (int16_t)(output >> FIXED_POINT_Q) + X_AXIS;
    }
}

static void mix_echo_tap(uint16_t *out_buf, size_t out_idx, int32_t src_sample, size_t tap_gain)
{
    int32_t wet = ((int32_t)tap_gain * src_sample) >> FIXED_POINT_Q;
    out_buf[out_idx] = saturate_u16((int32_t)out_buf[out_idx] + wet);
}

/* Buffered echo:
 * - Echo with maximum delay of (MAX_DELAY_SAMPLES / sample rate); equivalent to FIR.
 * - Uses ring buffer for multi-tap delay line.
 * - Useful for discrete early reflections, but cannot produce a long tail.
 * - out_buf[k] = dry sample at start_idx + delay_samples + k; each older dry sample
 *   then sprays decaying echo taps forward onto whichever out_buf indices they land on.
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
        out_buf[k] = ring_read(ring, ring_len, start_idx + delay_samples + k);
    }

    if (delay_samples == 0 || density == 0 || pre_delay > delay_samples)
    {
        return;
    }

    size_t echo_spacing = QN_ONE / density; // num samples between consecutive taps
    if (echo_spacing == 0)
    {
        echo_spacing = 1;
    }

    for (size_t src_idx = 0; src_idx < delay_samples + num_samples - 1; src_idx++)
    {
        int32_t src_sample = sample_center(ring_read(ring, ring_len, start_idx + src_idx));

        size_t tap_gain = attack;

        for (size_t tap_delay = pre_delay; tap_delay <= delay_samples; tap_delay += echo_spacing)
        {
            if (src_idx + tap_delay >= delay_samples && src_idx + tap_delay < delay_samples + num_samples)
            {
                mix_echo_tap(out_buf, src_idx + tap_delay - delay_samples, src_sample, tap_gain);
            }

            tap_gain = saturate_min((int32_t)tap_gain - (int32_t)decay, 0);
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
 * - fb->line recirculates: each write is a damped, attenuated copy of its own
 *   past output (from fb_delay samples ago) plus fresh dry input.
 * - out_buf[k] += that recirculating line content from fb_delay samples ago.
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
    int32_t lpf_coeff = (int32_t)(QN_ONE - damping);

    for (size_t n = 0; n < num_samples; n++)
    {
        size_t read_idx = ring_rewind(fb->write_idx, fb_delay, fb->line_len);
        int32_t delayed = sample_center(fb->line[read_idx]);
        int32_t dry = sample_center(in_buf[n]);

        int32_t fb_in = dry + (((int32_t)feedback * delayed) >> FIXED_POINT_Q);

        fb->lpf_state += (lpf_coeff * (fb_in - fb->lpf_state)) >> FIXED_POINT_Q;
        ring_write(fb->line, fb->line_len, fb->write_idx, sample_uncenter_saturate(fb->lpf_state));
        fb->write_idx = ring_advance(fb->write_idx, 1, fb->line_len);

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
        ring_fill(fb->line, fb->line_len, (uint16_t)X_AXIS);
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
        ring_fill(state->history, state->history_len, (uint16_t)X_AXIS);
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

    ratio = (int32_t)QN_ONE - ratio;

    for (size_t n = 0; n < num_samples; n++)
    {
        int32_t sample = sample_center(in_buf[n]);

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
