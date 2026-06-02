#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "effects_model.h"
#include "harness/expect.h"

int failures = 0;

static void test_effects_state_order_valid_default_order(void) {
    EffectsState state = {
        .ordered = {OVERDRIVE, ECHO, COMPRESSION},
        .isEnabled = {true, false, true},
        .activeEffectSelection = 1,
    };

    expect_true(effects_state_order_valid(&state), "order valid default");
}

static void test_effects_state_normalize_invalid_order_resets(void) {
    EffectsState state = {
        .ordered = {OVERDRIVE, OVERDRIVE, COMPRESSION},
        .isEnabled = {true, true, true},
        .activeEffectSelection = NUM_EFFECTS,
    };

    effects_state_normalize(&state);

    expect_eq_u8(OVERDRIVE, state.ordered[0], "normalize order[0]");
    expect_eq_u8(ECHO, state.ordered[1], "normalize order[1]");
    expect_eq_u8(COMPRESSION, state.ordered[2], "normalize order[2]");
    expect_eq_u8(0, state.activeEffectSelection, "normalize active selection");
}

static void test_effects_state_get_active_effect_invalid_state_fails(void) {
    EffectsState state = {
        .ordered = {OVERDRIVE, OVERDRIVE, COMPRESSION},
        .isEnabled = {true, true, true},
        .activeEffectSelection = 0,
    };

    Effect active = OVERDRIVE;
    expect_false(effects_state_get_active_effect(&state, &active),
                 "get active effect invalid order");
}

static void test_effects_state_get_active_effect_after_normalize(void) {
    EffectsState state = {
        .ordered = {OVERDRIVE, ECHO, COMPRESSION},
        .isEnabled = {true, true, true},
        .activeEffectSelection = 2,
    };

    Effect active = OVERDRIVE;
    expect_true(effects_state_get_active_effect(&state, &active), "get active effect valid state");
    expect_eq_u8(COMPRESSION, (uint8_t)active, "active effect selection");
}

static void test_map_adc_to_param_zero_adcmax_returns_min(void) {
    expect_eq_size(17, map_adc_to_param(123, 17, 99, 0), "map adc zero max");
}

static void test_map_adc_to_param_swaps_inverted_bounds(void) {
    size_t mapped_inverted = map_adc_to_param(255, 100, 20, 255);
    size_t mapped_ordered = map_adc_to_param(255, 20, 100, 255);
    expect_eq_size(mapped_ordered, mapped_inverted, "map adc swapped bounds");
}

static void test_map_adc_to_param_clamps_adc_input(void) {
    size_t mapped_clamped = map_adc_to_param(500, 10, 110, 255);
    size_t mapped_at_max = map_adc_to_param(255, 10, 110, 255);
    expect_eq_size(mapped_at_max, mapped_clamped, "map adc clamp input");
}

static EffectsParams test_params_template(void) {
    EffectsParams params;
    memset(&params, 0, sizeof(params));

    params.overdriveMin.gain = 10;
    params.overdriveMax.gain = 110;
    params.overdriveMin.level = 100;
    params.overdriveMax.level = 500;
    params.overdriveMin.tone = 50;
    params.overdriveMax.tone = 250;
    params.overdriveMin.mix = 20;
    params.overdriveMax.mix = 120;

    params.echoMin.pre_delay = 1;
    params.echoMax.pre_delay = 9;
    params.echoMin.density = 5;
    params.echoMax.density = 25;
    params.echoMin.attack = 7;
    params.echoMax.attack = 27;
    params.echoMin.decay = 3;
    params.echoMax.decay = 23;

    params.compressionMin.threshold = 11;
    params.compressionMax.threshold = 211;
    params.compressionMin.ratio = 13;
    params.compressionMax.ratio = 213;

    return params;
}

static void test_switch_mapping_uses_effect_order(void) {
    EffectsState state = {
        .ordered = {ECHO, COMPRESSION, OVERDRIVE},
        .isEnabled = {false, false, false},
        .activeEffectSelection = 0,
    };

    effects_state_apply_switches(&state, true, false, true);

    expect_true(state.isEnabled[ECHO], "switch A maps to ordered[0]");
    expect_false(state.isEnabled[COMPRESSION], "switch B maps to ordered[1]");
    expect_true(state.isEnabled[OVERDRIVE], "switch C maps to ordered[2]");
}

static void test_overdrive_pot_updates_target_parameter(void) {
    EffectsParams params = test_params_template();
    size_t expected_level =
        map_adc_to_param(255, params.overdriveMin.level, params.overdriveMax.level, 255);

    expect_true(effects_params_apply_pot_sample(&params, OVERDRIVE, 1, 255, 255),
                "overdrive pot apply returns true");

    expect_eq_size(expected_level, params.overdrive.level, "overdrive pot1 updates level");
}

static void test_compression_unused_pots_are_noop(void) {
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

static void test_invalid_effect_or_index_rejected(void) {
    EffectsParams params = test_params_template();

    expect_false(effects_params_apply_pot_sample(&params, (Effect)99, 0, 10, 255),
                 "invalid effect rejected");
    expect_false(effects_params_apply_pot_sample(&params, OVERDRIVE, 9, 10, 255),
                 "invalid pot index rejected");
}

int main(void) {
    test_effects_state_order_valid_default_order();
    test_effects_state_normalize_invalid_order_resets();
    test_effects_state_get_active_effect_invalid_state_fails();
    test_effects_state_get_active_effect_after_normalize();
    test_map_adc_to_param_zero_adcmax_returns_min();
    test_map_adc_to_param_swaps_inverted_bounds();
    test_map_adc_to_param_clamps_adc_input();

    test_switch_mapping_uses_effect_order();
    test_overdrive_pot_updates_target_parameter();
    test_compression_unused_pots_are_noop();
    test_invalid_effect_or_index_rejected();

    if (failures != 0) {
        fprintf(stderr, "effects model tests failed: %d\n", failures);
        return 1;
    }

    puts("effects model tests passed");
    return 0;
}
