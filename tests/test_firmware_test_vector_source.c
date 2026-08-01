#include <stdbool.h>
#include <stdint.h>
#include <math.h>

#include "test_vector_source.h"
#include "cli_core.h"
#include "effects.h"
#include "harness/expect.h"

int failures = 0;

#define SWEEP_COUNT 256U
#define SWEEP_SAMPLE_RATE 48000U

static void test_init_sets_default_sample_rate(void)
{
    TestVectorSource src;
    test_vector_source_init(&src, 0U);
    expect_eq_u32(SWEEP_SAMPLE_RATE, src.sample_rate_hz, "default sample rate");
    expect_eq_u32(0U, src.phase_q16, "phase starts at 0");
    expect_eq_u32(20U, src.sweep_freq_hz, "sweep starts at 20 Hz");
}

static void test_init_respects_custom_sample_rate(void)
{
    TestVectorSource src;
    test_vector_source_init(&src, 48000U);
    expect_eq_u32(48000U, src.sample_rate_hz, "custom sample rate");
}

static void test_fill_null_rejected(void)
{
    TestVectorSource src;
    test_vector_source_init(&src, SWEEP_SAMPLE_RATE);

    CliTestModeStatus status = {.enabled = true, .vector = TEST_VECTOR_SINE};
    uint16_t buf[4] = {0};

    expect_false(test_vector_source_fill_buffer(NULL, &status, buf, 4),
                 "null source rejected");
    expect_false(test_vector_source_fill_buffer(&src, NULL, buf, 4),
                 "null status rejected");
    expect_false(test_vector_source_fill_buffer(&src, &status, NULL, 4),
                 "null buf rejected");
}

static void test_fill_disabled_returns_true_no_writes(void)
{
    TestVectorSource src;
    test_vector_source_init(&src, SWEEP_SAMPLE_RATE);

    CliTestModeStatus status = {.enabled = false};
    uint16_t buf[4] = {0xABCD, 0xABCD, 0xABCD, 0xABCD};

    expect_true(test_vector_source_fill_buffer(&src, &status, buf, 4),
                "disabled returns true");
    expect_eq_u16(0xABCD, buf[0], "buffer unchanged when disabled");
    expect_eq_u16(0xABCD, buf[1], "buffer unchanged when disabled");
    expect_eq_u16(0xABCD, buf[2], "buffer unchanged when disabled");
    expect_eq_u16(0xABCD, buf[3], "buffer unchanged when disabled");
}

static void test_sine_rms_consistent_across_frequencies(void)
{
    TestVectorSource src;
    test_vector_source_init(&src, SWEEP_SAMPLE_RATE);

    uint16_t buf[SWEEP_COUNT];

    static const uint16_t test_freqs[] = {100U, 500U, 1000U, 5000U,
                                          10000U, 15000U, 18000U, 20000U};
    static const size_t n_freqs = sizeof(test_freqs) / sizeof(test_freqs[0]);
    double freq_rms[n_freqs];

    for (size_t f = 0; f < n_freqs; f++)
    {
        CliTestModeStatus status = {
            .enabled = true,
            .vector = TEST_VECTOR_SINE,
            .frequency_hz = test_freqs[f],
            .amplitude = INT16_MAX,
        };

        src.phase_q16 = 0U;
        test_vector_source_fill_buffer(&src, &status, buf, SWEEP_COUNT);

        int64_t sum_sq = 0;
        for (size_t i = 0; i < SWEEP_COUNT; i++)
        {
            int32_t centered = (int32_t)buf[i] - (int32_t)X_AXIS;
            sum_sq += (int64_t)centered * (int64_t)centered;
        }
        freq_rms[f] = sqrt((double)sum_sq / (double)SWEEP_COUNT);
    }

    double ref = freq_rms[0];
    for (size_t f = 1; f < n_freqs; f++)
    {
        double ratio = freq_rms[f] / ref;
        char label[80];
        (void)snprintf(label, sizeof(label),
                       "sine rms ratio %.3f at %u Hz", ratio, test_freqs[f]);
        expect_true(ratio > 0.85, label);
    }
}

static void test_sweep_rms_consistent_across_bands(void)
{
    TestVectorSource src;
    test_vector_source_init(&src, SWEEP_SAMPLE_RATE);

    CliTestModeStatus status = {
        .enabled = true,
        .vector = TEST_VECTOR_SWEEP,
        .frequency_hz = 0,
        .amplitude = INT16_MAX,
    };

    uint16_t buf[SWEEP_COUNT];

    uint32_t sweep_max_hz = SWEEP_SAMPLE_RATE / 2U;

    uint32_t skip_above_hz = 1000U;
    uint32_t sweep_step = (sweep_max_hz - 20U) * (uint64_t)SWEEP_COUNT / SWEEP_SAMPLE_RATE;
    if (sweep_step == 0U)
    {
        sweep_step = 1U;
    }

    uint32_t skip_bufs = (skip_above_hz - 20U) / sweep_step;
    for (uint32_t i = 0; i < skip_bufs; i++)
    {
        test_vector_source_fill_buffer(&src, &status, buf, SWEEP_COUNT);
    }

    uint32_t start_hz = 20U + skip_bufs * sweep_step;

    uint32_t band_min = start_hz;
    uint32_t band_max = sweep_max_hz;
    uint32_t band_width = (band_max - band_min) / 4U;

    double band_rms[4] = {0.0};
    size_t band_counts[4] = {0};

    while (src.sweep_freq_hz >= band_min && src.sweep_freq_hz < band_max)
    {
        uint32_t freq = src.sweep_freq_hz;
        test_vector_source_fill_buffer(&src, &status, buf, SWEEP_COUNT);

        int64_t sum_sq = 0;
        for (size_t i = 0; i < SWEEP_COUNT; i++)
        {
            int32_t centered = (int32_t)buf[i] - (int32_t)X_AXIS;
            sum_sq += (int64_t)centered * (int64_t)centered;
        }
        double rms = sqrt((double)sum_sq / (double)SWEEP_COUNT);

        int band = (int)((freq - band_min) / band_width);
        if (band >= 0 && band < 4)
        {
            band_rms[band] += rms;
            band_counts[band]++;
        }
    }

    for (int b = 0; b < 4; b++)
    {
        if (band_counts[b] > 0)
        {
            band_rms[b] /= (double)band_counts[b];
        }
        char label[48];
        (void)snprintf(label, sizeof(label),
                       "sweep band %d has samples", b);
        expect_true(band_counts[b] > 10, label);
    }

    double ref = band_rms[0];
    for (int b = 1; b < 4; b++)
    {
        double ratio = ref > 0.0 ? band_rms[b] / ref : 0.0;
        char label[80];
        (void)snprintf(label, sizeof(label),
                       "sweep band %d rms ratio %.3f", b, ratio);
        expect_true(ratio > 0.85, label);
    }
}

static void test_impulse_one_shot_produces_single_pulse(void)
{
    TestVectorSource src;
    test_vector_source_init(&src, SWEEP_SAMPLE_RATE);

    CliTestModeStatus status = {.enabled = true, .vector = TEST_VECTOR_IMPULSE, .frequency_hz = 0, .amplitude = INT16_MAX};
    uint16_t buf[4] = {0};

    expect_true(test_vector_source_fill_buffer(&src, &status, buf, 4),
                "impulse one-shot returns true");
    expect_eq_u16((uint16_t)(X_AXIS + INT16_MAX), buf[0],
                  "impulse at first sample has max amplitude");

    src.phase_q16 = 0;
    expect_true(test_vector_source_fill_buffer(&src, &status, buf, 4),
                "impulse second call returns true");
    expect_eq_u16((uint16_t)X_AXIS, buf[0],
                  "impulse second call is silent");
    expect_eq_u16((uint16_t)X_AXIS, buf[1],
                  "impulse second call is silent");
}

static void test_impulse_periodic(void)
{
    TestVectorSource src;
    test_vector_source_init(&src, SWEEP_SAMPLE_RATE);

    CliTestModeStatus status = {.enabled = true, .vector = TEST_VECTOR_IMPULSE, .frequency_hz = SWEEP_SAMPLE_RATE / 4U, .amplitude = INT16_MAX};
    uint16_t buf[8] = {0};

    expect_true(test_vector_source_fill_buffer(&src, &status, buf, 8),
                "impulse periodic returns true");

    int impulse_count = 0;
    for (size_t i = 0; i < 8; i++)
    {
        if (buf[i] != (uint16_t)X_AXIS)
        {
            impulse_count++;
        }
    }
    expect_true(impulse_count >= 1, "impulse periodic produced at least one pulse");
}

static bool test_stream_pop(int16_t *sample, void *context)
{
    (void)context;
    *sample = 1000;
    return true;
}

static void test_usb_stream_fills_from_callback(void)
{
    TestVectorSource src;
    test_vector_source_init(&src, SWEEP_SAMPLE_RATE);

    src.stream_pop = test_stream_pop;
    src.stream_context = NULL;

    CliTestModeStatus status = {.enabled = true, .vector = TEST_VECTOR_USB_STREAM, .amplitude = INT16_MAX};
    uint16_t buf[4] = {0};

    expect_true(test_vector_source_fill_buffer(&src, &status, buf, 4),
                "usb stream fill returns true");
    expect_true(buf[0] >= (uint16_t)X_AXIS,
                "usb stream sample is at or above x-axis");
    expect_true(buf[0] <= (uint16_t)X_AXIS + 2000,
                "usb stream sample is within expected range");
}

static void test_usb_stream_null_callback_fails(void)
{
    TestVectorSource src;
    test_vector_source_init(&src, SWEEP_SAMPLE_RATE);

    src.stream_pop = NULL;
    src.stream_context = NULL;

    CliTestModeStatus status = {.enabled = true, .vector = TEST_VECTOR_USB_STREAM, .amplitude = INT16_MAX};
    uint16_t buf[4] = {0xABCD, 0xABCD, 0xABCD, 0xABCD};

    expect_false(test_vector_source_fill_buffer(&src, &status, buf, 4),
                 "usb stream with null callback fails");
}

int main(void)
{
    test_init_sets_default_sample_rate();
    test_init_respects_custom_sample_rate();
    test_fill_null_rejected();
    test_fill_disabled_returns_true_no_writes();
    test_sine_rms_consistent_across_frequencies();
    test_sweep_rms_consistent_across_bands();
    test_impulse_one_shot_produces_single_pulse();
    test_impulse_periodic();
    test_usb_stream_fills_from_callback();
    test_usb_stream_null_callback_fails();

    if (failures != 0)
    {
        fprintf(stderr, "test_vector_source tests failed: %d\n", failures);
        return 1;
    }

    puts("test_vector_source tests passed");
    return 0;
}
