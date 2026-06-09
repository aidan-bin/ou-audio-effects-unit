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

    expect_true(effects_pipeline_process(&pipeline, OVERDRIVE, input, output, 4) == 0,
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

    expect_true(effects_pipeline_process(&pipeline, ECHO, echo_input, output, 4) == 0,
                "pipeline echo process succeeds");
    buf_echo(echo_input, expected, 4, &params.echo);

    for (size_t i = 0; i < 4; i++)
    {
        expect_eq_u16(expected[i], output[i], "echo output matches runtime");
    }
}

int main(void)
{
    test_pipeline_init_and_sync_params();
    test_pipeline_process_matches_runtime_helpers();

    if (failures != 0)
    {
        fprintf(stderr, "Firmware effects pipeline tests failed: %d\n", failures);
        return 1;
    }

    puts("Firmware effects pipeline tests passed");
    return 0;
}