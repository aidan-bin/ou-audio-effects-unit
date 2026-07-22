#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "switch_task.h"
#include "harness/expect.h"

int failures = 0;

typedef struct
{
    bool switch_values[3];
    EffectsState source_state;

    bool read_switch_succeeds;
    bool read_state_succeeds;
    bool write_state_succeeds;

    uint32_t write_calls;
    EffectsState written_state;

    uint32_t sleep_calls;
    uint32_t last_sleep_ms;
} SwitchTaskTestState;

static bool read_switch(uint8_t index, bool *enabled_out, void *context)
{
    SwitchTaskTestState *state = (SwitchTaskTestState *)context;

    if (!state->read_switch_succeeds || enabled_out == NULL || index >= 3)
    {
        return false;
    }

    *enabled_out = state->switch_values[index];
    return true;
}

static bool read_state(EffectsState *state_out, void *context)
{
    SwitchTaskTestState *state = (SwitchTaskTestState *)context;

    if (!state->read_state_succeeds || state_out == NULL)
    {
        return false;
    }

    *state_out = state->source_state;
    return true;
}

static bool write_state(const EffectsState *state_in, void *context)
{
    SwitchTaskTestState *state = (SwitchTaskTestState *)context;

    if (!state->write_state_succeeds || state_in == NULL)
    {
        return false;
    }

    state->write_calls++;
    state->written_state = *state_in;
    return true;
}

static void sleep_ms(uint32_t ms, void *context)
{
    SwitchTaskTestState *state = (SwitchTaskTestState *)context;
    state->sleep_calls++;
    state->last_sleep_ms = ms;
}

static SwitchTaskOps make_ops(SwitchTaskTestState *state)
{
    SwitchTaskOps ops = {
        .read_switch = read_switch,
        .read_state = read_state,
        .write_state = write_state,
        .sleep_ms = sleep_ms,
        .context = state,
    };

    return ops;
}

static SwitchTaskContext make_context(void)
{
    SwitchTaskContext context = {
        .polling_frequency_ms = 5,
    };

    return context;
}

static void init_state(SwitchTaskTestState *state)
{
    memset(state, 0, sizeof(*state));
    state->read_switch_succeeds = true;
    state->read_state_succeeds = true;
    state->write_state_succeeds = true;

    effects_state_set_default_slots(&state->source_state);
    state->source_state.active_slot = 1;
}

static void test_switch_mapping_and_sleep(void)
{
    SwitchTaskTestState state;
    init_state(&state);
    state.switch_values[0] = true;
    state.switch_values[1] = false;
    state.switch_values[2] = true;

    SwitchTaskContext context = make_context();
    SwitchTaskOps ops = make_ops(&state);

    bool ok = switch_task_step(&context, &ops);
    expect_true(ok, "switch step succeeds");
    expect_eq_u32(1, state.write_calls, "state written once");
    expect_true(state.written_state.slot_enabled[0], "switch A maps to slot 0");
    expect_true(!state.written_state.slot_enabled[1], "switch B maps to slot 1");
    expect_true(state.written_state.slot_enabled[2], "switch C maps to slot 2");
    expect_eq_u32(1, state.sleep_calls, "sleep called once");
    expect_eq_u32(5, state.last_sleep_ms, "sleep uses polling frequency");
}

static void test_state_is_normalized_before_write(void)
{
    SwitchTaskTestState state;
    init_state(&state);

    state.source_state.slots[0] = (uint8_t)99;
    state.source_state.slots[1] = EFFECT_TYPE_ECHO;
    state.source_state.slots[2] = EFFECT_TYPE_ECHO;
    state.source_state.active_slot = 99;

    SwitchTaskContext context = make_context();
    SwitchTaskOps ops = make_ops(&state);

    bool ok = switch_task_step(&context, &ops);
    expect_true(ok, "switch step succeeds with invalid source state");
    expect_eq_u32(1, state.write_calls, "normalized state written");
    expect_true(effects_state_slots_valid(&state.written_state), "written state slots normalized");
    expect_true(state.written_state.active_slot < NUM_SLOTS, "active slot normalized");
}

int main(void)
{
    test_switch_mapping_and_sleep();
    test_state_is_normalized_before_write();

    if (failures != 0)
    {
        fprintf(stderr, "test_firmware_switch_task: %d failure(s)\n", failures);
        return 1;
    }

    return 0;
}
