#include "test_vector_source.h"

#include <stddef.h>
#include <stdint.h>

#include "effects.h"

#define TEST_VECTOR_DEFAULT_SAMPLE_RATE_HZ 48000U
#define TEST_VECTOR_SWEEP_MIN_HZ 20U

#define Q16_ONE (1UL << 16)

#ifdef INCLUDE_WAV_LUT
#include "test_vector_wav.h"
#endif

static const int16_t sine_lut[] = {
    0,
    12539,
    23170,
    30273,
    32767,
    30273,
    23170,
    12539,
    0,
    -12539,
    -23170,
    -30273,
    -32767,
    -30273,
    -23170,
    -12539,
};

static const int16_t user_lut[] = {
    -32767,
    -20000,
    -5000,
    5000,
    20000,
    32767,
    20000,
    5000,
    -5000,
    -20000,
};

static uint16_t clamp_u16_from_i32(int32_t value)
{
    if (value < 0)
    {
        return 0U;
    }

    if (value > (int32_t)UINT16_MAX)
    {
        return UINT16_MAX;
    }

    return (uint16_t)value;
}

// Scales a signed sample by amplitude (relative to INT16_MAX) and centers it on
// the unsigned X_AXIS midpoint, saturating to the uint16_t range.
static uint16_t scale_sample(int32_t sample, uint16_t amplitude)
{
    int32_t scaled = (int32_t)((sample * (int32_t)amplitude) / INT16_MAX);
    return clamp_u16_from_i32((int32_t)X_AXIS + scaled);
}

// Q16 phase increment per output sample for a wavetable of lut_len entries
// played at freq_units cycles/second; never zero so playback always advances.
static uint32_t phase_step_q16(uint32_t freq_units, uint32_t lut_len, uint32_t sample_rate_hz)
{
    uint32_t step = (uint32_t)(((uint64_t)freq_units * (uint64_t)lut_len * Q16_ONE) /
                               (uint64_t)sample_rate_hz);
    return step == 0U ? 1U : step;
}

static void advance_sweep(TestVectorSource *source, size_t count)
{
    uint32_t sweep_max_hz = source->sample_rate_hz / 2U;
    uint32_t sweep_step = (uint32_t)((uint64_t)(sweep_max_hz - TEST_VECTOR_SWEEP_MIN_HZ) *
                                     (uint64_t)count / (uint64_t)source->sample_rate_hz);
    if (sweep_step == 0U)
    {
        sweep_step = 1U;
    }
    source->sweep_freq_hz += sweep_step;
    if (source->sweep_freq_hz > sweep_max_hz)
    {
        source->sweep_freq_hz = TEST_VECTOR_SWEEP_MIN_HZ;
        source->phase_q16 = 0U;
    }
}

void test_vector_source_init(TestVectorSource *source, uint32_t sample_rate_hz)
{
    if (source == NULL)
    {
        return;
    }

    source->sample_rate_hz = sample_rate_hz == 0U ? TEST_VECTOR_DEFAULT_SAMPLE_RATE_HZ : sample_rate_hz;
    source->phase_q16 = 0U;
    source->sweep_freq_hz = TEST_VECTOR_SWEEP_MIN_HZ;
    source->impulse_counter = 0U;
    source->impulse_fired = false;
    source->usb_underruns = 0U;
}

bool test_vector_source_fill_buffer(TestVectorSource *source, const CliTestModeStatus *status,
                                    uint16_t *buf, size_t count)
{
    if (source == NULL || status == NULL || buf == NULL)
    {
        return false;
    }

    if (!status->enabled)
    {
        return true;
    }

    if (status->vector == TEST_VECTOR_COUNTER)
    {
        /* Fill buf with a monotonically-incrementing counter so that any
         * dropped, duplicated, or wrong-buffer samples in the AS-IN
         * transport show up as gaps, repeats, or restarts on the host. */
        for (size_t i = 0; i < count; i++)
        {
            buf[i] = (uint16_t)(source->counter_state & 0xFFFFU);
            source->counter_state++;
        }
        return true;
    }

    if (status->vector == TEST_VECTOR_USB_STREAM)
    {
        if (source->stream_pop == NULL)
        {
            return false;
        }

        for (size_t i = 0; i < count; i++)
        {
            int16_t sample = 0;
            if (source->stream_pop(&sample, source->stream_context))
            {
                buf[i] = scale_sample(sample, status->amplitude);
            }
            else
            {
                source->usb_underruns++;
                buf[i] = (uint16_t)X_AXIS;
            }
        }

        return true;
    }

    if (status->vector == TEST_VECTOR_IMPULSE)
    {
        uint32_t freq_hz = (uint32_t)status->frequency_hz;
        for (size_t i = 0; i < count; i++)
        {
            bool do_impulse = false;
            if (freq_hz == 0U)
            {
                if (!source->impulse_fired)
                {
                    do_impulse = true;
                    source->impulse_fired = true;
                }
            }
            else
            {
                source->impulse_counter++;
                uint32_t period_samples = source->sample_rate_hz / freq_hz;
                if (period_samples == 0U)
                {
                    period_samples = 1U;
                }
                if (source->impulse_counter >= period_samples)
                {
                    do_impulse = true;
                    source->impulse_counter = 0U;
                }
            }

            if (do_impulse)
            {
                buf[i] = scale_sample(INT16_MAX, status->amplitude);
            }
            else
            {
                buf[i] = (uint16_t)X_AXIS;
            }
        }

        return true;
    }

    if (status->vector == TEST_VECTOR_WAV)
    {
#ifndef INCLUDE_WAV_LUT
        for (size_t i = 0; i < count; i++)
        {
            buf[i] = (uint16_t)X_AXIS;
        }
        return true;
#else
        uint32_t rate_mult = (uint32_t)status->frequency_hz;
        if (rate_mult == 0U)
        {
            rate_mult = TEST_VECTOR_WAV_DEFAULT_FREQ;
        }

        uint32_t phase_step =
            (uint32_t)(((uint64_t)WAV_LUT_SAMPLE_RATE * (uint64_t)WAV_LUT_LEN * Q16_ONE) /
                       (uint64_t)source->sample_rate_hz);
        if (rate_mult != TEST_VECTOR_WAV_DEFAULT_FREQ)
        {
            phase_step =
                (uint32_t)(((uint64_t)phase_step * (uint64_t)rate_mult) / TEST_VECTOR_WAV_DEFAULT_FREQ);
        }

        if (phase_step == 0U)
        {
            phase_step = 1U;
        }

        for (size_t i = 0; i < count; i++)
        {
            uint32_t idx = (source->phase_q16 >> 16) % (uint32_t)WAV_LUT_LEN;
            int32_t sample = wav_lut[idx];
            buf[i] = scale_sample(sample, status->amplitude);
            source->phase_q16 += phase_step;
        }

        return true;
#endif
    }

    const int16_t *lut = status->vector == TEST_VECTOR_USER ? user_lut : sine_lut;
    const size_t lut_size = status->vector == TEST_VECTOR_USER ? (sizeof(user_lut) / sizeof(user_lut[0]))
                                                               : (sizeof(sine_lut) / sizeof(sine_lut[0]));

    uint32_t freq_hz = status->vector == TEST_VECTOR_SWEEP ? source->sweep_freq_hz : (uint32_t)status->frequency_hz;

    uint32_t phase_step = phase_step_q16(freq_hz, (uint32_t)lut_size, source->sample_rate_hz);

    for (size_t i = 0; i < count; i++)
    {
        uint32_t index0 = (source->phase_q16 >> 16) % (uint32_t)lut_size;
        uint32_t index1 = (index0 + 1U) % (uint32_t)lut_size;
        uint32_t frac = source->phase_q16 & 0xFFFFU;
        int32_t a = lut[index0];
        int32_t b = lut[index1];
        int32_t sample = a + (int32_t)(((int64_t)(b - a) * (int64_t)frac) >> 16);
        buf[i] = scale_sample(sample, status->amplitude);
        source->phase_q16 += phase_step;
    }

    if (status->vector == TEST_VECTOR_SWEEP)
    {
        advance_sweep(source, count);
    }

    return true;
}
