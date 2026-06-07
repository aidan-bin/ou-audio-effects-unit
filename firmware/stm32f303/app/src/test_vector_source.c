#include "test_vector_source.h"

#include <stddef.h>
#include <stdint.h>

#include "effects.h"

#define TEST_VECTOR_DEFAULT_SAMPLE_RATE_HZ 40500U

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
    uint32_t phase_step =
        (uint32_t)(((uint64_t)status->frequencyHz * (uint64_t)lut_size * 65536ULL) /
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

    return true;
}
