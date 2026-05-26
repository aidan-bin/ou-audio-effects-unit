#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "effects.h"
#include "fast_math.h"

#define Q_ONE (1U << FIXED_POINT_Q)

static int failures = 0;

static void expect_eq_u16(uint16_t expected, uint16_t actual, const char* label)
{
    if (expected != actual)
    {
        fprintf(stderr, "FAIL: %s expected=%u actual=%u\n", label, expected, actual);
        failures++;
    }
}

static void expect_eq_i16(int16_t expected, int16_t actual, const char* label)
{
    if (expected != actual)
    {
        fprintf(stderr, "FAIL: %s expected=%d actual=%d\n", label, expected, actual);
        failures++;
    }
}

static void test_q_tanh_clamps(void)
{
    expect_eq_i16((int16_t)Q_ONE, q_tanh(10000), "q_tanh positive clamp");
    expect_eq_i16((int16_t)-Q_ONE, q_tanh(-10000), "q_tanh negative clamp");
    expect_eq_i16(0, q_tanh(0), "q_tanh zero");
}

static void test_overdrive_dry_passthrough(void)
{
    const uint16_t input[] = {
        (uint16_t)(X_AXIS - 1500),
        (uint16_t)X_AXIS,
        (uint16_t)(X_AXIS + 1500),
        (uint16_t)(X_AXIS + 4000),
    };
    uint16_t output[sizeof(input) / sizeof(input[0])] = {0};

    OverdriveParam param = {
        .level = MAX_OVERDRIVE_LEVEL,
        .gain = Q_ONE,
        .tone = Q_ONE,
        .mix = 0,
    };

    buf_overdrive(input, output, sizeof(input) / sizeof(input[0]), &param);

    for (size_t i = 0; i < sizeof(input) / sizeof(input[0]); i++)
    {
        expect_eq_u16(input[i], output[i], "overdrive dry passthrough");
    }
}

static void test_compression_ratio_behavior(void)
{
    const uint16_t input[] = {
        (uint16_t)(X_AXIS - 1000),
        (uint16_t)(X_AXIS - 200),
        (uint16_t)(X_AXIS + 200),
        (uint16_t)(X_AXIS + 1000),
    };

    uint16_t output_no_compression[4] = {0};
    CompressionParam no_compression = {
        .threshold = 300,
        .ratio = 0,
    };

    buf_compression(input, output_no_compression, 4, &no_compression);
    for (size_t i = 0; i < 4; i++)
    {
        expect_eq_u16(input[i], output_no_compression[i], "compression ratio=0 passthrough");
    }

    uint16_t output_hard_clip[4] = {0};
    CompressionParam hard_clip = {
        .threshold = 300,
        .ratio = Q_ONE,
    };

    buf_compression(input, output_hard_clip, 4, &hard_clip);
    expect_eq_u16((uint16_t)(X_AXIS - 300), output_hard_clip[0], "compression hard clip low");
    expect_eq_u16(input[1], output_hard_clip[1], "compression hard clip low in-threshold");
    expect_eq_u16(input[2], output_hard_clip[2], "compression hard clip high in-threshold");
    expect_eq_u16((uint16_t)(X_AXIS + 300), output_hard_clip[3], "compression hard clip high");
}

static void test_echo_attack_zero_is_copy(void)
{
    const EchoParam param = {
        .delay_samples = 2,
        .pre_delay = 1,
        .density = Q_ONE,
        .attack = 0,
        .decay = Q_ONE,
    };

    const uint16_t input_with_delay[] = {
        (uint16_t)(X_AXIS + 11),
        (uint16_t)(X_AXIS + 22),
        (uint16_t)(X_AXIS + 33),
        (uint16_t)(X_AXIS + 44),
        (uint16_t)(X_AXIS + 55),
        (uint16_t)(X_AXIS + 66),
    };

    const uint16_t expected[] = {
        input_with_delay[2],
        input_with_delay[3],
        input_with_delay[4],
        input_with_delay[5],
    };

    uint16_t output[4] = {0};
    buf_echo(input_with_delay, output, 4, &param);

    for (size_t i = 0; i < 4; i++)
    {
        expect_eq_u16(expected[i], output[i], "echo attack=0 copy");
    }
}

static void test_echo_zero_density_is_copy(void)
{
    const EchoParam param = {
        .delay_samples = 2,
        .pre_delay = 1,
        .density = 0,
        .attack = Q_ONE,
        .decay = 0,
    };

    const uint16_t input_with_delay[] = {
        (uint16_t)(X_AXIS + 10),
        (uint16_t)(X_AXIS + 20),
        (uint16_t)(X_AXIS + 30),
        (uint16_t)(X_AXIS + 40),
        (uint16_t)(X_AXIS + 50),
        (uint16_t)(X_AXIS + 60),
    };

    const uint16_t expected[] = {
        input_with_delay[2],
        input_with_delay[3],
        input_with_delay[4],
        input_with_delay[5],
    };

    uint16_t output[4] = {0};
    buf_echo(input_with_delay, output, 4, &param);

    for (size_t i = 0; i < 4; i++)
    {
        expect_eq_u16(expected[i], output[i], "echo density=0 copy");
    }
}

static void test_echo_single_step_full_coverage(void)
{
    const EchoParam param = {
        .delay_samples = 1,
        .pre_delay = 1,
        .density = Q_ONE,
        .attack = Q_ONE,
        .decay = Q_ONE,
    };

    const uint16_t input_with_delay[] = {
        (uint16_t)(X_AXIS + 1),
        (uint16_t)(X_AXIS + 2),
        (uint16_t)(X_AXIS + 3),
        (uint16_t)(X_AXIS + 4),
        (uint16_t)(X_AXIS + 5),
    };

    const uint16_t expected[] = {
        (uint16_t)(X_AXIS + 3),
        (uint16_t)(X_AXIS + 5),
        (uint16_t)(X_AXIS + 7),
        (uint16_t)(X_AXIS + 9),
    };

    uint16_t output[4] = {0};
    buf_echo(input_with_delay, output, 4, &param);

    for (size_t i = 0; i < 4; i++)
    {
        expect_eq_u16(expected[i], output[i], "echo one-step full coverage");
    }
}

int main(void)
{
    test_q_tanh_clamps();
    test_overdrive_dry_passthrough();
    test_compression_ratio_behavior();
    test_echo_attack_zero_is_copy();
    test_echo_zero_density_is_copy();
    test_echo_single_step_full_coverage();

    if (failures != 0)
    {
        fprintf(stderr, "DSP tests failed: %d\n", failures);
        return 1;
    }

    puts("DSP tests passed");
    return 0;
}
