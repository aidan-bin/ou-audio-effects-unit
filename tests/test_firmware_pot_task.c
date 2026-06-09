#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "pot_task.h"

#include "harness/expect.h"

int failures = 0;

typedef struct
{
    bool start_succeeds[4];
    bool wait_succeeds[4];
    uint32_t samples[4];

    bool active_effect_valid[4];
    Effect active_effects[4];

    uint8_t current_pot;

    uint32_t start_calls;
    uint32_t wait_calls;
    uint32_t apply_calls;

    uint8_t applied_pots[4];
    Effect applied_effects[4];
    uint32_t applied_samples[4];
    uint32_t applied_adc_max[4];
} PotTaskTestState;

static bool start_adc(uint8_t pot_index, void *context)
{
    PotTaskTestState *state = (PotTaskTestState *)context;
    state->current_pot = pot_index;
    state->start_calls++;
    return state->start_succeeds[pot_index];
}

static bool wait_for_sample(uint32_t timeout_ticks, uint32_t *value_out, void *context)
{
    (void)timeout_ticks;

    PotTaskTestState *state = (PotTaskTestState *)context;
    state->wait_calls++;

    if (!state->wait_succeeds[state->current_pot] || value_out == NULL)
    {
        return false;
    }

    *value_out = state->samples[state->current_pot];
    return true;
}

static bool read_active_effect(Effect *active_effect, void *context)
{
    PotTaskTestState *state = (PotTaskTestState *)context;

    if (active_effect == NULL || !state->active_effect_valid[state->current_pot])
    {
        return false;
    }

    *active_effect = state->active_effects[state->current_pot];
    return true;
}

static void apply_pot_sample(Effect active_effect, uint8_t pot_index, uint32_t adc_value,
                             uint32_t adc_max, void *context)
{
    PotTaskTestState *state = (PotTaskTestState *)context;
    uint32_t i = state->apply_calls;

    if (i < 4)
    {
        state->applied_effects[i] = active_effect;
        state->applied_pots[i] = pot_index;
        state->applied_samples[i] = adc_value;
        state->applied_adc_max[i] = adc_max;
    }

    state->apply_calls++;
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
        .sampling_frequency_ticks = 5,
        .timeout_slack_ticks = 1,
        .adc_max = 255,
        .pot_count = 4,
    };

    return context;
}

static void init_state(PotTaskTestState *state)
{
    memset(state, 0, sizeof(*state));

    for (int i = 0; i < 4; i++)
    {
        state->start_succeeds[i] = true;
        state->wait_succeeds[i] = true;
        state->samples[i] = (uint32_t)(10 + i);
        state->active_effect_valid[i] = true;
        state->active_effects[i] = OVERDRIVE;
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
    expect_eq_u32(4, state.apply_calls, "apply called for each pot");

    for (uint8_t i = 0; i < 4; i++)
    {
        expect_eq_u32(i, state.applied_pots[i], "pot index propagated");
        expect_eq_u32(state.samples[i], state.applied_samples[i], "sample propagated");
        expect_eq_u32(255, state.applied_adc_max[i], "adc max propagated");
    }
}

static void test_inactive_effect_is_ignored(void)
{
    PotTaskTestState state;
    init_state(&state);
    state.active_effect_valid[2] = false;

    PotTaskContext context = make_context();
    PotTaskOps ops = make_ops(&state);

    bool ok = pot_task_step(&context, &ops);
    expect_true(ok, "pot step succeeds when one active effect read is invalid");
    expect_eq_u32(3, state.apply_calls, "invalid active effect skips apply");
}

static void test_adc_start_failure_skips_channel(void)
{
    PotTaskTestState state;
    init_state(&state);
    state.start_succeeds[1] = false;

    PotTaskContext context = make_context();
    PotTaskOps ops = make_ops(&state);

    bool ok = pot_task_step(&context, &ops);
    expect_true(ok, "pot step succeeds when one adc start fails");
    expect_eq_u32(3, state.apply_calls, "start failure skips apply for that pot");
}

static void test_notify_timeout_skips_channel(void)
{
    PotTaskTestState state;
    init_state(&state);
    state.wait_succeeds[3] = false;

    PotTaskContext context = make_context();
    PotTaskOps ops = make_ops(&state);

    bool ok = pot_task_step(&context, &ops);
    expect_true(ok, "pot step succeeds when one sample wait times out");
    expect_eq_u32(3, state.apply_calls, "wait timeout skips apply for that pot");
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
