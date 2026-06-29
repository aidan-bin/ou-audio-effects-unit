#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bam.h"
#include "effects.h"
#include "effect_runtime.h"
#include "fast_math.h"
#include "harness/expect.h"

#define Q_ONE QN_ONE // 1.0 in QN (canonical, from fixed_point.h)

int failures = 0;

static void test_q_tanh_clamps(void)
{
    expect_eq_i32((int32_t)Q_ONE, q_tanh(10000), "q_tanh positive clamp");
    expect_eq_i32(-(int32_t)Q_ONE, q_tanh(-10000), "q_tanh negative clamp");
    expect_eq_i32(0, q_tanh(0), "q_tanh zero");
}

static void test_q_tanh_odd_symmetry(void)
{
    for (int32_t x = 1; x <= (int32_t)(5 * Q_ONE); x += 13)
    {
        int32_t positive = q_tanh(x);
        int32_t negative = q_tanh(-x);

        if (negative != -positive)
        {
            fprintf(stderr, "FAIL: q_tanh odd symmetry x=%d tanh(x)=%d tanh(-x)=%d\n", x, positive,
                    negative);
            failures++;
        }
    }
}

static void test_overdrive_extreme_inputs_stay_bounded(void)
{
    const uint16_t input[] = {
        0,
        (uint16_t)X_AXIS,
        UINT16_MAX,
    };

    uint16_t output[3] = {0};
    OverdriveParam param = {
        .level = MAX_OVERDRIVE_LEVEL,
        .gain = Q_ONE,
        .tone = MAX_OVERDRIVE_TONE,
        .mix = Q_ONE,
    };

    buf_overdrive(input, output, 3, &param);

    if (!(output[0] <= (uint16_t)X_AXIS && output[1] == (uint16_t)X_AXIS &&
          output[2] >= (uint16_t)X_AXIS))
    {
        fprintf(stderr, "FAIL: overdrive extreme bounds low=%u mid=%u high=%u\n", output[0],
                output[1], output[2]);
        failures++;
    }

    if (output[0] > output[2])
    {
        fprintf(stderr, "FAIL: overdrive extreme monotonic low=%u high=%u\n", output[0], output[2]);
        failures++;
    }
}

static void test_q_tanh_is_bounded(void)
{
    for (int16_t x = (int16_t)(-8 * Q_ONE); x <= (int16_t)(8 * Q_ONE); x += 11)
    {
        int32_t y = q_tanh(x);
        if (y < -(int32_t)Q_ONE || y > (int32_t)Q_ONE)
        {
            fprintf(stderr, "FAIL: q_tanh boundedness x=%d y=%d\n", x, y);
            failures++;
        }
    }
}

static void test_q_tanh_monotonic(void)
{
    int32_t prev = q_tanh((int16_t)(-8 * Q_ONE));

    for (int16_t x = (int16_t)(-8 * Q_ONE + 1); x <= (int16_t)(8 * Q_ONE); x++)
    {
        int32_t current = q_tanh(x);
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

    expect_eq_u16_array(input, output, sizeof(input) / sizeof(input[0]),
                        "overdrive dry passthrough");
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

static void test_overdrive_param_clamps(void)
{
    const struct
    {
        OverdriveParam ref;
        OverdriveParam variant;
        uint16_t delta;
        const char *label;
    } cases[] = {
        {{.level = MAX_OVERDRIVE_LEVEL, .gain = Q_ONE, .tone = Q_ONE, .mix = Q_ONE},
         {.level = MAX_OVERDRIVE_LEVEL, .gain = Q_ONE, .tone = Q_ONE, .mix = Q_ONE + 64},
         900,
         "overdrive mix>1 clamps"},
        {{.level = MAX_OVERDRIVE_LEVEL, .gain = Q_ONE, .tone = Q_ONE, .mix = Q_ONE},
         {.level = MAX_OVERDRIVE_LEVEL, .gain = Q_ONE + 64, .tone = Q_ONE, .mix = Q_ONE},
         1100,
         "overdrive gain>1 clamps"},
        {{.level = MAX_OVERDRIVE_LEVEL, .gain = Q_ONE, .tone = Q_ONE, .mix = Q_ONE},
         {.level = MAX_OVERDRIVE_LEVEL, .gain = Q_ONE, .tone = 0, .mix = Q_ONE},
         1300,
         "overdrive tone<1 clamps"},
        {{.level = MAX_OVERDRIVE_LEVEL, .gain = Q_ONE, .tone = Q_ONE, .mix = Q_ONE},
         {.level = (size_t)MAX_OVERDRIVE_LEVEL + 1, .gain = Q_ONE, .tone = Q_ONE, .mix = Q_ONE},
         1300,
         "overdrive level>max clamps"},
        {{.level = MAX_OVERDRIVE_LEVEL, .gain = Q_ONE, .tone = MAX_OVERDRIVE_TONE, .mix = Q_ONE},
         {.level = MAX_OVERDRIVE_LEVEL, .gain = Q_ONE, .tone = MAX_OVERDRIVE_TONE + Q_ONE, .mix = Q_ONE},
         1300,
         "overdrive tone>max clamps"},
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        const uint16_t input[2] = {
            (uint16_t)(X_AXIS - cases[i].delta),
            (uint16_t)(X_AXIS + cases[i].delta),
        };
        uint16_t output_ref[2] = {0};
        uint16_t output_variant[2] = {0};

        buf_overdrive(input, output_ref, 2, &cases[i].ref);
        buf_overdrive(input, output_variant, 2, &cases[i].variant);

        expect_eq_u16_array(output_ref, output_variant, 2, cases[i].label);
    }
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

static void test_compression_extreme_inputs_stay_ordered(void)
{
    const uint16_t input[] = {
        0,
        (uint16_t)(X_AXIS - 1000),
        (uint16_t)X_AXIS,
        (uint16_t)(X_AXIS + 1000),
        UINT16_MAX,
    };

    uint16_t output[5] = {0};
    CompressionParam param = {
        .threshold = 300,
        .ratio = Q_ONE,
    };

    buf_compression(input, output, 5, &param);

    for (size_t i = 1; i < 5; i++)
    {
        if (output[i - 1] > output[i])
        {
            fprintf(stderr, "FAIL: compression ordering idx=%zu prev=%u curr=%u\n", i,
                    output[i - 1], output[i]);
            failures++;
            return;
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
        .density = (size_t)(2U * Q_ONE),
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
        fprintf(stderr, "FAIL: echo attack monotonic low=%u mid=%u high=%u\n", out_low[0],
                out_mid[0], out_high[0]);
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
    EXPECT_OK(effect_instance_set_overdrive_params(&instance, &param), "runtime overdrive set params");

    uint16_t runtime_output[2] = {0};
    EXPECT_OK(effect_instance_process(&instance, input, runtime_output, 2), "runtime overdrive process");

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
    EXPECT_OK(effect_instance_set_compression_params(&instance, &param), "runtime compression set params");

    uint16_t runtime_output[2] = {0};
    EXPECT_OK(effect_instance_process(&instance, input, runtime_output, 2), "runtime compression process");

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
    EXPECT_OK(effect_instance_set_overdrive_params(&instance, &raw_param), "runtime overdrive normalize set params");

    uint16_t runtime_output[2] = {0};
    EXPECT_OK(effect_instance_process(&instance, input, runtime_output, 2), "runtime overdrive normalize process");

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
    EXPECT_OK(effect_instance_set_echo_params(&instance, &raw_param), "runtime echo normalize set params");

    uint16_t runtime_output[4] = {0};
    EXPECT_OK(effect_instance_process(&instance, input_with_delay, runtime_output, 4), "runtime echo normalize process");

    expect_eq_u16_array(direct_output, runtime_output, 4, "runtime echo normalize parity");
}

static void test_runtime_echo_zero_delay_normalizes_to_copy(void)
{
    const uint16_t input_with_delay[] = {
        (uint16_t)(X_AXIS + 10),
        (uint16_t)(X_AXIS + 20),
        (uint16_t)(X_AXIS + 30),
        (uint16_t)(X_AXIS + 40),
    };

    EchoParam raw_param = {
        .delay_samples = 0,
        .pre_delay = 3,
        .density = Q_ONE,
        .attack = Q_ONE,
        .decay = Q_ONE,
    };

    uint16_t direct_output[4] = {0};
    buf_echo(input_with_delay, direct_output, 4, &raw_param);

    EffectInstance instance;
    effect_instance_init(&instance, EFFECT_TYPE_ECHO);
    EXPECT_OK(effect_instance_set_echo_params(&instance, &raw_param), "runtime zero-delay echo normalize set params");

    uint16_t runtime_output[4] = {0};
    EXPECT_OK(effect_instance_process(&instance, input_with_delay, runtime_output, 4), "runtime zero-delay echo normalize process");

    expect_eq_u16_array(direct_output, runtime_output, 4,
                        "runtime zero-delay echo normalize parity");
}

static void test_bam_conversion_round_trip(void)
{
    bam_t signed_half_turn = float_deg_to_bam(90.0f);
    bam_t signed_negative_half_turn = float_deg_to_bam(-90.0f);
    ubam_t unsigned_three_quarters_turn = float_deg_to_ubam(270.0f);

    expect_eq_i16((int16_t)(UBAM_180_DEG / 2), signed_half_turn, "bam 90 degree conversion");
    expect_eq_i16((int16_t)(-(int16_t)(UBAM_180_DEG / 2)), signed_negative_half_turn,
                  "bam -90 degree conversion");

    if (unsigned_three_quarters_turn != (ubam_t)(UBAM_180_DEG + UBAM_180_DEG / 2))
    {
        fprintf(stderr, "FAIL: ubam 270 degree conversion expected=%u actual=%u\n",
                (unsigned)(UBAM_180_DEG + UBAM_180_DEG / 2),
                (unsigned)unsigned_three_quarters_turn);
        failures++;
    }

    if (bam_to_float_deg(signed_half_turn) != 90.0f)
    {
        fprintf(stderr, "FAIL: bam 90 degree float round trip actual=%f\n",
                bam_to_float_deg(signed_half_turn));
        failures++;
    }

    if (ubam_to_float_deg(unsigned_three_quarters_turn) != 270.0f)
    {
        fprintf(stderr, "FAIL: ubam 270 degree float round trip actual=%f\n",
                ubam_to_float_deg(unsigned_three_quarters_turn));
        failures++;
    }
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
    EXPECT_OK(effect_instance_set_compression_params(&instance, &raw_param), "runtime compression normalize set params");

    uint16_t runtime_output[2] = {0};
    EXPECT_OK(effect_instance_process(&instance, input, runtime_output, 2), "runtime compression normalize process");

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
    EXPECT_OK(effect_handle_init(&handle, EFFECT_TYPE_OVERDRIVE), "handle overdrive init");

    EXPECT_OK(effect_handle_set_overdrive_params(&handle, &param), "handle overdrive set params");

    uint16_t handle_output[2] = {0};
    EXPECT_OK(effect_handle_process(&handle, input, handle_output, 2), "handle overdrive process");

    expect_eq_u16_array(direct_output, handle_output, 2, "handle overdrive parity");
}

static void test_handle_rejects_wrong_param_setter(void)
{
    EffectHandle handle = {0};
    EXPECT_OK(effect_handle_init(&handle, EFFECT_TYPE_ECHO), "handle echo init for wrong setter test");

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

    EXPECT_OK(effect_handle_init(&handle, EFFECT_TYPE_OVERDRIVE), "handle init for null buffer test");

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
    EXPECT_OK(effect_handle_init(&handle, EFFECT_TYPE_ECHO), "handle echo init");

    EXPECT_OK(effect_handle_set_echo_params(&handle, &param), "handle echo set params");

    uint16_t handle_output[3] = {0};
    EXPECT_OK(effect_handle_process(&handle, input_with_delay, handle_output, 3), "handle echo process");

    expect_eq_u16_array(direct_output, handle_output, 3, "handle echo parity");
}

static void test_handle_echo_delay_accessor(void)
{
    EffectHandle handle = {0};
    EXPECT_OK(effect_handle_init(&handle, EFFECT_TYPE_ECHO), "handle echo init for delay accessor");

    EchoParam param = {
        .delay_samples = 2,
        .pre_delay = 1,
        .density = Q_ONE,
        .attack = Q_ONE,
        .decay = Q_ONE,
    };

    EXPECT_OK(effect_handle_set_echo_params(&handle, &param), "handle echo set params for delay accessor");

    size_t delay_samples = 0;
    EXPECT_OK(effect_handle_get_echo_delay_samples(&handle, &delay_samples), "handle echo delay accessor returned error");

    if (delay_samples != 2)
    {
        fprintf(stderr, "FAIL: handle echo delay accessor expected=2 actual=%zu\n", delay_samples);
        failures++;
    }
}

static void test_handle_echo_delay_accessor_rejects_wrong_type(void)
{
    EffectHandle handle = {0};
    EXPECT_OK(effect_handle_init(&handle, EFFECT_TYPE_OVERDRIVE), "handle overdrive init for delay accessor type test");

    size_t delay_samples = 0;
    if (effect_handle_get_echo_delay_samples(&handle, &delay_samples) == 0)
    {
        fprintf(stderr, "FAIL: handle echo delay accessor accepted non-echo handle\n");
        failures++;
    }
}

static void test_handle_echo_delay_accessor_rejects_null_out_ptr(void)
{
    EffectHandle handle = {0};
    EXPECT_OK(effect_handle_init(&handle, EFFECT_TYPE_ECHO), "handle echo init for delay null ptr test");

    if (effect_handle_get_echo_delay_samples(&handle, NULL) == 0)
    {
        fprintf(stderr, "FAIL: handle echo delay accessor accepted null output pointer\n");
        failures++;
    }
}

static void test_handle_echo_delay_accessor_rejects_uninitialized_handle(void)
{
    EffectHandle handle = {0};
    size_t delay_samples = 0;

    if (effect_handle_get_echo_delay_samples(&handle, &delay_samples) == 0)
    {
        fprintf(stderr, "FAIL: handle echo delay accessor accepted uninitialized handle\n");
        failures++;
    }
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
    EXPECT_OK(effect_handle_init(&handle, EFFECT_TYPE_COMPRESSION), "handle compression init");

    EXPECT_OK(effect_handle_set_compression_params(&handle, &custom), "handle compression set custom");

    EXPECT_OK(effect_handle_reset(&handle), "handle compression reset");

    uint16_t handle_output[2] = {0};
    EXPECT_OK(effect_handle_process(&handle, input, handle_output, 2), "handle compression process after reset");

    uint16_t direct_output[2] = {0};
    buf_compression(input, direct_output, 2, &defaults);

    expect_eq_u16_array(direct_output, handle_output, 2, "handle reset default parity");
}

static void test_echo_feedback_zero_is_noop(void)
{
    uint16_t line[8];
    EchoFeedback fb = {.line = line, .line_len = 8};
    echo_feedback_reset(&fb);

    const EchoParam param = {.feedback = 0, .feedback_delay = 2, .damping = 0};
    const uint16_t in[4] = {(uint16_t)(X_AXIS + 100), (uint16_t)(X_AXIS - 50),
                            (uint16_t)(X_AXIS + 7), (uint16_t)X_AXIS};
    uint16_t out[4] = {1, 2, 3, 4};
    const uint16_t expected[4] = {1, 2, 3, 4};

    buf_echo_feedback(&fb, in, out, 4, &param);
    expect_eq_u16_array(expected, out, 4, "feedback=0 leaves out_buf untouched");
}

static void test_echo_feedback_decaying_repeats(void)
{
    uint16_t line[4];
    EchoFeedback fb = {.line = line, .line_len = 4};
    echo_feedback_reset(&fb);

    const EchoParam param = {.feedback = Q_ONE / 2U, .feedback_delay = 2, .damping = 0};
    uint16_t in[8] = {0};
    uint16_t out[8];
    for (size_t i = 0; i < 8; i++)
    {
        in[i] = (uint16_t)X_AXIS;
        out[i] = (uint16_t)X_AXIS;
    }
    in[0] = (uint16_t)(X_AXIS + 100);

    buf_echo_feedback(&fb, in, out, 8, &param);

    expect_eq_u16((uint16_t)(X_AXIS + 100), out[2], "first feedback repeat at delay");
    expect_eq_u16((uint16_t)(X_AXIS + 50), out[4], "second repeat halved");
    expect_eq_u16((uint16_t)(X_AXIS + 25), out[6], "third repeat quartered");
    expect_eq_u16((uint16_t)X_AXIS, out[1], "no repeat between taps (n=1)");
    expect_eq_u16((uint16_t)X_AXIS, out[3], "no repeat between taps (n=3)");
}

static void test_echo_feedback_clamps_and_decays(void)
{
    uint16_t line[16];
    EchoFeedback fb = {.line = line, .line_len = 16};
    echo_feedback_reset(&fb);

    const EchoParam param = {
        .feedback = Q_ONE * 4U, .feedback_delay = 4, .damping = 0};

    uint16_t impulse[8];
    uint16_t silence[8];
    uint16_t out[8];
    for (size_t i = 0; i < 8; i++)
    {
        impulse[i] = (uint16_t)X_AXIS;
        silence[i] = (uint16_t)X_AXIS;
    }
    impulse[0] = (uint16_t)(X_AXIS + 5000);

    for (size_t i = 0; i < 8; i++)
    {
        out[i] = (uint16_t)X_AXIS;
    }
    buf_echo_feedback(&fb, impulse, out, 8, &param);

    int32_t initial_energy = 0;
    for (size_t i = 0; i < fb.line_len; i++)
    {
        initial_energy += abs((int32_t)line[i] - X_AXIS);
    }
    expect_true(initial_energy > 0, "feedback line holds energy after impulse");

    bool bounded = true;
    int32_t final_energy = initial_energy;
    for (int frame = 0; frame < 60; frame++)
    {
        for (size_t i = 0; i < 8; i++)
        {
            out[i] = (uint16_t)X_AXIS;
        }
        buf_echo_feedback(&fb, silence, out, 8, &param);

        int32_t energy = 0;
        for (size_t i = 0; i < fb.line_len; i++)
        {
            energy += abs((int32_t)line[i] - X_AXIS);
        }
        if (energy > 2 * initial_energy)
        {
            bounded = false;
        }
        final_energy = energy;
    }
    expect_true(bounded, "clamped feedback stays bounded (no divergence)");
    expect_true(final_energy < initial_energy / 2, "clamped feedback tail decays over time");
}

static void test_echo_feedback_reset_clears(void)
{
    uint16_t line[4];
    EchoFeedback fb = {.line = line, .line_len = 4, .write_idx = 3, .lpf_state = 123};
    for (size_t i = 0; i < 4; i++)
    {
        line[i] = 0;
    }

    echo_feedback_reset(&fb);

    for (size_t i = 0; i < 4; i++)
    {
        expect_eq_u16((uint16_t)X_AXIS, line[i], "reset fills line with silence");
    }
    expect_true(fb.write_idx == 0, "reset clears write index");
    expect_true(fb.lpf_state == 0, "reset clears lpf state");
}

static void test_echo_feedback_null_safe(void)
{
    const EchoParam param = {.feedback = Q_ONE / 2U, .feedback_delay = 1};
    uint16_t buf[2] = {(uint16_t)X_AXIS, (uint16_t)X_AXIS};

    buf_echo_feedback(NULL, buf, buf, 2, &param); /* must not crash */

    EchoFeedback fb_null_line = {.line = NULL, .line_len = 4};
    buf_echo_feedback(&fb_null_line, buf, buf, 2, &param); /* must not crash */

    echo_feedback_reset(NULL); /* must not crash */
}

static void test_buf_echo_ring_matches_contiguous_across_offsets(void)
{
    const EchoParam param = {
        .delay_samples = 3, .pre_delay = 1, .density = Q_ONE, .attack = Q_ONE, .decay = 0};
    const size_t num = 5;
    const size_t len = param.delay_samples + num;

    uint16_t in_buf[8];
    for (size_t i = 0; i < len; i++)
    {
        in_buf[i] = (uint16_t)(X_AXIS + (int)(i * 37) - 80);
    }

    uint16_t expected[5] = {0};
    buf_echo(in_buf, expected, num, &param);

    for (size_t start = 0; start < len; start++)
    {
        uint16_t ring[8];
        for (size_t i = 0; i < len; i++)
        {
            ring[(start + i) % len] = in_buf[i];
        }
        uint16_t out[5] = {0};
        buf_echo_ring(ring, len, start, out, num, &param);
        expect_eq_u16_array(expected, out, num, "buf_echo_ring matches contiguous across offsets");
    }
}

static void test_instance_echo_stateful_delays_prior_input(void)
{
    uint16_t history[64];
    uint16_t fb_line[32];
    EchoState st;
    st.history = history;
    st.history_len = sizeof(history) / sizeof(history[0]);
    st.fb.line = fb_line;
    st.fb.line_len = sizeof(fb_line) / sizeof(fb_line[0]);
    st.prev_enabled = 0;
    echo_state_reset(&st);

    EffectInstance inst;
    effect_instance_init(&inst, EFFECT_TYPE_ECHO);
    const EchoParam p = {
        .delay_samples = 4, .pre_delay = 1, .density = Q_ONE, .attack = Q_ONE, .decay = 0,
        .feedback = 0, .feedback_delay = 0, .damping = 0};
    expect_true(effect_instance_set_echo_params(&inst, &p) == 0, "set echo params");
    expect_true(effect_instance_attach_echo_state(&inst, &st) == 0, "attach echo state");

    uint16_t in1[4] = {(uint16_t)(X_AXIS + 500), (uint16_t)X_AXIS, (uint16_t)X_AXIS,
                       (uint16_t)X_AXIS};
    uint16_t out1[4] = {0};
    expect_true(effect_instance_process(&inst, in1, out1, 4) == 0, "echo process frame1");

    uint16_t oracle_in1[8];
    for (size_t i = 0; i < 4; i++)
    {
        oracle_in1[i] = (uint16_t)X_AXIS;
        oracle_in1[4 + i] = in1[i];
    }
    uint16_t oracle1[4] = {0};
    buf_echo(oracle_in1, oracle1, 4, &p);
    expect_eq_u16_array(oracle1, out1, 4, "stateful echo frame1 uses silent history");

    uint16_t in2[4] = {(uint16_t)X_AXIS, (uint16_t)X_AXIS, (uint16_t)X_AXIS, (uint16_t)X_AXIS};
    uint16_t out2[4] = {0};
    expect_true(effect_instance_process(&inst, in2, out2, 4) == 0, "echo process frame2");

    uint16_t oracle_in2[8];
    for (size_t i = 0; i < 4; i++)
    {
        oracle_in2[i] = in1[i];
        oracle_in2[4 + i] = in2[i];
    }
    uint16_t oracle2[4] = {0};
    buf_echo(oracle_in2, oracle2, 4, &p);
    expect_eq_u16_array(oracle2, out2, 4, "stateful echo frame2 delays prior frame's input");

    effect_instance_reset_state(&inst);
    bool cleared = true;
    for (size_t i = 0; i < st.history_len; i++)
    {
        if (history[i] != (uint16_t)X_AXIS)
        {
            cleared = false;
        }
    }
    expect_true(cleared, "reset_state clears echo history to silence");
    expect_true(st.history_w == 0, "reset_state clears history write index");
}

static void test_instance_echo_stateless_without_state(void)
{
    EffectInstance inst;
    effect_instance_init(&inst, EFFECT_TYPE_ECHO);
    const EchoParam p = {
        .delay_samples = 2, .pre_delay = 1, .density = Q_ONE, .attack = Q_ONE, .decay = 0};
    expect_true(effect_instance_set_echo_params(&inst, &p) == 0, "set echo params (stateless)");

    uint16_t in_buf[6];
    for (size_t i = 0; i < 6; i++)
    {
        in_buf[i] = (uint16_t)(X_AXIS + (int)(i * 25));
    }
    uint16_t out[4] = {0};
    uint16_t expected[4] = {0};
    expect_true(effect_instance_process(&inst, in_buf, out, 4) == 0, "stateless echo process");
    buf_echo(in_buf, expected, 4, &p);
    expect_eq_u16_array(expected, out, 4, "unattached echo instance == stateless buf_echo");
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
    test_overdrive_param_clamps();
    test_overdrive_sign_symmetry();
    test_overdrive_extreme_inputs_stay_bounded();
    test_compression_ratio_behavior();
    test_compression_zero_threshold_hard_clip();
    test_compression_ratio_above_one_clamps_to_hard_clip();
    test_compression_threshold_above_axis_clamps();
    test_compression_threshold_above_axis_clamps_negative_side();
    test_compression_sign_symmetry();
    test_compression_extreme_inputs_stay_ordered();
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
    test_runtime_echo_zero_delay_normalizes_to_copy();
    test_runtime_compression_setter_normalizes_params();
    test_bam_conversion_round_trip();
    test_handle_rejects_use_before_init();
    test_handle_overdrive_parity();
    test_handle_rejects_wrong_param_setter();
    test_handle_process_rejects_null_buffers();
    test_handle_echo_parity();
    test_handle_echo_delay_accessor();
    test_handle_echo_delay_accessor_rejects_wrong_type();
    test_handle_echo_delay_accessor_rejects_null_out_ptr();
    test_handle_echo_delay_accessor_rejects_uninitialized_handle();
    test_handle_reset_restores_defaults();

    test_echo_feedback_zero_is_noop();
    test_echo_feedback_decaying_repeats();
    test_echo_feedback_clamps_and_decays();
    test_echo_feedback_reset_clears();
    test_echo_feedback_null_safe();

    test_buf_echo_ring_matches_contiguous_across_offsets();
    test_instance_echo_stateful_delays_prior_input();
    test_instance_echo_stateless_without_state();

    if (failures != 0)
    {
        fprintf(stderr, "DSP tests failed: %d\n", failures);
        return 1;
    }

    puts("DSP tests passed");
    return 0;
}
