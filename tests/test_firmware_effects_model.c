#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "effects_model.h"
#include "harness/expect.h"

int failures = 0;

static void test_default_slots(void)
{
    EffectsState state;
    memset(&state, 0xAB, sizeof(state));
    effects_state_set_default_slots(&state);

    expect_eq_u8(OVERDRIVE, state.slots[0], "default slot0 overdrive");
    expect_eq_u8(ECHO, state.slots[1], "default slot1 echo");
    expect_eq_u8(COMPRESSION, state.slots[2], "default slot2 compression");
    expect_eq_u8(EFFECT_ID_NONE, state.slots[3], "default slot3 empty");
    expect_true(state.slot_enabled[0], "default slot0 enabled");
    expect_false(state.slot_enabled[1], "default slot1 disabled");
    expect_false(state.slot_enabled[2], "default slot2 disabled");
    expect_eq_u8(0, state.active_slot, "default active slot 0");
    expect_true(effects_state_slots_valid(&state), "default slots valid");
}

static void test_slots_valid_detects_duplicate(void)
{
    EffectsState state;
    effects_state_set_default_slots(&state);
    state.slots[3] = OVERDRIVE; // duplicate of slot0

    expect_false(effects_state_slots_valid(&state), "duplicate effect is invalid");
}

static void test_normalize_clears_invalid_and_duplicate(void)
{
    EffectsState state;
    effects_state_set_default_slots(&state);
    state.slots[2] = (uint8_t)99; // invalid id
    state.slots[3] = OVERDRIVE;   // duplicate of slot0
    state.slot_enabled[3] = true;
    state.active_slot = (uint8_t)(NUM_SLOTS + 3);

    effects_state_normalize(&state);

    expect_eq_u8(OVERDRIVE, state.slots[0], "normalize keeps first overdrive");
    expect_eq_u8(EFFECT_ID_NONE, state.slots[2], "normalize clears invalid slot");
    expect_eq_u8(EFFECT_ID_NONE, state.slots[3], "normalize clears duplicate slot");
    expect_false(state.slot_enabled[3], "normalize disables cleared slot");
    expect_eq_u8(0, state.active_slot, "normalize clamps active slot");
}

static void test_assign_slot_moves_existing(void)
{
    EffectsState state;
    effects_state_set_default_slots(&state); // od@0 echo@1 comp@2 empty@3

    expect_true(effects_state_assign_slot(&state, 3, ECHO), "assign echo to slot3");
    expect_eq_u8(ECHO, state.slots[3], "echo now in slot3");
    expect_eq_u8(EFFECT_ID_NONE, state.slots[1], "old echo slot vacated");

    expect_false(effects_state_assign_slot(&state, NUM_SLOTS, OVERDRIVE),
                 "assign rejects out-of-range pos");
    expect_false(effects_state_assign_slot(&state, 0, (Effect)99), "assign rejects invalid effect");
}

static void test_clear_slot(void)
{
    EffectsState state;
    effects_state_set_default_slots(&state);

    expect_true(effects_state_clear_slot(&state, 0), "clear slot0");
    expect_eq_u8(EFFECT_ID_NONE, state.slots[0], "slot0 empty after clear");
    expect_false(state.slot_enabled[0], "slot0 disabled after clear");
    expect_false(effects_state_clear_slot(&state, NUM_SLOTS), "clear rejects out-of-range pos");
}

static void test_swap_slots_carries_enable(void)
{
    EffectsState state;
    effects_state_set_default_slots(&state); // slot0 enabled, slot2 disabled

    expect_true(effects_state_swap_slots(&state, 0, 2), "swap slots 0 and 2");
    expect_eq_u8(COMPRESSION, state.slots[0], "slot0 now compression");
    expect_eq_u8(OVERDRIVE, state.slots[2], "slot2 now overdrive");
    expect_false(state.slot_enabled[0], "slot0 takes old slot2 enable (off)");
    expect_true(state.slot_enabled[2], "slot2 takes old slot0 enable (on)");
    expect_false(effects_state_swap_slots(&state, 0, NUM_SLOTS), "swap rejects bad pos");
}

static void test_set_slot_enabled(void)
{
    EffectsState state;
    effects_state_set_default_slots(&state);

    expect_true(effects_state_set_slot_enabled(&state, 3, true), "enable slot3 via CLI");
    expect_true(state.slot_enabled[3], "slot3 enabled");
    expect_false(effects_state_set_slot_enabled(&state, NUM_SLOTS, true),
                 "set_slot_enabled rejects bad pos");
}

static void test_effect_enabled_reflects_slot(void)
{
    EffectsState state;
    effects_state_set_default_slots(&state); // overdrive enabled in slot0

    expect_true(effects_state_effect_enabled(&state, OVERDRIVE), "overdrive enabled");
    expect_false(effects_state_effect_enabled(&state, ECHO), "echo assigned but disabled");

    effects_state_clear_slot(&state, 1);
    expect_false(effects_state_effect_enabled(&state, ECHO), "unassigned echo not enabled");
    expect_false(effects_state_effect_enabled(&state, (Effect)99), "invalid effect not enabled");
}

static void test_active_effect_from_active_slot(void)
{
    EffectsState state;
    effects_state_set_default_slots(&state);
    state.active_slot = 1;

    Effect active = OVERDRIVE;
    expect_true(effects_state_get_active_effect(&state, &active), "active effect resolves");
    expect_eq_u8(ECHO, (uint8_t)active, "active effect is echo");

    effects_state_clear_slot(&state, 1);
    expect_false(effects_state_get_active_effect(&state, &active),
                 "empty active slot yields no effect");
}

static void test_apply_switches_drives_slot_enables(void)
{
    EffectsState state;
    effects_state_set_default_slots(&state);

    effects_state_apply_switches(&state, false, true, true);
    expect_false(state.slot_enabled[0], "switch A drives slot0");
    expect_true(state.slot_enabled[1], "switch B drives slot1");
    expect_true(state.slot_enabled[2], "switch C drives slot2");
}

static void test_set_active_slot(void)
{
    EffectsState state;
    effects_state_set_default_slots(&state);

    expect_true(effects_state_set_active_slot(&state, 2), "set active slot 2");
    expect_eq_u8(2, state.active_slot, "active slot updated");
    expect_false(effects_state_set_active_slot(&state, NUM_SLOTS),
                 "reject out-of-range active slot");
    expect_eq_u8(2, state.active_slot, "active slot unchanged after rejection");
}

static void test_effect_id_is_empty(void)
{
    expect_true(effect_id_is_empty(EFFECT_ID_NONE), "EFFECT_ID_NONE is empty");
    expect_false(effect_id_is_empty((EffectId)OVERDRIVE), "valid id is not empty");
}

static void test_effect_is_valid_boundaries(void)
{
    expect_true(effect_is_valid(OVERDRIVE), "OVERDRIVE valid");
    expect_true(effect_is_valid(COMPRESSION), "COMPRESSION valid");
    expect_false(effect_is_valid((Effect)-1), "negative effect invalid");
    expect_false(effect_is_valid((Effect)NUM_EFFECTS), "NUM_EFFECTS out of range");
}

static void test_map_adc_to_param_zero_adcmax_returns_min(void)
{
    expect_eq_size(17, map_adc_to_param(123, 17, 99, 0), "map adc zero max");
}

static void test_map_adc_to_param_swaps_inverted_bounds(void)
{
    size_t mapped_inverted = map_adc_to_param(255, 100, 20, 255);
    size_t mapped_ordered = map_adc_to_param(255, 20, 100, 255);
    expect_eq_size(mapped_ordered, mapped_inverted, "map adc swapped bounds");
}

static void test_map_adc_to_param_clamps_adc_input(void)
{
    size_t mapped_clamped = map_adc_to_param(500, 10, 110, 255);
    size_t mapped_at_max = map_adc_to_param(255, 10, 110, 255);
    expect_eq_size(mapped_at_max, mapped_clamped, "map adc clamp input");
}

static EffectsParams test_params_template(void)
{
    EffectsParams params;
    memset(&params, 0, sizeof(params));

    params.overdrive_min.gain = 10;
    params.overdrive_max.gain = 110;
    params.overdrive_min.level = 100;
    params.overdrive_max.level = 500;
    params.overdrive_min.tone = 50;
    params.overdrive_max.tone = 250;
    params.overdrive_min.mix = 20;
    params.overdrive_max.mix = 120;

    params.echo_min.pre_delay = 1;
    params.echo_max.pre_delay = 9;
    params.echo_min.density = 5;
    params.echo_max.density = 25;
    params.echo_min.attack = 7;
    params.echo_max.attack = 27;
    params.echo_min.decay = 3;
    params.echo_max.decay = 23;

    params.compression_min.threshold = 11;
    params.compression_max.threshold = 211;
    params.compression_min.ratio = 13;
    params.compression_max.ratio = 213;

    return params;
}

static void test_overdrive_pot_updates_target_parameter(void)
{
    EffectsParams params = test_params_template();
    size_t expected_level =
        map_adc_to_param(255, params.overdrive_min.level, params.overdrive_max.level, 255);

    expect_true(effects_params_apply_pot_sample(&params, OVERDRIVE, 1, 255, 255),
                "overdrive pot apply returns true");

    expect_eq_size(expected_level, params.overdrive.level, "overdrive pot1 updates level");
}

static void test_compression_unused_pots_are_noop(void)
{
    EffectsParams params = test_params_template();
    params.compression.threshold = 77;
    params.compression.ratio = 88;

    expect_true(effects_params_apply_pot_sample(&params, COMPRESSION, 2, 255, 255),
                "compression pot2 noop success");
    expect_true(effects_params_apply_pot_sample(&params, COMPRESSION, 3, 255, 255),
                "compression pot3 noop success");

    expect_eq_size(77, params.compression.threshold, "compression threshold unchanged on pot2/3");
    expect_eq_size(88, params.compression.ratio, "compression ratio unchanged on pot2/3");
}

static void test_invalid_effect_or_index_rejected(void)
{
    EffectsParams params = test_params_template();

    expect_false(effects_params_apply_pot_sample(&params, (Effect)99, 0, 10, 255),
                 "invalid effect rejected");
    expect_false(effects_params_apply_pot_sample(&params, OVERDRIVE, 9, 10, 255),
                 "invalid pot index rejected");
}

static void test_effects_params_value_for_points_at_members(void)
{
    EffectsParams params;
    memset(&params, 0, sizeof(params));

    expect_true(effects_params_value_for(&params, OVERDRIVE) == (const void *)&params.overdrive,
                "value_for overdrive");
    expect_true(effects_params_value_for(&params, ECHO) == (const void *)&params.echo,
                "value_for echo");
    expect_true(
        effects_params_value_for(&params, COMPRESSION) == (const void *)&params.compression,
        "value_for compression");
    expect_true(effects_params_value_for(&params, (Effect)99) == NULL,
                "value_for invalid effect is NULL");
    expect_true(effects_params_value_for(NULL, OVERDRIVE) == NULL, "value_for NULL params");
}

int main(void)
{
    test_default_slots();
    test_slots_valid_detects_duplicate();
    test_normalize_clears_invalid_and_duplicate();
    test_assign_slot_moves_existing();
    test_clear_slot();
    test_swap_slots_carries_enable();
    test_set_slot_enabled();
    test_effect_enabled_reflects_slot();
    test_active_effect_from_active_slot();
    test_apply_switches_drives_slot_enables();
    test_set_active_slot();
    test_effect_id_is_empty();
    test_effect_is_valid_boundaries();
    test_map_adc_to_param_zero_adcmax_returns_min();
    test_map_adc_to_param_swaps_inverted_bounds();
    test_map_adc_to_param_clamps_adc_input();
    test_overdrive_pot_updates_target_parameter();
    test_compression_unused_pots_are_noop();
    test_invalid_effect_or_index_rejected();
    test_effects_params_value_for_points_at_members();

    if (failures != 0)
    {
        fprintf(stderr, "effects model tests failed: %d\n", failures);
        return 1;
    }

    puts("effects model tests passed");
    return 0;
}
