#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "switch_task.h"
#include "harness/expect.h"

int failures = 0;

typedef struct {
    bool switchValues[3];
    EffectsState sourceState;

    bool readSwitchSucceeds;
    bool readStateSucceeds;
    bool writeStateSucceeds;

    uint32_t writeCalls;
    EffectsState writtenState;

    uint32_t sleepCalls;
    uint32_t lastSleepMs;
} SwitchTaskTestState;

static bool read_switch(uint8_t index, bool *enabledOut, void *context) {
    SwitchTaskTestState *state = (SwitchTaskTestState *)context;

    if (!state->readSwitchSucceeds || enabledOut == NULL || index >= 3) {
        return false;
    }

    *enabledOut = state->switchValues[index];
    return true;
}

static bool read_state(EffectsState *stateOut, void *context) {
    SwitchTaskTestState *state = (SwitchTaskTestState *)context;

    if (!state->readStateSucceeds || stateOut == NULL) {
        return false;
    }

    *stateOut = state->sourceState;
    return true;
}

static bool write_state(const EffectsState *stateIn, void *context) {
    SwitchTaskTestState *state = (SwitchTaskTestState *)context;

    if (!state->writeStateSucceeds || stateIn == NULL) {
        return false;
    }

    state->writeCalls++;
    state->writtenState = *stateIn;
    return true;
}

static void sleep_ms(uint32_t ms, void *context) {
    SwitchTaskTestState *state = (SwitchTaskTestState *)context;
    state->sleepCalls++;
    state->lastSleepMs = ms;
}

static SwitchTaskOps make_ops(SwitchTaskTestState *state) {
    SwitchTaskOps ops = {
        .read_switch = read_switch,
        .read_state = read_state,
        .write_state = write_state,
        .sleep_ms = sleep_ms,
        .context = state,
    };

    return ops;
}

static SwitchTaskContext make_context(void) {
    SwitchTaskContext context = {
        .pollingFrequencyMs = 5,
    };

    return context;
}

static void init_state(SwitchTaskTestState *state) {
    memset(state, 0, sizeof(*state));
    state->readSwitchSucceeds = true;
    state->readStateSucceeds = true;
    state->writeStateSucceeds = true;

    effects_state_set_default_order(&state->sourceState);
    state->sourceState.activeEffectSelection = 1;
}

static void test_switch_mapping_and_sleep(void) {
    SwitchTaskTestState state;
    init_state(&state);
    state.switchValues[0] = true;
    state.switchValues[1] = false;
    state.switchValues[2] = true;

    SwitchTaskContext context = make_context();
    SwitchTaskOps ops = make_ops(&state);

    bool ok = switch_task_step(&context, &ops);
    expect_true(ok, "switch step succeeds");
    expect_eq_u32(1, state.writeCalls, "state written once");
    expect_true(state.writtenState.isEnabled[state.writtenState.ordered[0]],
                "switch A maps to first ordered effect");
    expect_true(!state.writtenState.isEnabled[state.writtenState.ordered[1]],
                "switch B maps to second ordered effect");
    expect_true(state.writtenState.isEnabled[state.writtenState.ordered[2]],
                "switch C maps to third ordered effect");
    expect_eq_u32(1, state.sleepCalls, "sleep called once");
    expect_eq_u32(5, state.lastSleepMs, "sleep uses polling frequency");
}

static void test_state_is_normalized_before_write(void) {
    SwitchTaskTestState state;
    init_state(&state);

    state.sourceState.ordered[0] = (Effect)99;
    state.sourceState.ordered[1] = ECHO;
    state.sourceState.ordered[2] = ECHO;
    state.sourceState.activeEffectSelection = 99;

    SwitchTaskContext context = make_context();
    SwitchTaskOps ops = make_ops(&state);

    bool ok = switch_task_step(&context, &ops);
    expect_true(ok, "switch step succeeds with invalid source state");
    expect_eq_u32(1, state.writeCalls, "normalized state written");
    expect_true(effects_state_order_valid(&state.writtenState), "written state order normalized");
    expect_true(state.writtenState.activeEffectSelection < NUM_EFFECTS,
                "active selection normalized");
}

int main(void) {
    test_switch_mapping_and_sleep();
    test_state_is_normalized_before_write();

    if (failures != 0) {
        fprintf(stderr, "test_firmware_switch_task: %d failure(s)\n", failures);
        return 1;
    }

    return 0;
}
