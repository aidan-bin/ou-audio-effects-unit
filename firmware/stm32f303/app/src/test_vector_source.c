#include "test_vector_source.h"

#include <stddef.h>
#include <stdint.h>

#include "effects.h"

#define TEST_VECTOR_DEFAULT_SAMPLE_RATE_HZ 40500U
#define TEST_VECTOR_SWEEP_MIN_HZ 20U
#define TEST_VECTOR_SWEEP_MAX_HZ 8000U

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

    const int16_t *lut = status->vector == 1U ? user_lut : sine_lut;
    const size_t lut_size = status->vector == 1U ? (sizeof(user_lut) / sizeof(user_lut[0]))
                                                 : (sizeof(sine_lut) / sizeof(sine_lut[0]));

    uint32_t sample_rate_hz = source->sample_rate_hz == 0U ? TEST_VECTOR_DEFAULT_SAMPLE_RATE_HZ
                                                           : source->sample_rate_hz;

    uint32_t freq_hz = status->vector == 2U ? source->sweep_freq_hz : (uint32_t)status->frequencyHz;

    uint32_t phase_step =
        (uint32_t)(((uint64_t)freq_hz * (uint64_t)lut_size * 65536ULL) /
                   (uint64_t)sample_rate_hz);
    if (phase_step == 0U)
    {
        phase_step = 1U;
    }

    for (size_t i = 0; i < count; i++)
    {
        uint32_t lut_index = (source->phase_q16 >> 16) % (uint32_t)lut_size;
        int32_t scaled = (int32_t)(((int32_t)lut[lut_index] * (int32_t)status->amplitude) / 32767);
        buf[i] = clamp_u16_from_i32((int32_t)X_AXIS + scaled);
        source->phase_q16 += phase_step;
    }

    if (status->vector == 2U)
    {
        uint32_t sweep_step = (uint32_t)((uint64_t)(TEST_VECTOR_SWEEP_MAX_HZ - TEST_VECTOR_SWEEP_MIN_HZ) *
                                         (uint64_t)count / (uint64_t)sample_rate_hz);
        if (sweep_step == 0U)
        {
            sweep_step = 1U;
        }
        source->sweep_freq_hz += sweep_step;
        if (source->sweep_freq_hz > TEST_VECTOR_SWEEP_MAX_HZ)
        {
            source->sweep_freq_hz = TEST_VECTOR_SWEEP_MIN_HZ;
            source->phase_q16 = 0U;
        }
    }

    return true;
}
