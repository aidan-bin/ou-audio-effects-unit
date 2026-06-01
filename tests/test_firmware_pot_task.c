#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "pot_task.h"

static int failures = 0;

typedef struct
{
    bool startSucceeds[4];
    bool waitSucceeds[4];
    uint32_t samples[4];

    bool activeEffectValid[4];
    Effect activeEffects[4];

    uint8_t currentPot;

    uint32_t startCalls;
    uint32_t waitCalls;
    uint32_t applyCalls;

    uint8_t appliedPots[4];
    Effect appliedEffects[4];
    uint32_t appliedSamples[4];
    uint32_t appliedAdcMax[4];
} PotTaskTestState;

static void expect_true(bool condition, const char *label)
{
    if (!condition)
    {
        fprintf(stderr, "FAIL: %s\n", label);
        failures++;
    }
}

static void expect_eq_u32(uint32_t expected, uint32_t actual, const char *label)
{
    if (expected != actual)
    {
        fprintf(stderr, "FAIL: %s expected=%u actual=%u\n", label, expected, actual);
        failures++;
    }
}

static bool start_adc(uint8_t potIndex, void *context)
{
    PotTaskTestState *state = (PotTaskTestState *)context;
    state->currentPot = potIndex;
    state->startCalls++;
    return state->startSucceeds[potIndex];
}

static bool wait_for_sample(uint32_t timeoutTicks, uint32_t *valueOut, void *context)
{
    (void)timeoutTicks;

    PotTaskTestState *state = (PotTaskTestState *)context;
    state->waitCalls++;

    if (!state->waitSucceeds[state->currentPot] || valueOut == NULL)
    {
        return false;
    }

    *valueOut = state->samples[state->currentPot];
    return true;
}

static bool read_active_effect(Effect *activeEffect, void *context)
{
    PotTaskTestState *state = (PotTaskTestState *)context;

    if (activeEffect == NULL || !state->activeEffectValid[state->currentPot])
    {
        return false;
    }

    *activeEffect = state->activeEffects[state->currentPot];
    return true;
}

static void apply_pot_sample(Effect activeEffect, uint8_t potIndex, uint32_t adcValue, uint32_t adcMax,
    void *context)
{
    PotTaskTestState *state = (PotTaskTestState *)context;
    uint32_t i = state->applyCalls;

    if (i < 4)
    {
        state->appliedEffects[i] = activeEffect;
        state->appliedPots[i] = potIndex;
        state->appliedSamples[i] = adcValue;
        state->appliedAdcMax[i] = adcMax;
    }

    state->applyCalls++;
}

static PotTaskOps make_ops(PotTaskTestState *state)
{
    PotTaskOps ops = {
        .start_adc = start_adc,
        .wait_for_sample = wait_for_sample,
        .read_active_effect = read_active_effect,
        .apply_pot_sample = apply_pot_sample,
        .context = state,
    };

    return ops;
}

static PotTaskContext make_context(void)
{
    PotTaskContext context = {
        .samplingFrequencyTicks = 5,
        .timeoutSlackTicks = 1,
        .adcMax = 255,
        .potCount = 4,
    };

    return context;
}

static void init_state(PotTaskTestState *state)
{
    memset(state, 0, sizeof(*state));

    for (int i = 0; i < 4; i++)
    {
        state->startSucceeds[i] = true;
        state->waitSucceeds[i] = true;
        state->samples[i] = (uint32_t)(10 + i);
        state->activeEffectValid[i] = true;
        state->activeEffects[i] = OVERDRIVE;
    }
}

static void test_four_channel_sweep_updates_active_effect(void)
{
    PotTaskTestState state;
    init_state(&state);

    PotTaskContext context = make_context();
    PotTaskOps ops = make_ops(&state);

    bool ok = pot_task_step(&context, &ops);
    expect_true(ok, "pot step succeeds");
    expect_eq_u32(4, state.applyCalls, "apply called for each pot");

    for (uint8_t i = 0; i < 4; i++)
    {
        expect_eq_u32(i, state.appliedPots[i], "pot index propagated");
        expect_eq_u32(state.samples[i], state.appliedSamples[i], "sample propagated");
        expect_eq_u32(255, state.appliedAdcMax[i], "adc max propagated");
    }
}

static void test_inactive_effect_is_ignored(void)
{
    PotTaskTestState state;
    init_state(&state);
    state.activeEffectValid[2] = false;

    PotTaskContext context = make_context();
    PotTaskOps ops = make_ops(&state);

    bool ok = pot_task_step(&context, &ops);
    expect_true(ok, "pot step succeeds when one active effect read is invalid");
    expect_eq_u32(3, state.applyCalls, "invalid active effect skips apply");
}

static void test_adc_start_failure_skips_channel(void)
{
    PotTaskTestState state;
    init_state(&state);
    state.startSucceeds[1] = false;

    PotTaskContext context = make_context();
    PotTaskOps ops = make_ops(&state);

    bool ok = pot_task_step(&context, &ops);
    expect_true(ok, "pot step succeeds when one adc start fails");
    expect_eq_u32(3, state.applyCalls, "start failure skips apply for that pot");
}

static void test_notify_timeout_skips_channel(void)
{
    PotTaskTestState state;
    init_state(&state);
    state.waitSucceeds[3] = false;

    PotTaskContext context = make_context();
    PotTaskOps ops = make_ops(&state);

    bool ok = pot_task_step(&context, &ops);
    expect_true(ok, "pot step succeeds when one sample wait times out");
    expect_eq_u32(3, state.applyCalls, "wait timeout skips apply for that pot");
}

int main(void)
{
    test_four_channel_sweep_updates_active_effect();
    test_inactive_effect_is_ignored();
    test_adc_start_failure_skips_channel();
    test_notify_timeout_skips_channel();

    if (failures != 0)
    {
        fprintf(stderr, "test_firmware_pot_task: %d failure(s)\n", failures);
        return 1;
    }

    return 0;
}
