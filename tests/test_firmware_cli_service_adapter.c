#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "cli_service_adapter.h"
#include "harness/expect.h"

int failures = 0;

typedef struct
{
    uint32_t romReadCalls;
    uint32_t romWriteCalls;
} TestOpsContext;

static void set_default_effects_state(EffectsState *state)
{
    memset(state, 0, sizeof(*state));
    effects_state_set_default_order(state);
}

static void set_default_effects_params(EffectsParams *params)
{
    memset(params, 0, sizeof(*params));

    params->overdriveMin.gain = 10;
    params->overdriveMax.gain = 110;
    params->overdriveMin.level = 20;
    params->overdriveMax.level = 120;
    params->overdriveMin.tone = 30;
    params->overdriveMax.tone = 130;
    params->overdriveMin.mix = 40;
    params->overdriveMax.mix = 140;

    params->echoMin.delay_samples = 5;
    params->echoMax.delay_samples = 50;
    params->echoMin.pre_delay = 1;
    params->echoMax.pre_delay = 20;
    params->echoMin.density = 2;
    params->echoMax.density = 22;
    params->echoMin.attack = 3;
    params->echoMax.attack = 23;
    params->echoMin.decay = 4;
    params->echoMax.decay = 24;

    params->compressionMin.threshold = 100;
    params->compressionMax.threshold = 400;
    params->compressionMin.ratio = 1;
    params->compressionMax.ratio = 32;
}

static bool rom_read_raw(uint16_t address, uint16_t length, uint8_t *payloadOut,
                         size_t payloadCapacity, size_t *payloadSizeOut, void *context)
{
    (void)address;
    (void)length;
    TestOpsContext *ops = (TestOpsContext *)context;
    ops->romReadCalls++;

    if (payloadCapacity < 2)
    {
        return false;
    }

    payloadOut[0] = 0x12;
    payloadOut[1] = 0x34;
    *payloadSizeOut = 2;
    return true;
}

static bool rom_write_raw(uint16_t address, const uint8_t *payload, size_t payloadSize,
                          void *context)
{
    (void)address;
    (void)payload;
    (void)payloadSize;
    TestOpsContext *ops = (TestOpsContext *)context;
    ops->romWriteCalls++;
    return true;
}

static void test_pot_override_precedence(void)
{
    EffectsState state;
    EffectsParams params;
    set_default_effects_state(&state);
    set_default_effects_params(&params);

    CliServiceAdapterContext context;
    cli_service_adapter_init(&context, &state, &params, NULL, 255);

    CliServices services = {0};
    cli_service_adapter_bind(&context, &services);

    expect_true(cli_service_adapter_apply_pot_sample(&context, OVERDRIVE, 0, 0),
                "pot apply without override");
    expect_eq_size(10, params.overdrive.gain, "raw pot input applied");

    expect_true(services.set_pot_override(0, 255, services.context), "set pot override");
    expect_true(cli_service_adapter_apply_pot_sample(&context, OVERDRIVE, 0, 0),
                "pot apply with override");
    expect_eq_size(110, params.overdrive.gain, "override value takes precedence");

    expect_true(services.clear_pot_override(0, services.context), "clear pot override");
    expect_true(cli_service_adapter_apply_pot_sample(&context, OVERDRIVE, 0, 0),
                "pot apply after clear");
    expect_eq_size(10, params.overdrive.gain, "hardware value restored after clear");
}

static void test_switch_override_precedence(void)
{
    EffectsState state;
    EffectsParams params;
    set_default_effects_state(&state);
    set_default_effects_params(&params);

    CliServiceAdapterContext context;
    cli_service_adapter_init(&context, &state, &params, NULL, 255);

    CliServices services = {0};
    cli_service_adapter_bind(&context, &services);

    expect_true(services.set_switch_override(1, true, services.context), "set switch override");
    expect_true(cli_service_adapter_apply_switches(&context, false, false, false),
                "apply switches with override");
    expect_true(state.isEnabled[ECHO], "switch override took precedence");

    expect_true(services.clear_switch_override(1, services.context), "clear switch override");
    expect_true(cli_service_adapter_apply_switches(&context, false, false, false),
                "apply switches after clear");
    expect_false(state.isEnabled[ECHO], "hardware switch restored after clear");
}

static void test_config_bounds_and_state_updates(void)
{
    EffectsState state;
    EffectsParams params;
    set_default_effects_state(&state);
    set_default_effects_params(&params);

    CliServiceAdapterContext context;
    cli_service_adapter_init(&context, &state, &params, NULL, 255);

    CliServices services = {0};
    cli_service_adapter_bind(&context, &services);

    expect_false(services.config_set("overdrive.gain", 9, services.context),
                 "reject out-of-range config write");
    expect_true(services.config_set("overdrive.gain", 50, services.context),
                "accept in-range config write");

    int32_t value = -1;
    expect_true(services.config_get("overdrive.gain", &value, services.context),
                "read config value");
    expect_eq_u32(50, (uint32_t)value, "config write persisted");

    expect_true(services.config_set("state.active_effect", 2, services.context),
                "set active effect");
    expect_eq_u8(2, state.activeEffectSelection, "active effect updated");
    expect_false(services.config_set("state.active_effect", 9, services.context),
                 "reject invalid active effect");
}

static void test_rom_raw_guardrails(void)
{
    EffectsState state;
    EffectsParams params;
    set_default_effects_state(&state);
    set_default_effects_params(&params);

    TestOpsContext ops_context = {0};
    CliServiceAdapterOps ops = {
        .rom_read_raw = rom_read_raw,
        .rom_write_raw = rom_write_raw,
        .context = &ops_context,
    };

    CliServiceAdapterContext context;
    cli_service_adapter_init(&context, &state, &params, &ops, 255);
    context.romRawMinAddress = 0x0040;
    context.romRawMaxAddress = 0x00FF;
    context.romRawMaxLength = 8;

    CliServices services = {0};
    cli_service_adapter_bind(&context, &services);

    uint8_t out[8] = {0};
    size_t out_size = 0;
    expect_true(services.rom_read_raw(0x0040, 2, out, sizeof(out), &out_size, services.context),
                "rom read in range");
    expect_eq_u32(1, ops_context.romReadCalls, "rom read delegated");

    expect_false(services.rom_read_raw(0x0001, 2, out, sizeof(out), &out_size, services.context),
                 "rom read out of range rejected");
    expect_eq_u32(1, ops_context.romReadCalls, "rom read not delegated when rejected");

    uint8_t payload[2] = {0xAA, 0xBB};
    expect_true(services.rom_write_raw(0x0041, payload, 2, services.context),
                "rom write in range");
    expect_eq_u32(1, ops_context.romWriteCalls, "rom write delegated");

    expect_false(services.rom_write_raw(0x00FE, payload, 4, services.context),
                 "rom write overflow rejected");
    expect_eq_u32(1, ops_context.romWriteCalls, "rom write not delegated when rejected");
}

int main(void)
{
    test_pot_override_precedence();
    test_switch_override_precedence();
    test_config_bounds_and_state_updates();
    test_rom_raw_guardrails();

    if (failures != 0)
    {
        fprintf(stderr, "Firmware cli service adapter tests failed: %d\n", failures);
        return 1;
    }

    puts("Firmware cli service adapter tests passed");
    return 0;
}