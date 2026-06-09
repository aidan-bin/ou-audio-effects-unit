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

typedef struct
{
    EffectsState effects_state;
    EffectsParams effects_params;

    bool switches[3];
    uint32_t pot_samples[4];
    uint8_t current_pot;

    bool adc_ready;
    bool dac_ready;
    uint16_t *adc_buffer;
    uint16_t *dac_buffer;

    uint32_t outstanding_allocs;
    uint32_t failure_reports;
} IntegrationState;

static bool switch_read_switch(uint8_t index, bool *enabled_out, void *context)
{
    IntegrationState *state = (IntegrationState *)context;

    if (state == NULL || enabled_out == NULL || index >= 3)
    {
        return false;
    }

    *enabled_out = state->switches[index];
    return true;
}

static bool switch_read_state(EffectsState *state_out, void *context)
{
    IntegrationState *state = (IntegrationState *)context;

    if (state == NULL || state_out == NULL)
    {
        return false;
    }

    *state_out = state->effects_state;
    return true;
}

static bool switch_write_state(const EffectsState *state_in, void *context)
{
    IntegrationState *state = (IntegrationState *)context;

    if (state == NULL || state_in == NULL)
    {
        return false;
    }

    state->effects_state = *state_in;
    return true;
}

static void switch_sleep_ms(uint32_t ms, void *context)
{
    (void)ms;
    (void)context;
}

static bool pot_start_adc(uint8_t pot_index, void *context)
{
    IntegrationState *state = (IntegrationState *)context;

    if (state == NULL || pot_index >= 4)
    {
        return false;
    }

    state->current_pot = pot_index;
    return true;
}

static bool pot_wait_for_sample(uint32_t timeout_ticks, uint32_t *value_out, void *context)
{
    (void)timeout_ticks;

    IntegrationState *state = (IntegrationState *)context;

    if (state == NULL || value_out == NULL)
    {
        return false;
    }

    *value_out = state->pot_samples[state->current_pot];
    return true;
}

static bool pot_read_active_effect(Effect *active_effect, void *context)
{
    IntegrationState *state = (IntegrationState *)context;

    if (state == NULL || active_effect == NULL)
    {
        return false;
    }

    EffectsState latched_state = state->effects_state;
    effects_state_normalize(&latched_state);

    return effects_state_get_active_effect(&latched_state, active_effect);
}

static void pot_apply_sample(Effect active_effect, uint8_t pot_index, uint32_t adc_value,
                             uint32_t adc_max, void *context)
{
    IntegrationState *state = (IntegrationState *)context;

    if (state == NULL)
    {
        return;
    }

    (void)effects_params_apply_pot_sample(&state->effects_params, active_effect, pot_index, adc_value,
                                          adc_max);
}

static bool effects_wait_for_adc(uint32_t timeout_ticks, uint16_t **buf_ptr, void *context)
{
    (void)timeout_ticks;

    IntegrationState *state = (IntegrationState *)context;

    if (state == NULL || buf_ptr == NULL || !state->adc_ready)
    {
        return false;
    }

    state->adc_ready = false;
    *buf_ptr = state->adc_buffer;
    return true;
}

static bool effects_wait_for_dac(uint32_t timeout_ticks, uint16_t **buf_ptr, void *context)
{
    (void)timeout_ticks;

    IntegrationState *state = (IntegrationState *)context;

    if (state == NULL || buf_ptr == NULL || !state->dac_ready)
    {
        return false;
    }

    state->dac_ready = false;
    *buf_ptr = state->dac_buffer;
    return true;
}

static bool effects_dma_copy(const uint16_t *src, uint16_t *dst, size_t count,
                             uint32_t timeout_ticks, void *context)
{
    (void)timeout_ticks;
    (void)context;

    memmove(dst, src, count * sizeof(uint16_t));
    return true;
}

static void *effects_alloc(size_t size, void *context)
{
    IntegrationState *state = (IntegrationState *)context;
    void *ptr = malloc(size);

    if (state != NULL && ptr != NULL)
    {
        state->outstanding_allocs++;
    }

    return ptr;
}

static void effects_free(void *ptr, void *context)
{
    IntegrationState *state = (IntegrationState *)context;

    if (ptr != NULL)
    {
        free(ptr);

        if (state != NULL)
        {
            state->outstanding_allocs--;
        }
    }
}

static bool effects_read_latched_state(EffectsState *state_out, EffectsParams *params_out,
                                       void *context)
{
    IntegrationState *state = (IntegrationState *)context;

    if (state == NULL || state_out == NULL || params_out == NULL)
    {
        return false;
    }

    *state_out = state->effects_state;
    *params_out = state->effects_params;
    return true;
}

static void effects_report_failure(void *context)
{
    IntegrationState *state = (IntegrationState *)context;

    if (state != NULL)
    {
        state->failure_reports++;
    }
}

static uint32_t effects_ms_to_ticks(uint32_t ms, void *context)
{
    (void)context;
    return ms;
}

static void seed_default_limits(IntegrationState *state)
{
    state->effects_params.overdrive_min = (OverdriveParam){
        .level = 0,
        .gain = 0,
        .tone = MIN_OVERDRIVE_TONE,
        .mix = 0,
    };
    state->effects_params.overdrive_max = (OverdriveParam){
        .level = MAX_OVERDRIVE_LEVEL,
        .gain = MAX_OVERDRIVE_GAIN,
        .tone = MAX_OVERDRIVE_TONE,
        .mix = MAX_OVERDRIVE_MIX,
    };
    state->effects_params.echo_min = (EchoParam){
        .pre_delay = MIN_ECHO_PRE_DELAY,
        .density = 5,
        .attack = 7,
        .decay = 3,
    };
    state->effects_params.echo_max = (EchoParam){
        .pre_delay = MAX_ECHO_PRE_DELAY,
        .density = 25,
        .attack = 27,
        .decay = 23,
    };
}

static void test_integration_switch_pot_and_effects_pipeline(void)
{
    IntegrationState state;
    memset(&state, 0, sizeof(state));

    effects_state_set_default_order(&state.effects_state);
    state.effects_state.active_effect_selection = 1;

    state.switches[0] = false;
    state.switches[1] = true;
    state.switches[2] = false;

    state.pot_samples[0] = 255;
    state.pot_samples[1] = 192;
    state.pot_samples[2] = 64;
    state.pot_samples[3] = 32;

    seed_default_limits(&state);

    SwitchTaskContext switch_context = {
        .polling_frequency_ms = 5,
    };
    SwitchTaskOps switch_ops = {
        .read_switch = switch_read_switch,
        .read_state = switch_read_state,
        .write_state = switch_write_state,
        .sleep_ms = switch_sleep_ms,
        .context = &state,
    };

    expect_true(switch_task_step(&switch_context, &switch_ops), "switch task step succeeds");
    expect_true(state.effects_state.is_enabled[OVERDRIVE] == false, "switch task disables overdrive");
    expect_true(state.effects_state.is_enabled[ECHO], "switch task enables echo");
    expect_true(state.effects_state.is_enabled[COMPRESSION] == false,
                "switch task disables compression");

    PotTaskContext pot_context = {
        .sampling_frequency_ticks = 5,
        .timeout_slack_ticks = 1,
        .adc_max = 255,
        .pot_count = 4,
    };
    PotTaskOps pot_ops = {
        .start_adc = pot_start_adc,
        .wait_for_sample = pot_wait_for_sample,
        .read_active_effect = pot_read_active_effect,
        .apply_pot_sample = pot_apply_sample,
        .context = &state,
    };

    expect_true(pot_task_step(&pot_context, &pot_ops), "pot task step succeeds");
    expect_eq_u32(
        map_adc_to_param(state.pot_samples[0], MIN_ECHO_PRE_DELAY, MAX_ECHO_PRE_DELAY, 255),
        state.effects_params.echo.pre_delay, "pot A updates echo pre-delay");
    expect_eq_u32(map_adc_to_param(state.pot_samples[1], state.effects_params.echo_min.density,
                                   state.effects_params.echo_max.density, 255),
                  state.effects_params.echo.density, "pot B updates echo density");
    expect_eq_u32(map_adc_to_param(state.pot_samples[2], state.effects_params.echo_min.attack,
                                   state.effects_params.echo_max.attack, 255),
                  state.effects_params.echo.attack, "pot C updates echo attack");
    expect_eq_u32(map_adc_to_param(state.pot_samples[3], state.effects_params.echo_min.decay,
                                   state.effects_params.echo_max.decay, 255),
                  state.effects_params.echo.decay, "pot D updates echo decay");

    EffectsPipeline pipeline;
    expect_true(effects_pipeline_init(&pipeline) == 0, "effects pipeline init succeeds");

    uint16_t adc_a[8] = {X_AXIS - 20, X_AXIS + 12, X_AXIS + 45, X_AXIS - 30,
                         X_AXIS + 60, X_AXIS - 5, X_AXIS + 1, X_AXIS - 1};
    uint16_t adc_b[8] = {0};
    uint16_t dac_a[8] = {0};
    uint16_t dac_b[8] = {0};
    uint16_t delay_samples[16] = {0};

    state.adc_ready = true;
    state.dac_ready = true;
    state.adc_buffer = adc_a;
    state.dac_buffer = dac_a;

    EffectsTaskContext effects_context = {
        .pipeline = &pipeline,
        .effects_state = NULL,
        .effects_params = NULL,
        .adc_buf_a = adc_a,
        .adc_buf_b = adc_b,
        .dac_buf_a = dac_a,
        .dac_buf_b = dac_b,
        .delay_samples_buf = delay_samples,
        .sample_buf_len = 8,
        .delay_samples_len = 16,
        .sampling_period_us = 25,
        .processing_slack_ms = 1,
    };
    EffectsTaskOps effects_ops = {
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

    expect_true(effects_task_step(&effects_context, &effects_ops), "effects task step succeeds");
    expect_eq_u32(0, state.failure_reports, "effects task reports no failures");
    expect_eq_u32(0, state.outstanding_allocs, "effects task frees all temporary allocations");
}

int main(void)
{
    test_integration_switch_pot_and_effects_pipeline();

    if (failures != 0)
    {
        fprintf(stderr, "test_firmware_task_integration: %d failure(s)\n", failures);
        return 1;
    }

    return 0;
}
