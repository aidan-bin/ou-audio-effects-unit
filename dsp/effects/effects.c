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
 * - Echo with maximum delay of (MAX_DELAY_SAMPLES / sample rate).
 * - num_samples is the size of out_buf.
 * - in_buf is assumed to have delay_samples of previous samples, plus num_samples of current
 *   samples (those to which to add echo).
 */
void buf_echo(const uint16_t *in_buf, uint16_t *out_buf, size_t num_samples,
              const EchoParam *param)
{
    size_t delay_samples = param->delay_samples;
    size_t pre_delay = clamp_min(param->pre_delay, MIN_ECHO_PRE_DELAY);
    size_t density = clamp_qn(param->density);
    size_t attack = clamp_qn(param->attack);
    size_t decay = clamp_qn(param->decay);

    if (delay_samples == 0)
    {
        memcpy(out_buf, in_buf, num_samples * sizeof(uint16_t));
        return;
    }

    if (num_samples == 0)
    {
        return;
    }

    const uint16_t *in_buf_curr = &in_buf[delay_samples];

    memcpy(out_buf, in_buf_curr, num_samples * sizeof(uint16_t));

    if (density == 0 || pre_delay > delay_samples)
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
        int32_t dry_input = (int32_t)in_buf[n] - X_AXIS;

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
