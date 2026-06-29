#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "effects_model.h"
#include "i2c_handler.h"
#include "rom_handler.h"
#include "rom_task_support.h"
#include "harness/expect.h"

int failures = 0;

typedef struct
{
    uint32_t allocate_calls;
    uint32_t free_calls;
    uint32_t queue_calls;
    uint32_t set_write_enable_calls;
    bool write_enable_state;
    uint8_t read_payload[1024];
    size_t read_payload_size;
    uint8_t saved_payload[1024];
    size_t saved_payload_size;
} RomTaskTestContext;

static bool queue_message_and_wait(void *queue_handle, void *message, bool *p_failed,
                                   void *failed_semaphore_handle, uint32_t timeout_ticks,
                                   void *context)
{
    (void)queue_handle;
    (void)failed_semaphore_handle;
    (void)timeout_ticks;

    RomTaskTestContext *ctx = (RomTaskTestContext *)context;
    I2CHandlerMessage *rom_message = (I2CHandlerMessage *)message;
    ctx->queue_calls++;
    *p_failed = false;

    if (!rom_message->rx_tx_bar)
    {
        ctx->saved_payload_size = rom_message->items;
        memcpy(ctx->saved_payload, rom_message->payload, rom_message->items);
        ctx->free_calls++;
        free(rom_message->payload);
        return true;
    }

    memcpy(rom_message->payload, ctx->read_payload, ctx->read_payload_size);
    return true;
}

static uint8_t *allocate_payload(size_t size, void *context)
{
    RomTaskTestContext *ctx = (RomTaskTestContext *)context;
    ctx->allocate_calls++;
    return malloc(size);
}

static void free_payload(uint8_t *payload, void *context)
{
    RomTaskTestContext *ctx = (RomTaskTestContext *)context;
    ctx->free_calls++;
    free(payload);
}

static void set_write_disable(bool disable_writes, void *context)
{
    RomTaskTestContext *ctx = (RomTaskTestContext *)context;
    ctx->write_enable_state = disable_writes;
    ctx->set_write_enable_calls++;
}

static RomTaskSupportOps make_ops(RomTaskTestContext *ctx)
{
    RomTaskSupportOps ops = {
        .queue_message_and_wait = queue_message_and_wait,
        .allocate_payload = allocate_payload,
        .free_payload = free_payload,
        .set_write_disable = set_write_disable,
        .context = ctx,
    };
    return ops;
}

static RomTaskSupportConfig make_config(void)
{
    RomTaskSupportConfig config = {
        .source_task = (void *)0x1234,
        .i2c_queue_handle = (void *)0x5678,
        .i2c_failed_rom_semaphore_handle = (void *)0x9ABC,
        .device_address = 0xA0,
        .read_address = 0x0040,
        .write_address = 0x00C0,
        .bootstrap_timeout_ticks = 42,
        .save_timeout_ticks = 84,
    };
    return config;
}

static void test_bootstrap_effects(void)
{
    RomTaskTestContext ctx = {0};
    RomTaskSupportOps ops = make_ops(&ctx);
    RomTaskSupportConfig config = make_config();

    EffectsState expected_state = {0};
    effects_state_set_default_slots(&expected_state);
    expected_state.active_slot = 2;

    EffectsParams expected_params = {0};
    expected_params.overdrive.level = 111;
    expected_params.echo.delay_samples = 7;
    expected_params.compression.threshold = 222;

    ctx.read_payload_size = rom_handler_read_payload_size();
    uint8_t encoded[ROM_HANDLER_ADDRESS_BYTES + ROM_STATE_HEADER_BYTES +
                    sizeof(expected_state) + sizeof(expected_params)] = {0};
    expect_true(rom_handler_encode_write_payload(config.read_address, &expected_state,
                                                 &expected_params, encoded, sizeof(encoded)),
                "build versioned read payload");
    memcpy(ctx.read_payload, &encoded[ROM_HANDLER_ADDRESS_BYTES], ctx.read_payload_size);

    EffectsState loaded_state = {0};
    EffectsParams loaded_params = {0};

    expect_true(rom_task_bootstrap_effects(&config, &ops, &loaded_state, &loaded_params),
                "bootstrap succeeds");
    expect_eq_u32(2, ctx.allocate_calls, "bootstrap allocates twice");
    expect_eq_u32(2, ctx.free_calls, "bootstrap frees twice");
    expect_eq_u32(2, ctx.queue_calls, "bootstrap queues twice");
    expect_eq_u32(2, ctx.set_write_enable_calls, "bootstrap toggles write enable twice");
    expect_true(ctx.write_enable_state, "bootstrap leaves write enable high");
    expect_eq_u32(42, config.bootstrap_timeout_ticks, "bootstrap timeout configured");
    expect_true(loaded_state.active_slot == 2, "bootstrap state decoded");
    expect_true(loaded_params.overdrive.level == 111, "bootstrap params decoded");
}

static void test_save_effects(void)
{
    RomTaskTestContext ctx = {0};
    RomTaskSupportOps ops = make_ops(&ctx);
    RomTaskSupportConfig config = make_config();

    EffectsState state = {0};
    effects_state_set_default_slots(&state);

    EffectsParams params = {0};
    params.overdrive.level = 7;
    params.echo.delay_samples = 9;
    params.compression.threshold = 99;

    expect_true(rom_task_save_effects(&config, &ops, &state, &params), "save succeeds");
    expect_eq_u32(1, ctx.allocate_calls, "save allocates once");
    expect_eq_u32(1, ctx.free_calls, "save frees once");
    expect_eq_u32(1, ctx.queue_calls, "save queues once");
    expect_eq_u32(2, ctx.set_write_enable_calls, "save toggles write enable twice");
    expect_true(ctx.write_enable_state, "save leaves write enable high");

    uint8_t *body = &ctx.saved_payload[ROM_HANDLER_ADDRESS_BYTES];
    expect_eq_u8((uint8_t)ROM_STATE_MAGIC_0, body[0], "save writes magic0");
    expect_eq_u8((uint8_t)ROM_STATE_MAGIC_1, body[1], "save writes magic1");
    expect_eq_u8((uint8_t)ROM_STATE_VERSION, body[2], "save writes version");
    expect_true(memcmp(&body[ROM_STATE_HEADER_BYTES], &state, sizeof(state)) == 0,
                "save encodes state");
    expect_true(memcmp(&body[ROM_STATE_HEADER_BYTES + sizeof(state)], &params, sizeof(params)) == 0,
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