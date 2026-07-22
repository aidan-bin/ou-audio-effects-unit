#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "effects.h"
#include "effects_pipeline.h"

#include "harness/expect.h"

int failures = 0;

static void test_pipeline_init_and_sync_params(void)
{
    EffectsPipeline pipeline;
    memset(&pipeline, 0, sizeof(pipeline));

    expect_true(effects_pipeline_init(&pipeline) == 0, "pipeline init succeeds");

    EffectsParams params;
    memset(&params, 0, sizeof(params));
    params.echo.delay_samples = 7;
    params.echo.pre_delay = 3;
    params.echo.density = 16;
    params.echo.attack = 8;
    params.echo.decay = 1;

    expect_true(effects_pipeline_sync_params(&pipeline, &params) == 0, "pipeline sync succeeds");

    size_t delay_samples = 0;
    expect_true(effects_pipeline_get_echo_delay_samples(&pipeline, &delay_samples) == 0,
                "pipeline get echo delay succeeds");
    expect_eq_size(7, delay_samples, "echo delay mirrors params");
}

static void test_pipeline_process_matches_runtime_helpers(void)
{
    EffectsPipeline pipeline;
    memset(&pipeline, 0, sizeof(pipeline));
    expect_true(effects_pipeline_init(&pipeline) == 0, "pipeline init for process succeeds");

    EffectsParams params;
    memset(&params, 0, sizeof(params));
    params.overdrive.level = 1234;
    params.overdrive.gain = 0x100;
    params.overdrive.tone = 0x100;
    params.overdrive.mix = 0x80;

    params.compression.threshold = X_AXIS + 100;
    params.compression.ratio = 0x80;

    params.echo.delay_samples = 4;
    params.echo.pre_delay = 1;
    params.echo.density = 0x100;
    params.echo.attack = 0x100;
    params.echo.decay = 0;

    expect_true(effects_pipeline_sync_params(&pipeline, &params) == 0,
                "pipeline sync for process succeeds");

    uint16_t input[4] = {X_AXIS - 120, X_AXIS - 10, X_AXIS + 30, X_AXIS + 80};
    uint16_t output[4] = {0};
    uint16_t expected[4] = {0};

    expect_true(effects_pipeline_process(&pipeline, EFFECT_TYPE_OVERDRIVE, input, output, 4) == 0,
                "pipeline overdrive process succeeds");
    buf_overdrive(input, expected, 4, &params.overdrive);

    for (size_t i = 0; i < 4; i++)
    {
        expect_eq_u16(expected[i], output[i], "overdrive output matches runtime");
    }

    uint16_t echo_input[8] = {0};
    memcpy(&echo_input[4], input, sizeof(input));
    memset(output, 0, sizeof(output));
    memset(expected, 0, sizeof(expected));

    expect_true(effects_pipeline_process(&pipeline, EFFECT_TYPE_ECHO, echo_input, output, 4) == 0,
                "pipeline echo process succeeds");
    buf_echo(echo_input, expected, 4, &params.echo);

    for (size_t i = 0; i < 4; i++)
    {
        expect_eq_u16(expected[i], output[i], "echo output matches runtime");
    }
}

static void test_pipeline_echo_state_attach_and_reset(void)
{
    EffectsPipeline pipeline;
    memset(&pipeline, 0, sizeof(pipeline));
    expect_true(effects_pipeline_init(&pipeline) == 0, "pipeline init for echo state succeeds");

    uint16_t history[64];
    uint16_t fb_line[32];
    EchoState st;
    st.history = history;
    st.history_len = sizeof(history) / sizeof(history[0]);
    st.fb.line = fb_line;
    st.fb.line_len = sizeof(fb_line) / sizeof(fb_line[0]);
    st.prev_enabled = 0;
    echo_state_reset(&st);

    expect_true(effects_pipeline_attach_echo_state(&pipeline, &st) == 0, "attach echo state");

    EffectsParams params;
    memset(&params, 0, sizeof(params));
    params.echo.delay_samples = 4;
    params.echo.pre_delay = 1;
    params.echo.density = 256;
    params.echo.attack = 256;
    params.echo.decay = 0;
    expect_true(effects_pipeline_sync_params(&pipeline, &params) == 0, "sync echo params");

    EffectsState enabled;
    effects_state_set_default_slots(&enabled);
    memset(enabled.slot_enabled, 0, sizeof(enabled.slot_enabled));
    enabled.slot_enabled[1] = true; // echo occupies slot 1 by default

    expect_true(effects_pipeline_apply_enabled(&pipeline, &enabled) == 0, "apply enabled (edge)");
    uint16_t loud[4];
    uint16_t out[4] = {0};
    for (size_t i = 0; i < 4; i++)
    {
        loud[i] = (uint16_t)(X_AXIS + 800);
    }
    expect_true(effects_pipeline_process(&pipeline, EFFECT_TYPE_ECHO, loud, out, 4) == 0, "process loud frame");

    bool history_has_energy = false;
    for (size_t i = 0; i < st.history_len; i++)
    {
        if (history[i] != (uint16_t)X_AXIS)
        {
            history_has_energy = true;
        }
    }
    expect_true(history_has_energy, "echo history captured the input frame");

    EffectsState disabled = enabled;
    disabled.slot_enabled[1] = false;
    expect_true(effects_pipeline_apply_enabled(&pipeline, &disabled) == 0, "apply disabled");
    expect_true(effects_pipeline_apply_enabled(&pipeline, &enabled) == 0, "apply re-enabled");

    bool cleared = true;
    for (size_t i = 0; i < st.history_len; i++)
    {
        if (history[i] != (uint16_t)X_AXIS)
        {
            cleared = false;
        }
    }
    expect_true(cleared, "re-enable edge clears stale echo history");
}

static void test_pipeline_process_compression_and_bounds(void)
{
    EffectsPipeline pipeline;
    memset(&pipeline, 0, sizeof(pipeline));
    expect_true(effects_pipeline_init(&pipeline) == 0, "pipeline init for compression succeeds");

    EffectsParams params;
    memset(&params, 0, sizeof(params));
    params.compression.threshold = X_AXIS / 2;
    params.compression.ratio = 0x80;
    expect_true(effects_pipeline_sync_params(&pipeline, &params) == 0, "sync compression params");

    uint16_t input[4] = {X_AXIS - 4000, X_AXIS - 100, X_AXIS + 2000, X_AXIS + 6000};
    uint16_t output[4] = {0};
    uint16_t expected[4] = {0};

    expect_true(effects_pipeline_process(&pipeline, EFFECT_TYPE_COMPRESSION, input, output, 4) == 0,
                "pipeline compression process succeeds");
    buf_compression(input, expected, 4, &params.compression);
    for (size_t i = 0; i < 4; i++)
    {
        expect_eq_u16(expected[i], output[i], "compression output matches runtime");
    }

    // Out-of-range effect index is rejected.
    expect_true(effects_pipeline_process(&pipeline, (EffectType)NUM_EFFECTS, input, output, 4) != 0,
                "process rejects out-of-range effect");
    expect_true(effects_pipeline_reset_state(&pipeline, (EffectType)NUM_EFFECTS) != 0,
                "reset rejects out-of-range effect");
}

int main(void)
{
    test_pipeline_init_and_sync_params();
    test_pipeline_process_matches_runtime_helpers();
    test_pipeline_process_compression_and_bounds();
    test_pipeline_echo_state_attach_and_reset();

    if (failures != 0)
    {
        fprintf(stderr, "Firmware effects pipeline tests failed: %d\n", failures);
        return 1;
    }

    puts("Firmware effects pipeline tests passed");
    return 0;
}