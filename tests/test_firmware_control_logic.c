#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

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

static void expect_eq_size(size_t expected, size_t actual, const char *label)
{
    if (expected != actual)
    {
        fprintf(stderr, "FAIL: %s expected=%zu actual=%zu\n", label, expected, actual);
        failures++;
    }
}

static EffectsParams test_params_template(void)
{
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

static void test_switch_mapping_uses_effect_order(void)
{
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

static void test_overdrive_pot_updates_target_parameter(void)
{
    EffectsParams params = test_params_template();
    size_t expected_level = map_adc_to_param(255, params.overdriveMin.level, params.overdriveMax.level, 255);

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

int main(void)
{
    test_switch_mapping_uses_effect_order();
    test_overdrive_pot_updates_target_parameter();
    test_compression_unused_pots_are_noop();
    test_invalid_effect_or_index_rejected();

    if (failures != 0)
    {
        fprintf(stderr, "Firmware control logic tests failed: %d\n", failures);
        return 1;
    }

    puts("Firmware control logic tests passed");
    return 0;
}
