#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "effects_task.h"

#include "harness/expect.h"

int failures = 0;

typedef struct
{
    uint16_t *adcBufferToReturn;
    uint16_t *dacBufferToReturn;
    bool waitForAdcSucceeds;
    bool waitForDacSucceeds;

    bool dmaCopySucceeds;

    EffectsState latchedState;
    EffectsParams latchedParams;

    uint32_t failureReports;
    uint32_t frameReports;
    uint32_t outstandingAllocs;

    uint32_t frameBeginCalls;
    uint32_t frameEndCalls;
    uint32_t lastFrameTimeUs;
    bool lastOverrun;
    uint32_t mockTimestampUs;

    bool replaceInputForTestingSucceeds;
    bool replaceInputForTestingCalled;
    uint16_t replaceInputValue;

    bool replaceOutputForTestingSucceeds;
    bool replaceOutputForTestingCalled;
    bool replaceOutputForTestingFail;
    uint16_t replaceOutputValue;
} EffectsTaskTestOpsState;

static bool wait_for_adc(uint32_t timeoutTicks, uint16_t **bufPtr, void *context)
{
    (void)timeoutTicks;

    EffectsTaskTestOpsState *state = (EffectsTaskTestOpsState *)context;
    if (!state->waitForAdcSucceeds || bufPtr == NULL)
    {
        return false;
    }

    *bufPtr = state->adcBufferToReturn;
    return true;
}

static bool wait_for_dac(uint32_t timeoutTicks, uint16_t **bufPtr, void *context)
{
    (void)timeoutTicks;

    EffectsTaskTestOpsState *state = (EffectsTaskTestOpsState *)context;
    if (!state->waitForDacSucceeds || bufPtr == NULL)
    {
        return false;
    }

    *bufPtr = state->dacBufferToReturn;
    return true;
}

static bool dma_copy(const uint16_t *src, uint16_t *dst, size_t count, uint32_t timeoutTicks,
                     void *context)
{
    (void)timeoutTicks;

    EffectsTaskTestOpsState *state = (EffectsTaskTestOpsState *)context;
    if (!state->dmaCopySucceeds)
    {
        return false;
    }

    memmove(dst, src, count * sizeof(uint16_t));
    return true;
}

static void *alloc_buf(size_t size, void *context)
{
    EffectsTaskTestOpsState *state = (EffectsTaskTestOpsState *)context;
    void *ptr = malloc(size);
    if (ptr != NULL)
    {
        state->outstandingAllocs++;
    }

    return ptr;
}

static void free_buf(void *ptr, void *context)
{
    EffectsTaskTestOpsState *state = (EffectsTaskTestOpsState *)context;

    if (ptr != NULL)
    {
        free(ptr);
        state->outstandingAllocs--;
    }
}

static bool read_latched_state(EffectsState *stateOut, EffectsParams *paramsOut, void *context)
{
    EffectsTaskTestOpsState *state = (EffectsTaskTestOpsState *)context;

    if (stateOut == NULL || paramsOut == NULL)
    {
        return false;
    }

    *stateOut = state->latchedState;
    *paramsOut = state->latchedParams;
    return true;
}

static bool replace_input_for_testing(uint16_t *buf, size_t count, void *context)
{
    EffectsTaskTestOpsState *state = (EffectsTaskTestOpsState *)context;
    if (!state->replaceInputForTestingSucceeds)
    {
        return false;
    }

    state->replaceInputForTestingCalled = true;
    for (size_t i = 0; i < count; i++)
    {
        buf[i] = state->replaceInputValue;
    }

    return true;
}

static bool replace_output_for_testing(uint16_t *buf, size_t count, void *context)
{
    EffectsTaskTestOpsState *state = (EffectsTaskTestOpsState *)context;

    if (state->replaceOutputForTestingFail)
    {
        return false;
    }

    if (!state->replaceOutputForTestingSucceeds)
    {
        return true;
    }

    state->replaceOutputForTestingCalled = true;
    for (size_t i = 0; i < count; i++)
    {
        buf[i] = state->replaceOutputValue;
    }

    return true;
}

static void report_failure(void *context)
{
    EffectsTaskTestOpsState *state = (EffectsTaskTestOpsState *)context;
    state->failureReports++;
}

static void report_frame_complete(void *context)
{
    EffectsTaskTestOpsState *state = (EffectsTaskTestOpsState *)context;
    state->frameReports++;
}

static uint32_t ms_to_ticks(uint32_t ms, void *context)
{
    (void)context;
    return ms;
}

static void on_frame_begin(void *context)
{
    EffectsTaskTestOpsState *state = (EffectsTaskTestOpsState *)context;
    state->frameBeginCalls++;
}

static void on_frame_end(uint32_t frame_time_us, bool overrun, void *context)
{
    EffectsTaskTestOpsState *state = (EffectsTaskTestOpsState *)context;
    state->frameEndCalls++;
    state->lastFrameTimeUs = frame_time_us;
    state->lastOverrun = overrun;
}

static uint32_t get_timestamp_us(void *context)
{
    EffectsTaskTestOpsState *state = (EffectsTaskTestOpsState *)context;
    return state->mockTimestampUs;
}

static EffectsTaskOps make_ops(EffectsTaskTestOpsState *state)
{
    EffectsTaskOps ops = {
        .wait_for_adc_buffer = wait_for_adc,
        .wait_for_dac_buffer = wait_for_dac,
        .dma_copy = dma_copy,
        .alloc = alloc_buf,
        .free = free_buf,
        .read_latched_state = read_latched_state,
        .replace_input_for_testing = replace_input_for_testing,
        .replace_output_for_testing = replace_output_for_testing,
        .report_failure = report_failure,
        .report_frame_complete = report_frame_complete,
        .ms_to_ticks = ms_to_ticks,
        .context = state,
    };

    return ops;
}

static void test_effects_task_happy_path_matches_pipeline(void)
{
    EffectsPipeline pipeline;
    expect_true(effects_pipeline_init(&pipeline) == 0, "pipeline init succeeds");

    EffectsPipeline expectedPipeline;
    expect_true(effects_pipeline_init(&expectedPipeline) == 0, "expected pipeline init succeeds");

    uint16_t adcA[8] = {X_AXIS - 20, X_AXIS + 12, X_AXIS + 45, X_AXIS - 30,
                        X_AXIS + 60, X_AXIS - 5, X_AXIS + 1, X_AXIS - 1};
    uint16_t adcB[8] = {0};
    uint16_t dacA[8] = {0};
    uint16_t dacB[8] = {0};
    uint16_t delaySamples[16] = {0};

    EffectsTaskContext taskContext = {
        .pipeline = &pipeline,
        .effectsState = NULL,
        .effectsParams = NULL,
        .adcBufA = adcA,
        .adcBufB = adcB,
        .dacBufA = dacA,
        .dacBufB = dacB,
        .delaySamplesBuf = delaySamples,
        .sampleBufLen = 8,
        .delaySamplesLen = 16,
        .samplingPeriodUs = 25,
        .processingSlackMs = 1,
    };

    EffectsTaskTestOpsState opsState = {0};
    opsState.waitForAdcSucceeds = true;
    opsState.waitForDacSucceeds = true;
    opsState.dmaCopySucceeds = true;
    opsState.replaceInputForTestingSucceeds = true;
    opsState.replaceInputValue = X_AXIS + 100;
    opsState.adcBufferToReturn = adcA;
    opsState.dacBufferToReturn = dacA;

    effects_state_set_default_order(&opsState.latchedState);
    memset(opsState.latchedState.isEnabled, 0, sizeof(opsState.latchedState.isEnabled));
    opsState.latchedState.isEnabled[OVERDRIVE] = true;

    memset(&opsState.latchedParams, 0, sizeof(opsState.latchedParams));
    opsState.latchedParams.overdrive.level = 1200;
    opsState.latchedParams.overdrive.gain = 0x100;
    opsState.latchedParams.overdrive.tone = 0x100;
    opsState.latchedParams.overdrive.mix = 0x80;

    EffectsTaskOps ops = make_ops(&opsState);

    bool stepOk = effects_task_step(&taskContext, &ops);
    expect_true(stepOk, "effects task step succeeds");
    expect_true(opsState.replaceInputForTestingCalled, "test input replacement hook called");
    expect_eq_u32(1, opsState.frameReports, "frame report callback called on success");
    expect_true(opsState.outstandingAllocs == 0, "effects task step frees temporary allocations");

    uint16_t expected[8] = {0};
    expect_true(effects_pipeline_sync_params(&expectedPipeline, &opsState.latchedParams) == 0,
                "expected pipeline sync succeeds");
    uint16_t expectedInput[8] = {0};
    for (size_t i = 0; i < 8; i++)
    {
        expectedInput[i] = opsState.replaceInputValue;
    }

    expect_true(effects_pipeline_process(&expectedPipeline, OVERDRIVE, expectedInput, expected, 8) == 0,
                "expected overdrive process succeeds");

    for (size_t i = 0; i < 8; i++)
    {
        expect_eq_u16(expected[i], dacA[i], "effects output matches expected pipeline output");
    }
}

static void test_effects_task_rejects_stray_adc_notification(void)
{
    EffectsPipeline pipeline;
    expect_true(effects_pipeline_init(&pipeline) == 0,
                "pipeline init for stray notification succeeds");

    uint16_t adcA[4] = {0};
    uint16_t adcB[4] = {0};
    uint16_t dacA[4] = {0};
    uint16_t dacB[4] = {0};
    uint16_t delaySamples[8] = {0};
    uint16_t stray[4] = {0};

    EffectsTaskContext taskContext = {
        .pipeline = &pipeline,
        .effectsState = NULL,
        .effectsParams = NULL,
        .adcBufA = adcA,
        .adcBufB = adcB,
        .dacBufA = dacA,
        .dacBufB = dacB,
        .delaySamplesBuf = delaySamples,
        .sampleBufLen = 4,
        .delaySamplesLen = 8,
        .samplingPeriodUs = 25,
        .processingSlackMs = 1,
    };

    EffectsTaskTestOpsState opsState = {0};
    opsState.waitForAdcSucceeds = true;
    opsState.waitForDacSucceeds = true;
    opsState.dmaCopySucceeds = true;
    opsState.replaceInputForTestingSucceeds = true;
    opsState.adcBufferToReturn = stray;
    opsState.dacBufferToReturn = dacA;

    effects_state_set_default_order(&opsState.latchedState);
    memset(&opsState.latchedParams, 0, sizeof(opsState.latchedParams));

    EffectsTaskOps ops = make_ops(&opsState);

    bool stepOk = effects_task_step(&taskContext, &ops);
    expect_true(!stepOk, "effects task rejects stray adc notification value");
    expect_true(opsState.outstandingAllocs == 0, "no allocations leaked on stray notification");
}

static void test_effects_task_timing_callbacks_fire(void)
{
    EffectsPipeline pipeline;
    expect_true(effects_pipeline_init(&pipeline) == 0, "pipeline init for timing test");

    uint16_t adcA[4] = {X_AXIS};
    uint16_t adcB[4] = {0};
    uint16_t dacA[4] = {0};
    uint16_t dacB[4] = {0};
    uint16_t delaySamples[8] = {0};

    EffectsTaskContext taskContext = {
        .pipeline = &pipeline,
        .effectsState = NULL,
        .effectsParams = NULL,
        .adcBufA = adcA,
        .adcBufB = adcB,
        .dacBufA = dacA,
        .dacBufB = dacB,
        .delaySamplesBuf = delaySamples,
        .sampleBufLen = 4,
        .delaySamplesLen = 8,
        .samplingPeriodUs = 25,
        .processingSlackMs = 1,
    };

    EffectsTaskTestOpsState opsState = {0};
    opsState.waitForAdcSucceeds = true;
    opsState.waitForDacSucceeds = true;
    opsState.dmaCopySucceeds = true;
    opsState.replaceInputForTestingSucceeds = true;
    opsState.adcBufferToReturn = adcA;
    opsState.dacBufferToReturn = dacA;
    opsState.mockTimestampUs = 1000;
    effects_state_set_default_order(&opsState.latchedState);
    memset(opsState.latchedState.isEnabled, 0, sizeof(opsState.latchedState.isEnabled));
    opsState.latchedState.isEnabled[OVERDRIVE] = true;
    memset(&opsState.latchedParams, 0, sizeof(opsState.latchedParams));
    opsState.latchedParams.overdrive.level = 1200;
    opsState.latchedParams.overdrive.gain = 0x100;
    opsState.latchedParams.overdrive.tone = 0x100;
    opsState.latchedParams.overdrive.mix = 0x80;

    EffectsTaskOps ops = make_ops(&opsState);
    ops.on_frame_begin = on_frame_begin;
    ops.on_frame_end = on_frame_end;
    ops.get_timestamp_us = get_timestamp_us;

    bool stepOk = effects_task_step(&taskContext, &ops);
    expect_true(stepOk, "timing test step succeeds");
    expect_eq_u32(1, opsState.frameBeginCalls, "on_frame_begin called once");
    expect_eq_u32(1, opsState.frameEndCalls, "on_frame_end called once");
    expect_false(opsState.lastOverrun, "no overrun for fast frame");
    expect_true(opsState.outstandingAllocs == 0, "no leaks");
}

static void test_effects_task_output_replacement_overwrites_dac_buffer(void)
{
    EffectsPipeline pipeline;
    expect_true(effects_pipeline_init(&pipeline) == 0, "pipeline init succeeds");

    uint16_t adcA[4] = {X_AXIS, X_AXIS, X_AXIS, X_AXIS};
    uint16_t adcB[4] = {0};
    uint16_t dacA[4] = {0};
    uint16_t dacB[4] = {0};
    uint16_t delaySamples[8] = {0};

    EffectsTaskContext taskContext = {
        .pipeline = &pipeline,
        .effectsState = NULL,
        .effectsParams = NULL,
        .adcBufA = adcA,
        .adcBufB = adcB,
        .dacBufA = dacA,
        .dacBufB = dacB,
        .delaySamplesBuf = delaySamples,
        .sampleBufLen = 4,
        .delaySamplesLen = 8,
        .samplingPeriodUs = 25,
        .processingSlackMs = 1,
    };

    EffectsTaskTestOpsState opsState = {0};
    opsState.waitForAdcSucceeds = true;
    opsState.waitForDacSucceeds = true;
    opsState.dmaCopySucceeds = true;
    opsState.replaceInputForTestingSucceeds = true;
    opsState.replaceOutputForTestingSucceeds = true;
    opsState.replaceOutputValue = X_AXIS + 200;
    opsState.adcBufferToReturn = adcA;
    opsState.dacBufferToReturn = dacA;

    effects_state_set_default_order(&opsState.latchedState);
    memset(opsState.latchedState.isEnabled, 0, sizeof(opsState.latchedState.isEnabled));
    memset(&opsState.latchedParams, 0, sizeof(opsState.latchedParams));

    EffectsTaskOps ops = make_ops(&opsState);

    bool stepOk = effects_task_step(&taskContext, &ops);
    expect_true(stepOk, "effects task step succeeds with output replacement");
    expect_true(opsState.replaceOutputForTestingCalled, "output replacement hook called");
    expect_eq_u32(1, opsState.frameReports, "frame report callback called on success");

    for (size_t i = 0; i < 4; i++)
    {
        expect_eq_u16(X_AXIS + 200, dacA[i], "dac buffer overwritten by output replacement");
    }
}

static void test_effects_task_reports_failure_when_test_replace_fails(void)
{
    EffectsPipeline pipeline;
    expect_true(effects_pipeline_init(&pipeline) == 0, "pipeline init succeeds");

    uint16_t adcA[4] = {X_AXIS};
    uint16_t adcB[4] = {0};
    uint16_t dacA[4] = {0};
    uint16_t dacB[4] = {0};
    uint16_t delaySamples[8] = {0};

    EffectsTaskContext taskContext = {
        .pipeline = &pipeline,
        .effectsState = NULL,
        .effectsParams = NULL,
        .adcBufA = adcA,
        .adcBufB = adcB,
        .dacBufA = dacA,
        .dacBufB = dacB,
        .delaySamplesBuf = delaySamples,
        .sampleBufLen = 4,
        .delaySamplesLen = 8,
        .samplingPeriodUs = 25,
        .processingSlackMs = 1,
    };

    EffectsTaskTestOpsState opsState = {0};
    opsState.waitForAdcSucceeds = true;
    opsState.waitForDacSucceeds = true;
    opsState.dmaCopySucceeds = true;
    opsState.replaceInputForTestingSucceeds = false;
    opsState.adcBufferToReturn = adcA;
    opsState.dacBufferToReturn = dacA;

    effects_state_set_default_order(&opsState.latchedState);
    memset(&opsState.latchedParams, 0, sizeof(opsState.latchedParams));

    EffectsTaskOps ops = make_ops(&opsState);

    bool stepOk = effects_task_step(&taskContext, &ops);
    expect_false(stepOk, "effects task fails when test replacement fails");
    expect_eq_u32(0, opsState.frameReports, "frame report not called on failure");
    expect_eq_u32(0, opsState.failureReports,
                  "replacement hook failure exits early without processing-failure callback");
}

static void test_effects_task_uses_matching_dac_buffer_when_wait_times_out(void)
{
    EffectsPipeline pipeline;
    expect_true(effects_pipeline_init(&pipeline) == 0, "pipeline init succeeds");

    EffectsPipeline expectedPipeline;
    expect_true(effects_pipeline_init(&expectedPipeline) == 0, "expected pipeline init succeeds");

    uint16_t adcA[8] = {0};
    uint16_t adcB[8] = {X_AXIS - 20, X_AXIS + 12, X_AXIS + 45, X_AXIS - 30,
                        X_AXIS + 60, X_AXIS - 5, X_AXIS + 1, X_AXIS - 1};
    uint16_t dacA[8] = {0};
    uint16_t dacB[8] = {0};
    uint16_t delaySamples[16] = {0};

    EffectsTaskContext taskContext = {
        .pipeline = &pipeline,
        .effectsState = NULL,
        .effectsParams = NULL,
        .adcBufA = adcA,
        .adcBufB = adcB,
        .dacBufA = dacA,
        .dacBufB = dacB,
        .delaySamplesBuf = delaySamples,
        .sampleBufLen = 8,
        .delaySamplesLen = 16,
        .samplingPeriodUs = 25,
        .processingSlackMs = 1,
    };

    EffectsTaskTestOpsState opsState = {0};
    opsState.waitForAdcSucceeds = true;
    opsState.waitForDacSucceeds = false;
    opsState.dmaCopySucceeds = true;
    opsState.replaceInputForTestingSucceeds = true;
    opsState.replaceInputValue = X_AXIS + 100;
    opsState.adcBufferToReturn = adcB;
    opsState.dacBufferToReturn = dacA;

    effects_state_set_default_order(&opsState.latchedState);
    memset(opsState.latchedState.isEnabled, 0, sizeof(opsState.latchedState.isEnabled));
    opsState.latchedState.isEnabled[OVERDRIVE] = true;

    memset(&opsState.latchedParams, 0, sizeof(opsState.latchedParams));
    opsState.latchedParams.overdrive.level = 1200;
    opsState.latchedParams.overdrive.gain = 0x100;
    opsState.latchedParams.overdrive.tone = 0x100;
    opsState.latchedParams.overdrive.mix = 0x80;

    EffectsTaskOps ops = make_ops(&opsState);

    bool stepOk = effects_task_step(&taskContext, &ops);
    expect_true(stepOk, "effects task step succeeds when dac wait times out");
    expect_eq_u32(1, opsState.frameReports, "frame report callback called on fallback success");

    uint16_t expected[8] = {0};
    expect_true(effects_pipeline_sync_params(&expectedPipeline, &opsState.latchedParams) == 0,
                "expected pipeline sync succeeds");
    uint16_t expectedInput[8] = {0};
    for (size_t i = 0; i < 8; i++)
    {
        expectedInput[i] = opsState.replaceInputValue;
    }
    expect_true(effects_pipeline_process(&expectedPipeline, OVERDRIVE, expectedInput, expected, 8) == 0,
                "expected overdrive process succeeds");

    for (size_t i = 0; i < 8; i++)
    {
        expect_eq_u16(expected[i], dacB[i], "fallback writes to matching dac buffer");
    }
}

int main(void)
{
    test_effects_task_happy_path_matches_pipeline();
    test_effects_task_rejects_stray_adc_notification();
    test_effects_task_timing_callbacks_fire();
    test_effects_task_reports_failure_when_test_replace_fails();
    test_effects_task_output_replacement_overwrites_dac_buffer();
    test_effects_task_uses_matching_dac_buffer_when_wait_times_out();

    if (failures != 0)
    {
        fprintf(stderr, "test_firmware_effects_task: %d failure(s)\n", failures);
        return 1;
    }

    return 0;
}
