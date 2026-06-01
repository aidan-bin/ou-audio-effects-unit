#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "effects_control_logic.h"
#include "effects_state_manager.h"
#include "i2c_handler.h"
#include "rom_handler.h"
#include "rom_task_support.h"

static int failures = 0;

typedef struct
{
    uint32_t allocateCalls;
    uint32_t freeCalls;
    uint32_t queueCalls;
    uint32_t setWriteEnableCalls;
    bool writeEnableState;
    uint8_t readPayload[1024];
    size_t readPayloadSize;
    uint8_t savedPayload[1024];
    size_t savedPayloadSize;
} RomTaskTestContext;

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

static void expect_eq_u16(uint16_t expected, uint16_t actual, const char *label)
{
    if (expected != actual)
    {
        fprintf(stderr, "FAIL: %s expected=%u actual=%u\n", label, expected, actual);
        failures++;
    }
}

static bool queue_message_and_wait(void *queueHandle, void *message, bool *pFailed,
    void *failedSemaphoreHandle, uint32_t timeoutTicks, void *context)
{
    (void)queueHandle;
    (void)failedSemaphoreHandle;
    (void)timeoutTicks;

    RomTaskTestContext *ctx = (RomTaskTestContext *)context;
    I2CHandlerMessage *romMessage = (I2CHandlerMessage *)message;
    ctx->queueCalls++;
    *pFailed = false;

    if (!romMessage->rxTxBar)
    {
        ctx->savedPayloadSize = romMessage->items;
        memcpy(ctx->savedPayload, romMessage->payload, romMessage->items);
        ctx->freeCalls++;
        free(romMessage->payload);
        return true;
    }

    memcpy(romMessage->payload, ctx->readPayload, ctx->readPayloadSize);
    return true;
}

static uint8_t *allocate_payload(size_t size, void *context)
{
    RomTaskTestContext *ctx = (RomTaskTestContext *)context;
    ctx->allocateCalls++;
    return malloc(size);
}

static void free_payload(uint8_t *payload, void *context)
{
    RomTaskTestContext *ctx = (RomTaskTestContext *)context;
    ctx->freeCalls++;
    free(payload);
}

static void set_write_enable(bool enabled, void *context)
{
    RomTaskTestContext *ctx = (RomTaskTestContext *)context;
    ctx->writeEnableState = enabled;
    ctx->setWriteEnableCalls++;
}

static RomTaskSupportOps make_ops(RomTaskTestContext *ctx)
{
    RomTaskSupportOps ops = {
        .queue_message_and_wait = queue_message_and_wait,
        .allocate_payload = allocate_payload,
        .free_payload = free_payload,
        .set_write_enable = set_write_enable,
        .context = ctx,
    };
    return ops;
}

static RomTaskSupportConfig make_config(void)
{
    RomTaskSupportConfig config = {
        .sourceTask = (void *)0x1234,
        .i2cQueueHandle = (void *)0x5678,
        .i2cFailedRomSemaphoreHandle = (void *)0x9ABC,
        .deviceAddress = 0xA0,
        .readAddress = 0x0040,
        .writeAddress = 0x00C0,
        .bootstrapTimeoutTicks = 42,
        .saveTimeoutTicks = 84,
    };
    return config;
}

static void test_bootstrap_effects(void)
{
    RomTaskTestContext ctx = {0};
    RomTaskSupportOps ops = make_ops(&ctx);
    RomTaskSupportConfig config = make_config();

    EffectsState expectedState = {0};
    effects_state_set_default_order(&expectedState);
    expectedState.activeEffectSelection = 2;

    EffectsParams expectedParams = {0};
    expectedParams.overdrive.level = 111;
    expectedParams.echo.delay_samples = 7;
    expectedParams.compression.threshold = 222;

    ctx.readPayloadSize = rom_handler_read_payload_size();
    memcpy(ctx.readPayload, &expectedState, sizeof(expectedState));
    memcpy(&ctx.readPayload[sizeof(expectedState)], &expectedParams, sizeof(expectedParams));

    EffectsState loadedState = {0};
    EffectsParams loadedParams = {0};

    expect_true(rom_task_bootstrap_effects(&config, &ops, &loadedState, &loadedParams),
        "bootstrap succeeds");
    expect_eq_u32(2, ctx.allocateCalls, "bootstrap allocates twice");
    expect_eq_u32(2, ctx.freeCalls, "bootstrap frees twice");
    expect_eq_u32(2, ctx.queueCalls, "bootstrap queues twice");
    expect_eq_u32(2, ctx.setWriteEnableCalls, "bootstrap toggles write enable twice");
    expect_true(ctx.writeEnableState, "bootstrap leaves write enable high");
    expect_eq_u32(42, config.bootstrapTimeoutTicks, "bootstrap timeout configured");
    expect_true(loadedState.activeEffectSelection == 2, "bootstrap state decoded");
    expect_true(loadedParams.overdrive.level == 111, "bootstrap params decoded");
}

static void test_save_effects(void)
{
    RomTaskTestContext ctx = {0};
    RomTaskSupportOps ops = make_ops(&ctx);
    RomTaskSupportConfig config = make_config();

    EffectsState state = {0};
    effects_state_set_default_order(&state);

    EffectsParams params = {0};
    params.overdrive.level = 7;
    params.echo.delay_samples = 9;
    params.compression.threshold = 99;

    expect_true(rom_task_save_effects(&config, &ops, &state, &params), "save succeeds");
    expect_eq_u32(1, ctx.allocateCalls, "save allocates once");
    expect_eq_u32(1, ctx.freeCalls, "save frees once");
    expect_eq_u32(1, ctx.queueCalls, "save queues once");
    expect_eq_u32(2, ctx.setWriteEnableCalls, "save toggles write enable twice");
    expect_true(ctx.writeEnableState, "save leaves write enable high");
    expect_true(memcmp(&ctx.savedPayload[2], &state, sizeof(state)) == 0, "save encodes state");
    expect_true(memcmp(&ctx.savedPayload[2 + sizeof(state)], &params, sizeof(params)) == 0,
        "save encodes params");
}

int main(void)
{
    test_bootstrap_effects();
    test_save_effects();

    if (failures != 0)
    {
        fprintf(stderr, "Firmware rom task support tests failed: %d\n", failures);
        return 1;
    }

    puts("Firmware rom task support tests passed");
    return 0;
}