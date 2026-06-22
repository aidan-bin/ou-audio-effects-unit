#include "test_vector_source.h"

#include <stddef.h>
#include <stdint.h>

#include "effects.h"

#define TEST_VECTOR_DEFAULT_SAMPLE_RATE_HZ 40500U
#define TEST_VECTOR_SWEEP_MIN_HZ 20U

#define Q16_ONE (1UL << 16)

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

    const int16_t *lut = status->vector == TEST_VECTOR_USER ? user_lut : sine_lut;
    const size_t lut_size = status->vector == TEST_VECTOR_USER ? (sizeof(user_lut) / sizeof(user_lut[0]))
                                                               : (sizeof(sine_lut) / sizeof(sine_lut[0]));

    uint32_t freq_hz = status->vector == TEST_VECTOR_SWEEP ? source->sweep_freq_hz : (uint32_t)status->frequency_hz;

    uint32_t phase_step =
        (uint32_t)(((uint64_t)freq_hz * (uint64_t)lut_size * Q16_ONE) /
                   (uint64_t)source->sample_rate_hz);
    if (phase_step == 0U)
    {
        phase_step = 1U;
    }

    for (size_t i = 0; i < count; i++)
    {
        uint32_t index0 = (source->phase_q16 >> 16) % (uint32_t)lut_size;
        uint32_t index1 = (index0 + 1U) % (uint32_t)lut_size;
        uint32_t frac = source->phase_q16 & 0xFFFFU;
        int32_t a = lut[index0];
        int32_t b = lut[index1];
        int32_t sample = a + (int32_t)(((int64_t)(b - a) * (int64_t)frac) >> 16);
        int32_t scaled = (int32_t)((sample * (int32_t)status->amplitude) / INT16_MAX);
        buf[i] = clamp_u16_from_i32((int32_t)X_AXIS + scaled);
        source->phase_q16 += phase_step;
    }

    if (status->vector == TEST_VECTOR_SWEEP)
    {
        advance_sweep(source, count);
    }

    return true;
}
