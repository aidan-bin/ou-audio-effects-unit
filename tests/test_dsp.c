#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "effects.h"
#include "effect_runtime.h"
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

static void expect_eq_u16_array(const uint16_t* expected, const uint16_t* actual, size_t count,
    const char* label)
{
    for (size_t i = 0; i < count; i++)
    {
        if (expected[i] != actual[i])
        {
            fprintf(stderr, "FAIL: %s[%zu] expected=%u actual=%u\n", label, i, expected[i], actual[i]);
            failures++;
        }
    }
}

static void test_q_tanh_clamps(void)
{
    expect_eq_i16((int16_t)Q_ONE, q_tanh(10000), "q_tanh positive clamp");
    expect_eq_i16((int16_t)-Q_ONE, q_tanh(-10000), "q_tanh negative clamp");
    expect_eq_i16(0, q_tanh(0), "q_tanh zero");
}

static void test_q_tanh_odd_symmetry(void)
{
    for (int16_t x = 1; x <= (5 * Q_ONE); x += 13)
    {
        int16_t positive = q_tanh(x);
        int16_t negative = q_tanh((int16_t)-x);

        if (negative != (int16_t)-positive)
        {
            fprintf(stderr, "FAIL: q_tanh odd symmetry x=%d tanh(x)=%d tanh(-x)=%d\n", x,
                positive, negative);
            failures++;
        }
    }
}

static void test_q_tanh_is_bounded(void)
{
    for (int16_t x = (int16_t)(-8 * Q_ONE); x <= (int16_t)(8 * Q_ONE); x += 11)
    {
        int16_t y = q_tanh(x);
        if (y < (int16_t)-Q_ONE || y > (int16_t)Q_ONE)
        {
            fprintf(stderr, "FAIL: q_tanh boundedness x=%d y=%d\n", x, y);
            failures++;
        }
    }
}

static void test_q_tanh_monotonic(void)
{
    int16_t prev = q_tanh((int16_t)(-8 * Q_ONE));

    for (int16_t x = (int16_t)(-8 * Q_ONE + 1); x <= (int16_t)(8 * Q_ONE); x++)
    {
        int16_t current = q_tanh(x);
        if (current + 1 < prev)
        {
            fprintf(stderr, "FAIL: q_tanh monotonic x=%d prev=%d current=%d\n", x, prev, current);
            failures++;
            return;
        }

        prev = current;
    }
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

    expect_eq_u16_array(input, output, sizeof(input) / sizeof(input[0]), "overdrive dry passthrough");
}

static void test_overdrive_zero_level_mutes_wet(void)
{
    const uint16_t input[] = {
        (uint16_t)(X_AXIS - 1200),
        (uint16_t)(X_AXIS + 1200),
    };
    uint16_t output[2] = {0};

    OverdriveParam param = {
        .level = 0,
        .gain = Q_ONE,
        .tone = Q_ONE,
        .mix = Q_ONE,
    };

    buf_overdrive(input, output, 2, &param);
    expect_eq_u16((uint16_t)X_AXIS, output[0], "overdrive zero level mute 0");
    expect_eq_u16((uint16_t)X_AXIS, output[1], "overdrive zero level mute 1");
}

static void test_overdrive_full_wet_changes_signal(void)
{
    const uint16_t input[] = {
        (uint16_t)(X_AXIS - 1500),
        (uint16_t)(X_AXIS + 1500),
    };
    uint16_t output[2] = {0};

    OverdriveParam param = {
        .level = MAX_OVERDRIVE_LEVEL,
        .gain = Q_ONE,
        .tone = Q_ONE,
        .mix = Q_ONE,
    };

    buf_overdrive(input, output, 2, &param);

    if (output[0] == input[0] || output[1] == input[1])
    {
        fprintf(stderr, "FAIL: overdrive full wet should alter signal\n");
        failures++;
    }
}

static void test_overdrive_mix_above_one_clamps_to_wet(void)
{
    const uint16_t input[] = {
        (uint16_t)(X_AXIS - 900),
        (uint16_t)(X_AXIS + 900),
    };

    uint16_t output_clamped[2] = {0};
    OverdriveParam clamped_param = {
        .level = MAX_OVERDRIVE_LEVEL,
        .gain = Q_ONE,
        .tone = Q_ONE,
        .mix = Q_ONE,
    };

    uint16_t output_over[2] = {0};
    OverdriveParam over_param = {
        .level = MAX_OVERDRIVE_LEVEL,
        .gain = Q_ONE,
        .tone = Q_ONE,
        .mix = Q_ONE + 64,
    };

    buf_overdrive(input, output_clamped, 2, &clamped_param);
    buf_overdrive(input, output_over, 2, &over_param);

    expect_eq_u16_array(output_clamped, output_over, 2, "overdrive mix>1 clamps");
}

static void test_overdrive_gain_above_one_clamps(void)
{
    const uint16_t input[] = {
        (uint16_t)(X_AXIS - 1100),
        (uint16_t)(X_AXIS + 1100),
    };

    uint16_t output_ref[2] = {0};
    OverdriveParam ref_param = {
        .level = MAX_OVERDRIVE_LEVEL,
        .gain = Q_ONE,
        .tone = Q_ONE,
        .mix = Q_ONE,
    };

    uint16_t output_over[2] = {0};
    OverdriveParam over_param = {
        .level = MAX_OVERDRIVE_LEVEL,
        .gain = Q_ONE + 64,
        .tone = Q_ONE,
        .mix = Q_ONE,
    };

    buf_overdrive(input, output_ref, 2, &ref_param);
    buf_overdrive(input, output_over, 2, &over_param);

    expect_eq_u16_array(output_ref, output_over, 2, "overdrive gain>1 clamps");
}

static void test_overdrive_tone_below_one_clamps(void)
{
    const uint16_t input[] = {
        (uint16_t)(X_AXIS - 1300),
        (uint16_t)(X_AXIS + 1300),
    };

    uint16_t output_ref[2] = {0};
    OverdriveParam ref_param = {
        .level = MAX_OVERDRIVE_LEVEL,
        .gain = Q_ONE,
        .tone = Q_ONE,
        .mix = Q_ONE,
    };

    uint16_t output_low_tone[2] = {0};
    OverdriveParam low_tone_param = {
        .level = MAX_OVERDRIVE_LEVEL,
        .gain = Q_ONE,
        .tone = 0,
        .mix = Q_ONE,
    };

    buf_overdrive(input, output_ref, 2, &ref_param);
    buf_overdrive(input, output_low_tone, 2, &low_tone_param);

    expect_eq_u16_array(output_ref, output_low_tone, 2, "overdrive tone<1 clamps");
}

static void test_overdrive_level_above_max_clamps(void)
{
    const uint16_t input[] = {
        (uint16_t)(X_AXIS - 1300),
        (uint16_t)(X_AXIS + 1300),
    };

    uint16_t output_ref[2] = {0};
    OverdriveParam ref_param = {
        .level = MAX_OVERDRIVE_LEVEL,
        .gain = Q_ONE,
        .tone = Q_ONE,
        .mix = Q_ONE,
    };

    uint16_t output_over_level[2] = {0};
    OverdriveParam over_level_param = {
        .level = (size_t)MAX_OVERDRIVE_LEVEL + 1,
        .gain = Q_ONE,
        .tone = Q_ONE,
        .mix = Q_ONE,
    };

    buf_overdrive(input, output_ref, 2, &ref_param);
    buf_overdrive(input, output_over_level, 2, &over_level_param);

    expect_eq_u16_array(output_ref, output_over_level, 2, "overdrive level>max clamps");
}

static void test_overdrive_tone_above_max_clamps(void)
{
    const uint16_t input[] = {
        (uint16_t)(X_AXIS - 1300),
        (uint16_t)(X_AXIS + 1300),
    };

    uint16_t output_ref[2] = {0};
    OverdriveParam ref_param = {
        .level = MAX_OVERDRIVE_LEVEL,
        .gain = Q_ONE,
        .tone = MAX_OVERDRIVE_TONE,
        .mix = Q_ONE,
    };

    uint16_t output_over_tone[2] = {0};
    OverdriveParam over_tone_param = {
        .level = MAX_OVERDRIVE_LEVEL,
        .gain = Q_ONE,
        .tone = MAX_OVERDRIVE_TONE + Q_ONE,
        .mix = Q_ONE,
    };

    buf_overdrive(input, output_ref, 2, &ref_param);
    buf_overdrive(input, output_over_tone, 2, &over_tone_param);

    expect_eq_u16_array(output_ref, output_over_tone, 2, "overdrive tone>max clamps");
}

static void test_overdrive_sign_symmetry(void)
{
    const uint16_t input[] = {
        (uint16_t)(X_AXIS - 2000),
        (uint16_t)(X_AXIS + 2000),
        (uint16_t)(X_AXIS - 700),
        (uint16_t)(X_AXIS + 700),
    };

    uint16_t output[4] = {0};
    OverdriveParam param = {
        .level = MAX_OVERDRIVE_LEVEL,
        .gain = Q_ONE,
        .tone = Q_ONE,
        .mix = Q_ONE,
    };

    buf_overdrive(input, output, 4, &param);

    for (size_t i = 0; i < 4; i += 2)
    {
        uint32_t mirrored_sum = (uint32_t)output[i] + (uint32_t)output[i + 1];
        uint32_t centered_sum = (uint32_t)(2U * X_AXIS);
        uint32_t diff = (mirrored_sum > centered_sum) ? (mirrored_sum - centered_sum)
                                                      : (centered_sum - mirrored_sum);

        if (diff > 1U)
        {
            fprintf(stderr, "FAIL: overdrive symmetry pair=%zu sum=%u expected=%u\n", i / 2,
                mirrored_sum, (unsigned)centered_sum);
            failures++;
        }
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
    expect_eq_u16_array(input, output_no_compression, 4, "compression ratio=0 passthrough");

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

static void test_compression_zero_threshold_hard_clip(void)
{
    const uint16_t input[] = {
        (uint16_t)(X_AXIS - 1000),
        (uint16_t)X_AXIS,
        (uint16_t)(X_AXIS + 1000),
    };

    uint16_t output[3] = {0};
    CompressionParam param = {
        .threshold = 0,
        .ratio = Q_ONE,
    };

    buf_compression(input, output, 3, &param);

    expect_eq_u16((uint16_t)X_AXIS, output[0], "compression zero-threshold clip low");
    expect_eq_u16((uint16_t)X_AXIS, output[1], "compression zero-threshold clip center");
    expect_eq_u16((uint16_t)X_AXIS, output[2], "compression zero-threshold clip high");
}

static void test_compression_ratio_above_one_clamps_to_hard_clip(void)
{
    const uint16_t input[] = {
        (uint16_t)(X_AXIS - 1000),
        (uint16_t)(X_AXIS + 1000),
    };

    uint16_t output_clip[2] = {0};
    CompressionParam hard_clip_param = {
        .threshold = 300,
        .ratio = Q_ONE,
    };

    uint16_t output_over[2] = {0};
    CompressionParam over_ratio_param = {
        .threshold = 300,
        .ratio = Q_ONE + 64,
    };

    buf_compression(input, output_clip, 2, &hard_clip_param);
    buf_compression(input, output_over, 2, &over_ratio_param);

    expect_eq_u16_array(output_clip, output_over, 2, "compression ratio>1 clamps");
}

static void test_compression_threshold_above_axis_clamps(void)
{
    const uint16_t input[] = {
        UINT16_MAX,
    };

    uint16_t output_ref[1] = {0};
    CompressionParam ref_param = {
        .threshold = X_AXIS,
        .ratio = Q_ONE,
    };

    uint16_t output_over[1] = {0};
    CompressionParam over_threshold_param = {
        .threshold = X_AXIS + 1000,
        .ratio = Q_ONE,
    };

    buf_compression(input, output_ref, 1, &ref_param);
    buf_compression(input, output_over, 1, &over_threshold_param);

    expect_eq_u16(output_ref[0], output_over[0], "compression threshold>axis clamps");
}

static void test_compression_threshold_above_axis_clamps_negative_side(void)
{
    const uint16_t input[] = {
        0,
    };

    uint16_t output_ref[1] = {0};
    CompressionParam ref_param = {
        .threshold = X_AXIS,
        .ratio = Q_ONE,
    };

    uint16_t output_over[1] = {0};
    CompressionParam over_threshold_param = {
        .threshold = X_AXIS + 1000,
        .ratio = Q_ONE,
    };

    buf_compression(input, output_ref, 1, &ref_param);
    buf_compression(input, output_over, 1, &over_threshold_param);

    expect_eq_u16(output_ref[0], output_over[0], "compression threshold>axis clamps negative side");
}

static void test_compression_sign_symmetry(void)
{
    const uint16_t input[] = {
        (uint16_t)(X_AXIS - 1000),
        (uint16_t)(X_AXIS + 1000),
        (uint16_t)(X_AXIS - 200),
        (uint16_t)(X_AXIS + 200),
    };

    uint16_t output[4] = {0};
    CompressionParam param = {
        .threshold = 300,
        .ratio = Q_ONE / 2,
    };

    buf_compression(input, output, 4, &param);

    for (size_t i = 0; i < 4; i += 2)
    {
        uint32_t mirrored_sum = (uint32_t)output[i] + (uint32_t)output[i + 1];
        if (mirrored_sum != (uint32_t)(2U * X_AXIS))
        {
            fprintf(stderr, "FAIL: compression symmetry pair=%zu sum=%u expected=%u\n", i / 2,
                mirrored_sum, (unsigned)(2U * X_AXIS));
            failures++;
        }
    }
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

    expect_eq_u16_array(expected, output, 4, "echo attack=0 copy");
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

    expect_eq_u16_array(expected, output, 4, "echo density=0 copy");
}

static void test_echo_pre_delay_beyond_window_is_copy(void)
{
    const EchoParam param = {
        .delay_samples = 2,
        .pre_delay = 3,
        .density = Q_ONE,
        .attack = Q_ONE,
        .decay = 0,
    };

    const uint16_t input_with_delay[] = {
        (uint16_t)(X_AXIS + 7),
        (uint16_t)(X_AXIS + 14),
        (uint16_t)(X_AXIS + 21),
        (uint16_t)(X_AXIS + 28),
        (uint16_t)(X_AXIS + 35),
        (uint16_t)(X_AXIS + 42),
    };

    const uint16_t expected[] = {
        input_with_delay[2],
        input_with_delay[3],
        input_with_delay[4],
        input_with_delay[5],
    };

    uint16_t output[4] = {0};
    buf_echo(input_with_delay, output, 4, &param);

    expect_eq_u16_array(expected, output, 4, "echo pre-delay beyond window copy");
}

static void test_echo_pre_delay_zero_clamps_to_one(void)
{
    const uint16_t input_with_delay[] = {
        (uint16_t)(X_AXIS + 9),
        (uint16_t)(X_AXIS + 30),
    };

    uint16_t output_ref[1] = {0};
    const EchoParam ref_param = {
        .delay_samples = 1,
        .pre_delay = 1,
        .density = Q_ONE,
        .attack = Q_ONE,
        .decay = Q_ONE,
    };

    uint16_t output_zero_pre_delay[1] = {0};
    const EchoParam zero_pre_delay_param = {
        .delay_samples = 1,
        .pre_delay = 0,
        .density = Q_ONE,
        .attack = Q_ONE,
        .decay = Q_ONE,
    };

    buf_echo(input_with_delay, output_ref, 1, &ref_param);
    buf_echo(input_with_delay, output_zero_pre_delay, 1, &zero_pre_delay_param);

    expect_eq_u16(output_ref[0], output_zero_pre_delay[0], "echo pre-delay=0 clamps to 1");
}

static void test_echo_attack_above_one_clamps(void)
{
    const uint16_t input_with_delay[] = {
        (uint16_t)(X_AXIS + 1000),
        (uint16_t)(X_AXIS + 2000),
    };

    uint16_t output_ref[1] = {0};
    const EchoParam ref_param = {
        .delay_samples = 1,
        .pre_delay = 1,
        .density = Q_ONE,
        .attack = Q_ONE,
        .decay = Q_ONE,
    };

    uint16_t output_over_attack[1] = {0};
    const EchoParam over_attack_param = {
        .delay_samples = 1,
        .pre_delay = 1,
        .density = Q_ONE,
        .attack = Q_ONE + 64,
        .decay = Q_ONE,
    };

    buf_echo(input_with_delay, output_ref, 1, &ref_param);
    buf_echo(input_with_delay, output_over_attack, 1, &over_attack_param);

    expect_eq_u16_array(output_ref, output_over_attack, 1, "echo attack>1 clamps");
}

static void test_echo_decay_above_one_clamps(void)
{
    const uint16_t input_with_delay[] = {
        (uint16_t)(X_AXIS + 500),
        (uint16_t)(X_AXIS + 1000),
    };

    uint16_t output_ref[1] = {0};
    const EchoParam ref_param = {
        .delay_samples = 1,
        .pre_delay = 1,
        .density = Q_ONE,
        .attack = Q_ONE,
        .decay = Q_ONE,
    };

    uint16_t output_over_decay[1] = {0};
    const EchoParam over_decay_param = {
        .delay_samples = 1,
        .pre_delay = 1,
        .density = Q_ONE,
        .attack = Q_ONE,
        .decay = Q_ONE + 64,
    };

    buf_echo(input_with_delay, output_ref, 1, &ref_param);
    buf_echo(input_with_delay, output_over_decay, 1, &over_decay_param);

    expect_eq_u16_array(output_ref, output_over_decay, 1, "echo decay>1 clamps");
}

static void test_echo_zero_delay_is_copy(void)
{
    const EchoParam param = {
        .delay_samples = 0,
        .pre_delay = 1,
        .density = Q_ONE,
        .attack = Q_ONE,
        .decay = Q_ONE,
    };

    const uint16_t input[] = {
        (uint16_t)(X_AXIS + 101),
        (uint16_t)(X_AXIS + 202),
        (uint16_t)(X_AXIS + 303),
    };

    uint16_t output[3] = {0};
    buf_echo(input, output, 3, &param);

    expect_eq_u16(input[0], output[0], "echo delay=0 copy sample 0");
    expect_eq_u16(input[1], output[1], "echo delay=0 copy sample 1");
    expect_eq_u16(input[2], output[2], "echo delay=0 copy sample 2");
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

    expect_eq_u16_array(expected, output, 4, "echo one-step full coverage");
}

static void test_echo_high_density_clamps_spacing(void)
{
    const EchoParam param = {
        .delay_samples = 2,
        .pre_delay = 1,
        .density = (2U * Q_ONE),
        .attack = Q_ONE,
        .decay = Q_ONE,
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
        (uint16_t)(X_AXIS + 50),
        (uint16_t)(X_AXIS + 70),
        (uint16_t)(X_AXIS + 90),
        (uint16_t)(X_AXIS + 110),
    };

    uint16_t output[4] = {0};
    buf_echo(input_with_delay, output, 4, &param);

    expect_eq_u16_array(expected, output, 4, "echo high density spacing clamp");
}

static void test_echo_density_above_one_clamps(void)
{
    const uint16_t input_with_delay[] = {
        (uint16_t)(X_AXIS + 10),
        (uint16_t)(X_AXIS + 20),
        (uint16_t)(X_AXIS + 30),
        (uint16_t)(X_AXIS + 40),
        (uint16_t)(X_AXIS + 50),
        (uint16_t)(X_AXIS + 60),
    };

    uint16_t output_ref[4] = {0};
    const EchoParam ref_param = {
        .delay_samples = 2,
        .pre_delay = 1,
        .density = Q_ONE,
        .attack = Q_ONE,
        .decay = Q_ONE,
    };

    uint16_t output_over_density[4] = {0};
    const EchoParam over_density_param = {
        .delay_samples = 2,
        .pre_delay = 1,
        .density = Q_ONE + 64,
        .attack = Q_ONE,
        .decay = Q_ONE,
    };

    buf_echo(input_with_delay, output_ref, 4, &ref_param);
    buf_echo(input_with_delay, output_over_density, 4, &over_density_param);

    expect_eq_u16_array(output_ref, output_over_density, 4, "echo density>1 clamps");
}

static void test_echo_zero_samples_no_write(void)
{
    const EchoParam param = {
        .delay_samples = 1,
        .pre_delay = 1,
        .density = Q_ONE,
        .attack = Q_ONE,
        .decay = Q_ONE,
    };

    const uint16_t input_with_delay[] = {
        (uint16_t)(X_AXIS + 10),
        (uint16_t)(X_AXIS + 20),
    };

    uint16_t output[2] = {1234, 5678};
    buf_echo(input_with_delay, output, 0, &param);

    expect_eq_u16(1234, output[0], "echo zero-samples preserves output[0]");
    expect_eq_u16(5678, output[1], "echo zero-samples preserves output[1]");
}

static void test_echo_positive_mix_saturates_at_u16_max(void)
{
    const EchoParam param = {
        .delay_samples = 1,
        .pre_delay = 1,
        .density = Q_ONE,
        .attack = Q_ONE,
        .decay = Q_ONE,
    };

    const uint16_t input_with_delay[] = {
        UINT16_MAX,
        UINT16_MAX,
    };

    uint16_t output[1] = {0};
    buf_echo(input_with_delay, output, 1, &param);

    expect_eq_u16(UINT16_MAX, output[0], "echo positive mix saturates");
}

static void test_echo_negative_mix_saturates_at_zero(void)
{
    const EchoParam param = {
        .delay_samples = 1,
        .pre_delay = 1,
        .density = Q_ONE,
        .attack = Q_ONE,
        .decay = Q_ONE,
    };

    const uint16_t input_with_delay[] = {
        0,
        0,
    };

    uint16_t output[1] = {0};
    buf_echo(input_with_delay, output, 1, &param);

    expect_eq_u16(0, output[0], "echo negative mix saturates");
}

static void test_echo_attack_monotonic_positive_case(void)
{
    const uint16_t input_with_delay[] = {
        (uint16_t)(X_AXIS + 500),
        (uint16_t)(X_AXIS + 1000),
    };

    const EchoParam low_attack = {
        .delay_samples = 1,
        .pre_delay = 1,
        .density = Q_ONE,
        .attack = Q_ONE / 4,
        .decay = Q_ONE,
    };

    const EchoParam mid_attack = {
        .delay_samples = 1,
        .pre_delay = 1,
        .density = Q_ONE,
        .attack = Q_ONE / 2,
        .decay = Q_ONE,
    };

    const EchoParam high_attack = {
        .delay_samples = 1,
        .pre_delay = 1,
        .density = Q_ONE,
        .attack = Q_ONE,
        .decay = Q_ONE,
    };

    uint16_t out_low[1] = {0};
    uint16_t out_mid[1] = {0};
    uint16_t out_high[1] = {0};

    buf_echo(input_with_delay, out_low, 1, &low_attack);
    buf_echo(input_with_delay, out_mid, 1, &mid_attack);
    buf_echo(input_with_delay, out_high, 1, &high_attack);

    if (!(out_low[0] <= out_mid[0] && out_mid[0] <= out_high[0]))
    {
        fprintf(stderr, "FAIL: echo attack monotonic low=%u mid=%u high=%u\n", out_low[0], out_mid[0],
            out_high[0]);
        failures++;
    }
}

static void test_runtime_overdrive_dispatch_matches_direct(void)
{
    const uint16_t input[] = {
        (uint16_t)(X_AXIS - 900),
        (uint16_t)(X_AXIS + 900),
    };

    OverdriveParam param = {
        .level = MAX_OVERDRIVE_LEVEL,
        .gain = Q_ONE,
        .tone = Q_ONE,
        .mix = Q_ONE,
    };

    uint16_t direct_output[2] = {0};
    buf_overdrive(input, direct_output, 2, &param);

    EffectInstance instance;
    effect_instance_init(&instance, EFFECT_TYPE_OVERDRIVE);
    if (effect_instance_set_overdrive_params(&instance, &param) != 0)
    {
        fprintf(stderr, "FAIL: runtime overdrive set params\n");
        failures++;
        return;
    }

    uint16_t runtime_output[2] = {0};
    if (effect_instance_process(&instance, input, runtime_output, 2) != 0)
    {
        fprintf(stderr, "FAIL: runtime overdrive process\n");
        failures++;
        return;
    }

    expect_eq_u16_array(direct_output, runtime_output, 2, "runtime overdrive dispatch parity");
}

static void test_runtime_compression_dispatch_matches_direct(void)
{
    const uint16_t input[] = {
        (uint16_t)(X_AXIS - 1000),
        (uint16_t)(X_AXIS + 1000),
    };

    CompressionParam param = {
        .threshold = 300,
        .ratio = Q_ONE,
    };

    uint16_t direct_output[2] = {0};
    buf_compression(input, direct_output, 2, &param);

    EffectInstance instance;
    effect_instance_init(&instance, EFFECT_TYPE_COMPRESSION);
    if (effect_instance_set_compression_params(&instance, &param) != 0)
    {
        fprintf(stderr, "FAIL: runtime compression set params\n");
        failures++;
        return;
    }

    uint16_t runtime_output[2] = {0};
    if (effect_instance_process(&instance, input, runtime_output, 2) != 0)
    {
        fprintf(stderr, "FAIL: runtime compression process\n");
        failures++;
        return;
    }

    expect_eq_u16_array(direct_output, runtime_output, 2, "runtime compression dispatch parity");
}

static void test_runtime_rejects_wrong_param_setter(void)
{
    EffectInstance instance;
    effect_instance_init(&instance, EFFECT_TYPE_ECHO);

    OverdriveParam overdrive = {
        .level = MAX_OVERDRIVE_LEVEL,
        .gain = Q_ONE,
        .tone = Q_ONE,
        .mix = 0,
    };

    if (effect_instance_set_overdrive_params(&instance, &overdrive) == 0)
    {
        fprintf(stderr, "FAIL: runtime accepted wrong param setter\n");
        failures++;
    }
}

static void test_runtime_overdrive_setter_normalizes_params(void)
{
    const uint16_t input[] = {
        (uint16_t)(X_AXIS - 900),
        (uint16_t)(X_AXIS + 900),
    };

    OverdriveParam raw_param = {
        .level = (size_t)MAX_OVERDRIVE_LEVEL + 1,
        .gain = Q_ONE + 64,
        .tone = 0,
        .mix = Q_ONE + 64,
    };

    OverdriveParam clamped_param = {
        .level = MAX_OVERDRIVE_LEVEL,
        .gain = Q_ONE,
        .tone = MIN_OVERDRIVE_TONE,
        .mix = Q_ONE,
    };

    uint16_t direct_output[2] = {0};
    buf_overdrive(input, direct_output, 2, &clamped_param);

    EffectInstance instance;
    effect_instance_init(&instance, EFFECT_TYPE_OVERDRIVE);
    if (effect_instance_set_overdrive_params(&instance, &raw_param) != 0)
    {
        fprintf(stderr, "FAIL: runtime overdrive normalize set params\n");
        failures++;
        return;
    }

    uint16_t runtime_output[2] = {0};
    if (effect_instance_process(&instance, input, runtime_output, 2) != 0)
    {
        fprintf(stderr, "FAIL: runtime overdrive normalize process\n");
        failures++;
        return;
    }

    expect_eq_u16_array(direct_output, runtime_output, 2, "runtime overdrive normalize parity");
}

static void test_runtime_echo_setter_normalizes_params(void)
{
    const uint16_t input_with_delay[] = {
        (uint16_t)(X_AXIS + 10),
        (uint16_t)(X_AXIS + 20),
        (uint16_t)(X_AXIS + 30),
        (uint16_t)(X_AXIS + 40),
        (uint16_t)(X_AXIS + 50),
        (uint16_t)(X_AXIS + 60),
    };

    EchoParam raw_param = {
        .delay_samples = 2,
        .pre_delay = 3,
        .density = Q_ONE + 64,
        .attack = Q_ONE + 64,
        .decay = Q_ONE + 64,
    };

    EchoParam clamped_param = {
        .delay_samples = 2,
        .pre_delay = 2,
        .density = Q_ONE,
        .attack = Q_ONE,
        .decay = Q_ONE,
    };

    uint16_t direct_output[4] = {0};
    buf_echo(input_with_delay, direct_output, 4, &clamped_param);

    EffectInstance instance;
    effect_instance_init(&instance, EFFECT_TYPE_ECHO);
    if (effect_instance_set_echo_params(&instance, &raw_param) != 0)
    {
        fprintf(stderr, "FAIL: runtime echo normalize set params\n");
        failures++;
        return;
    }

    uint16_t runtime_output[4] = {0};
    if (effect_instance_process(&instance, input_with_delay, runtime_output, 4) != 0)
    {
        fprintf(stderr, "FAIL: runtime echo normalize process\n");
        failures++;
        return;
    }

    expect_eq_u16_array(direct_output, runtime_output, 4, "runtime echo normalize parity");
}

static void test_runtime_compression_setter_normalizes_params(void)
{
    const uint16_t input[] = {
        0,
        UINT16_MAX,
    };

    CompressionParam raw_param = {
        .threshold = X_AXIS + 1000,
        .ratio = Q_ONE + 64,
    };

    CompressionParam clamped_param = {
        .threshold = X_AXIS,
        .ratio = Q_ONE,
    };

    uint16_t direct_output[2] = {0};
    buf_compression(input, direct_output, 2, &clamped_param);

    EffectInstance instance;
    effect_instance_init(&instance, EFFECT_TYPE_COMPRESSION);
    if (effect_instance_set_compression_params(&instance, &raw_param) != 0)
    {
        fprintf(stderr, "FAIL: runtime compression normalize set params\n");
        failures++;
        return;
    }

    uint16_t runtime_output[2] = {0};
    if (effect_instance_process(&instance, input, runtime_output, 2) != 0)
    {
        fprintf(stderr, "FAIL: runtime compression normalize process\n");
        failures++;
        return;
    }

    expect_eq_u16_array(direct_output, runtime_output, 2, "runtime compression normalize parity");
}

static void test_handle_rejects_use_before_init(void)
{
    EffectHandle handle = {0};
    uint16_t input[1] = {(uint16_t)X_AXIS};
    uint16_t output[1] = {0};

    if (effect_handle_process(&handle, input, output, 1) == 0)
    {
        fprintf(stderr, "FAIL: handle accepted process before init\n");
        failures++;
    }
}

static void test_handle_overdrive_parity(void)
{
    const uint16_t input[] = {
        (uint16_t)(X_AXIS - 900),
        (uint16_t)(X_AXIS + 900),
    };

    OverdriveParam param = {
        .level = MAX_OVERDRIVE_LEVEL,
        .gain = Q_ONE,
        .tone = Q_ONE,
        .mix = Q_ONE,
    };

    uint16_t direct_output[2] = {0};
    buf_overdrive(input, direct_output, 2, &param);

    EffectHandle handle = {0};
    if (effect_handle_init(&handle, EFFECT_TYPE_OVERDRIVE) != 0)
    {
        fprintf(stderr, "FAIL: handle overdrive init\n");
        failures++;
        return;
    }

    if (effect_handle_set_overdrive_params(&handle, &param) != 0)
    {
        fprintf(stderr, "FAIL: handle overdrive set params\n");
        failures++;
        return;
    }

    uint16_t handle_output[2] = {0};
    if (effect_handle_process(&handle, input, handle_output, 2) != 0)
    {
        fprintf(stderr, "FAIL: handle overdrive process\n");
        failures++;
        return;
    }

    expect_eq_u16_array(direct_output, handle_output, 2, "handle overdrive parity");
}

static void test_handle_rejects_wrong_param_setter(void)
{
    EffectHandle handle = {0};
    if (effect_handle_init(&handle, EFFECT_TYPE_ECHO) != 0)
    {
        fprintf(stderr, "FAIL: handle echo init for wrong setter test\n");
        failures++;
        return;
    }

    OverdriveParam overdrive = {
        .level = MAX_OVERDRIVE_LEVEL,
        .gain = Q_ONE,
        .tone = Q_ONE,
        .mix = 0,
    };

    if (effect_handle_set_overdrive_params(&handle, &overdrive) == 0)
    {
        fprintf(stderr, "FAIL: handle accepted wrong param setter\n");
        failures++;
    }
}

static void test_handle_process_rejects_null_buffers(void)
{
    EffectHandle handle = {0};
    uint16_t input[1] = {(uint16_t)X_AXIS};
    uint16_t output[1] = {0};

    if (effect_handle_init(&handle, EFFECT_TYPE_OVERDRIVE) != 0)
    {
        fprintf(stderr, "FAIL: handle init for null buffer test\n");
        failures++;
        return;
    }

    if (effect_handle_process(&handle, NULL, output, 1) == 0)
    {
        fprintf(stderr, "FAIL: handle accepted null input buffer\n");
        failures++;
    }

    if (effect_handle_process(&handle, input, NULL, 1) == 0)
    {
        fprintf(stderr, "FAIL: handle accepted null output buffer\n");
        failures++;
    }
}

static void test_handle_echo_parity(void)
{
    const uint16_t input_with_delay[] = {
        (uint16_t)(X_AXIS + 100),
        (uint16_t)(X_AXIS + 200),
        (uint16_t)(X_AXIS + 300),
        (uint16_t)(X_AXIS + 400),
        (uint16_t)(X_AXIS + 500),
    };

    EchoParam param = {
        .delay_samples = 2,
        .pre_delay = 1,
        .density = Q_ONE,
        .attack = Q_ONE,
        .decay = Q_ONE,
    };

    uint16_t direct_output[3] = {0};
    buf_echo(input_with_delay, direct_output, 3, &param);

    EffectHandle handle = {0};
    if (effect_handle_init(&handle, EFFECT_TYPE_ECHO) != 0)
    {
        fprintf(stderr, "FAIL: handle echo init\n");
        failures++;
        return;
    }

    if (effect_handle_set_echo_params(&handle, &param) != 0)
    {
        fprintf(stderr, "FAIL: handle echo set params\n");
        failures++;
        return;
    }

    uint16_t handle_output[3] = {0};
    if (effect_handle_process(&handle, input_with_delay, handle_output, 3) != 0)
    {
        fprintf(stderr, "FAIL: handle echo process\n");
        failures++;
        return;
    }

    expect_eq_u16_array(direct_output, handle_output, 3, "handle echo parity");
}

static void test_handle_reset_restores_defaults(void)
{
    const uint16_t input[] = {
        (uint16_t)(X_AXIS - 1200),
        (uint16_t)(X_AXIS + 1200),
    };

    CompressionParam custom = {
        .threshold = 300,
        .ratio = Q_ONE,
    };

    CompressionParam defaults = {
        .threshold = X_AXIS,
        .ratio = 0,
    };

    EffectHandle handle = {0};
    if (effect_handle_init(&handle, EFFECT_TYPE_COMPRESSION) != 0)
    {
        fprintf(stderr, "FAIL: handle compression init\n");
        failures++;
        return;
    }

    if (effect_handle_set_compression_params(&handle, &custom) != 0)
    {
        fprintf(stderr, "FAIL: handle compression set custom\n");
        failures++;
        return;
    }

    if (effect_handle_reset(&handle) != 0)
    {
        fprintf(stderr, "FAIL: handle compression reset\n");
        failures++;
        return;
    }

    uint16_t handle_output[2] = {0};
    if (effect_handle_process(&handle, input, handle_output, 2) != 0)
    {
        fprintf(stderr, "FAIL: handle compression process after reset\n");
        failures++;
        return;
    }

    uint16_t direct_output[2] = {0};
    buf_compression(input, direct_output, 2, &defaults);

    expect_eq_u16_array(direct_output, handle_output, 2, "handle reset default parity");
}

int main(void)
{
    test_q_tanh_clamps();
    test_q_tanh_odd_symmetry();
    test_q_tanh_is_bounded();
    test_q_tanh_monotonic();
    test_overdrive_dry_passthrough();
    test_overdrive_zero_level_mutes_wet();
    test_overdrive_full_wet_changes_signal();
    test_overdrive_mix_above_one_clamps_to_wet();
    test_overdrive_gain_above_one_clamps();
    test_overdrive_tone_below_one_clamps();
    test_overdrive_level_above_max_clamps();
    test_overdrive_tone_above_max_clamps();
    test_overdrive_sign_symmetry();
    test_compression_ratio_behavior();
    test_compression_zero_threshold_hard_clip();
    test_compression_ratio_above_one_clamps_to_hard_clip();
    test_compression_threshold_above_axis_clamps();
    test_compression_threshold_above_axis_clamps_negative_side();
    test_compression_sign_symmetry();
    test_echo_attack_zero_is_copy();
    test_echo_zero_density_is_copy();
    test_echo_pre_delay_beyond_window_is_copy();
    test_echo_pre_delay_zero_clamps_to_one();
    test_echo_attack_above_one_clamps();
    test_echo_decay_above_one_clamps();
    test_echo_zero_delay_is_copy();
    test_echo_single_step_full_coverage();
    test_echo_high_density_clamps_spacing();
    test_echo_density_above_one_clamps();
    test_echo_zero_samples_no_write();
    test_echo_positive_mix_saturates_at_u16_max();
    test_echo_negative_mix_saturates_at_zero();
    test_echo_attack_monotonic_positive_case();
    test_runtime_overdrive_dispatch_matches_direct();
    test_runtime_compression_dispatch_matches_direct();
    test_runtime_rejects_wrong_param_setter();
    test_runtime_overdrive_setter_normalizes_params();
    test_runtime_echo_setter_normalizes_params();
    test_runtime_compression_setter_normalizes_params();
    test_handle_rejects_use_before_init();
    test_handle_overdrive_parity();
    test_handle_rejects_wrong_param_setter();
    test_handle_process_rejects_null_buffers();
    test_handle_echo_parity();
    test_handle_reset_restores_defaults();

    if (failures != 0)
    {
        fprintf(stderr, "DSP tests failed: %d\n", failures);
        return 1;
    }

    puts("DSP tests passed");
    return 0;
}
