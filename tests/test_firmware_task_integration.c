#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "effects_model.h"
#include "effects_task.h"
#include "harness/expect.h"
#include "pot_task.h"
#include "switch_task.h"

int failures = 0;

typedef struct {
    EffectsState effectsState;
    EffectsParams effectsParams;

    bool switches[3];
    uint32_t potSamples[4];
    uint8_t currentPot;

    bool adcReady;
    bool dacReady;
    uint16_t *adcBuffer;
    uint16_t *dacBuffer;

    uint32_t outstandingAllocs;
    uint32_t failureReports;
} IntegrationState;

static bool switch_read_switch(uint8_t index, bool *enabledOut, void *context) {
    IntegrationState *state = (IntegrationState *)context;

    if (state == NULL || enabledOut == NULL || index >= 3) {
        return false;
    }

    *enabledOut = state->switches[index];
    return true;
}

static bool switch_read_state(EffectsState *stateOut, void *context) {
    IntegrationState *state = (IntegrationState *)context;

    if (state == NULL || stateOut == NULL) {
        return false;
    }

    *stateOut = state->effectsState;
    return true;
}

static bool switch_write_state(const EffectsState *stateIn, void *context) {
    IntegrationState *state = (IntegrationState *)context;

    if (state == NULL || stateIn == NULL) {
        return false;
    }

    state->effectsState = *stateIn;
    return true;
}

static void switch_sleep_ms(uint32_t ms, void *context) {
    (void)ms;
    (void)context;
}

static bool pot_start_adc(uint8_t potIndex, void *context) {
    IntegrationState *state = (IntegrationState *)context;

    if (state == NULL || potIndex >= 4) {
        return false;
    }

    state->currentPot = potIndex;
    return true;
}

static bool pot_wait_for_sample(uint32_t timeoutTicks, uint32_t *valueOut, void *context) {
    (void)timeoutTicks;

    IntegrationState *state = (IntegrationState *)context;

    if (state == NULL || valueOut == NULL) {
        return false;
    }

    *valueOut = state->potSamples[state->currentPot];
    return true;
}

static bool pot_read_active_effect(Effect *activeEffect, void *context) {
    IntegrationState *state = (IntegrationState *)context;

    if (state == NULL || activeEffect == NULL) {
        return false;
    }

    EffectsState latchedState = state->effectsState;
    effects_state_normalize(&latchedState);

    return effects_state_get_active_effect(&latchedState, activeEffect);
}

static void pot_apply_sample(Effect activeEffect, uint8_t potIndex, uint32_t adcValue,
                             uint32_t adcMax, void *context) {
    IntegrationState *state = (IntegrationState *)context;

    if (state == NULL) {
        return;
    }

    (void)effects_params_apply_pot_sample(&state->effectsParams, activeEffect, potIndex, adcValue,
                                          adcMax);
}

static bool effects_wait_for_adc(uint32_t timeoutTicks, uint16_t **bufPtr, void *context) {
    (void)timeoutTicks;

    IntegrationState *state = (IntegrationState *)context;

    if (state == NULL || bufPtr == NULL || !state->adcReady) {
        return false;
    }

    state->adcReady = false;
    *bufPtr = state->adcBuffer;
    return true;
}

static bool effects_wait_for_dac(uint32_t timeoutTicks, uint16_t **bufPtr, void *context) {
    (void)timeoutTicks;

    IntegrationState *state = (IntegrationState *)context;

    if (state == NULL || bufPtr == NULL || !state->dacReady) {
        return false;
    }

    state->dacReady = false;
    *bufPtr = state->dacBuffer;
    return true;
}

static bool effects_dma_copy(const uint16_t *src, uint16_t *dst, size_t count,
                             uint32_t timeoutTicks, void *context) {
    (void)timeoutTicks;
    (void)context;

    memmove(dst, src, count * sizeof(uint16_t));
    return true;
}

static void *effects_alloc(size_t size, void *context) {
    IntegrationState *state = (IntegrationState *)context;
    void *ptr = malloc(size);

    if (state != NULL && ptr != NULL) {
        state->outstandingAllocs++;
    }

    return ptr;
}

static void effects_free(void *ptr, void *context) {
    IntegrationState *state = (IntegrationState *)context;

    if (ptr != NULL) {
        free(ptr);

        if (state != NULL) {
            state->outstandingAllocs--;
        }
    }
}

static bool effects_read_latched_state(EffectsState *stateOut, EffectsParams *paramsOut,
                                       void *context) {
    IntegrationState *state = (IntegrationState *)context;

    if (state == NULL || stateOut == NULL || paramsOut == NULL) {
        return false;
    }

    *stateOut = state->effectsState;
    *paramsOut = state->effectsParams;
    return true;
}

static void effects_report_failure(void *context) {
    IntegrationState *state = (IntegrationState *)context;

    if (state != NULL) {
        state->failureReports++;
    }
}

static uint32_t effects_ms_to_ticks(uint32_t ms, void *context) {
    (void)context;
    return ms;
}

static void seed_default_limits(IntegrationState *state) {
    state->effectsParams.overdriveMin = (OverdriveParam){
        .level = 0,
        .gain = 0,
        .tone = MIN_OVERDRIVE_TONE,
        .mix = 0,
    };
    state->effectsParams.overdriveMax = (OverdriveParam){
        .level = MAX_OVERDRIVE_LEVEL,
        .gain = MAX_OVERDRIVE_GAIN,
        .tone = MAX_OVERDRIVE_TONE,
        .mix = MAX_OVERDRIVE_MIX,
    };
    state->effectsParams.echoMin = (EchoParam){
        .pre_delay = MIN_ECHO_PRE_DELAY,
        .density = 5,
        .attack = 7,
        .decay = 3,
    };
    state->effectsParams.echoMax = (EchoParam){
        .pre_delay = MAX_ECHO_PRE_DELAY,
        .density = 25,
        .attack = 27,
        .decay = 23,
    };
}

static void test_integration_switch_pot_and_effects_pipeline(void) {
    IntegrationState state;
    memset(&state, 0, sizeof(state));

    effects_state_set_default_order(&state.effectsState);
    state.effectsState.activeEffectSelection = 1;

    state.switches[0] = false;
    state.switches[1] = true;
    state.switches[2] = false;

    state.potSamples[0] = 255;
    state.potSamples[1] = 192;
    state.potSamples[2] = 64;
    state.potSamples[3] = 32;

    seed_default_limits(&state);

    SwitchTaskContext switchContext = {
        .pollingFrequencyMs = 5,
    };
    SwitchTaskOps switchOps = {
        .read_switch = switch_read_switch,
        .read_state = switch_read_state,
        .write_state = switch_write_state,
        .sleep_ms = switch_sleep_ms,
        .context = &state,
    };

    expect_true(switch_task_step(&switchContext, &switchOps), "switch task step succeeds");
    expect_true(state.effectsState.isEnabled[OVERDRIVE] == false, "switch task disables overdrive");
    expect_true(state.effectsState.isEnabled[ECHO], "switch task enables echo");
    expect_true(state.effectsState.isEnabled[COMPRESSION] == false,
                "switch task disables compression");

    PotTaskContext potContext = {
        .samplingFrequencyTicks = 5,
        .timeoutSlackTicks = 1,
        .adcMax = 255,
        .potCount = 4,
    };
    PotTaskOps potOps = {
        .start_adc = pot_start_adc,
        .wait_for_sample = pot_wait_for_sample,
        .read_active_effect = pot_read_active_effect,
        .apply_pot_sample = pot_apply_sample,
        .context = &state,
    };

    expect_true(pot_task_step(&potContext, &potOps), "pot task step succeeds");
    expect_eq_u32(
        map_adc_to_param(state.potSamples[0], MIN_ECHO_PRE_DELAY, MAX_ECHO_PRE_DELAY, 255),
        state.effectsParams.echo.pre_delay, "pot A updates echo pre-delay");
    expect_eq_u32(map_adc_to_param(state.potSamples[1], state.effectsParams.echoMin.density,
                                   state.effectsParams.echoMax.density, 255),
                  state.effectsParams.echo.density, "pot B updates echo density");
    expect_eq_u32(map_adc_to_param(state.potSamples[2], state.effectsParams.echoMin.attack,
                                   state.effectsParams.echoMax.attack, 255),
                  state.effectsParams.echo.attack, "pot C updates echo attack");
    expect_eq_u32(map_adc_to_param(state.potSamples[3], state.effectsParams.echoMin.decay,
                                   state.effectsParams.echoMax.decay, 255),
                  state.effectsParams.echo.decay, "pot D updates echo decay");

    EffectsPipeline pipeline;
    expect_true(effects_pipeline_init(&pipeline) == 0, "effects pipeline init succeeds");

    uint16_t adcA[8] = {X_AXIS - 20, X_AXIS + 12, X_AXIS + 45, X_AXIS - 30,
                        X_AXIS + 60, X_AXIS - 5,  X_AXIS + 1,  X_AXIS - 1};
    uint16_t adcB[8] = {0};
    uint16_t dacA[8] = {0};
    uint16_t dacB[8] = {0};
    uint16_t delaySamples[16] = {0};

    state.adcReady = true;
    state.dacReady = true;
    state.adcBuffer = adcA;
    state.dacBuffer = dacA;

    EffectsTaskContext effectsContext = {
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
    EffectsTaskOps effectsOps = {
        .wait_for_adc_buffer = effects_wait_for_adc,
        .wait_for_dac_buffer = effects_wait_for_dac,
        .dma_copy = effects_dma_copy,
        .alloc = effects_alloc,
        .free = effects_free,
        .read_latched_state = effects_read_latched_state,
        .report_failure = effects_report_failure,
        .ms_to_ticks = effects_ms_to_ticks,
        .context = &state,
    };

    expect_true(effects_task_step(&effectsContext, &effectsOps), "effects task step succeeds");
    expect_eq_u32(0, state.failureReports, "effects task reports no failures");
    expect_eq_u32(0, state.outstandingAllocs, "effects task frees all temporary allocations");
}

int main(void) {
    test_integration_switch_pot_and_effects_pipeline();

    if (failures != 0) {
        fprintf(stderr, "test_firmware_task_integration: %d failure(s)\n", failures);
        return 1;
    }

    return 0;
}
