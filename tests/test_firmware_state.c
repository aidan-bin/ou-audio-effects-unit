#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "effects_model.h"

static int failures = 0;

static void expect_true(bool value, const char *label)
{
    if (!value)
    {
        fprintf(stderr, "FAIL: %s expected=true actual=false\n", label);
        failures++;
    }
}

static void expect_false(bool value, const char *label)
{
    if (value)
    {
        fprintf(stderr, "FAIL: %s expected=false actual=true\n", label);
        failures++;
    }
}

static void expect_eq_u8(uint8_t expected, uint8_t actual, const char *label)
{
    if (expected != actual)
    {
        fprintf(stderr, "FAIL: %s expected=%u actual=%u\n", label, expected, actual);
        failures++;
    }
}

static void expect_eq_size(size_t expected, size_t actual, const char *label)
{
    if (expected != actual)
    {
        fprintf(stderr, "FAIL: %s expected=%zu actual=%zu\n", label, expected, actual);
        failures++;
    }
}

static void test_effects_state_order_valid_default_order(void)
{
    EffectsState state = {
        .ordered = {OVERDRIVE, ECHO, COMPRESSION},
        .isEnabled = {true, false, true},
        .activeEffectSelection = 1,
    };

    expect_true(effects_state_order_valid(&state), "order valid default");
}

static void test_effects_state_normalize_invalid_order_resets(void)
{
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

static void test_effects_state_get_active_effect_invalid_state_fails(void)
{
    EffectsState state = {
        .ordered = {OVERDRIVE, OVERDRIVE, COMPRESSION},
        .isEnabled = {true, true, true},
        .activeEffectSelection = 0,
    };

    Effect active = OVERDRIVE;
    expect_false(effects_state_get_active_effect(&state, &active), "get active effect invalid order");
}

static void test_effects_state_get_active_effect_after_normalize(void)
{
    EffectsState state = {
        .ordered = {OVERDRIVE, ECHO, COMPRESSION},
        .isEnabled = {true, true, true},
        .activeEffectSelection = 2,
    };

    Effect active = OVERDRIVE;
    expect_true(effects_state_get_active_effect(&state, &active), "get active effect valid state");
    expect_eq_u8(COMPRESSION, (uint8_t)active, "active effect selection");
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

int main(void)
{
    test_effects_state_order_valid_default_order();
    test_effects_state_normalize_invalid_order_resets();
    test_effects_state_get_active_effect_invalid_state_fails();
    test_effects_state_get_active_effect_after_normalize();
    test_map_adc_to_param_zero_adcmax_returns_min();
    test_map_adc_to_param_swaps_inverted_bounds();
    test_map_adc_to_param_clamps_adc_input();

    if (failures != 0)
    {
        fprintf(stderr, "Firmware state tests failed: %d\n", failures);
        return 1;
    }

    puts("Firmware state tests passed");
    return 0;
}
