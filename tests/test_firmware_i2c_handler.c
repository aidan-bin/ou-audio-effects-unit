#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "i2c_handler.h"
#include "mocks/freertos_mock.h"
#include "mocks/hal_mock.h"

#include "harness/expect.h"

int failures = 0;

typedef struct
{
    void *rom_task;
    void *display_task;
    bool completion_signaled;
    uint32_t delay_calls;
    uint32_t free_calls;
    bool transfer_failed;
    FreeRtosMockBinarySemaphore completion_semaphore;
} I2CTestContext;

static bool is_source_valid(void *source_task, void *context)
{
    I2CTestContext *ctx = (I2CTestContext *)context;
    return source_task == ctx->rom_task || source_task == ctx->display_task;
}

static void signal_source_completion(void *source_task, void *context)
{
    (void)source_task;
    I2CTestContext *ctx = (I2CTestContext *)context;
    ctx->completion_signaled = true;
}

static bool is_device_ready(uint16_t address, uint32_t trials, uint32_t timeout_ms, void *context)
{
    (void)context;
    return hal_mock_i2c_is_device_ready(address, trials, timeout_ms) == HAL_MOCK_STATUS_OK;
}

static bool start_receive(uint16_t address, uint8_t *payload, uint16_t items, void *context)
{
    (void)context;
    return hal_mock_i2c_master_receive_dma(address, payload, items) == HAL_MOCK_STATUS_OK;
}

static bool start_transmit(uint16_t address, uint8_t *payload, uint16_t items, void *context)
{
    (void)context;
    return hal_mock_i2c_master_transmit_dma(address, payload, items) == HAL_MOCK_STATUS_OK;
}

static bool wait_for_completion(uint32_t timeout_ms, bool *transfer_failed, void *context)
{
    (void)timeout_ms;
    I2CTestContext *ctx = (I2CTestContext *)context;

    if (freertos_mock_semaphore_take(&ctx->completion_semaphore) != FREERTOS_MOCK_OK)
    {
        return false;
    }

    *transfer_failed = ctx->transfer_failed;
    return true;
}

static void free_payload(uint8_t *payload, void *context)
{
    I2CTestContext *ctx = (I2CTestContext *)context;
    ctx->free_calls++;
    free(payload);
}

static void delay_ms(uint32_t delay_ms, void *context)
{
    (void)delay_ms;
    I2CTestContext *ctx = (I2CTestContext *)context;
    ctx->delay_calls++;
}

static void on_i2c_tx_complete(void *context)
{
    I2CTestContext *ctx = (I2CTestContext *)context;
    ctx->transfer_failed = false;
    freertos_mock_semaphore_give(&ctx->completion_semaphore);
}

static void on_i2c_rx_complete(void *context)
{
    I2CTestContext *ctx = (I2CTestContext *)context;
    ctx->transfer_failed = false;
    freertos_mock_semaphore_give(&ctx->completion_semaphore);
}

static void on_i2c_error(void *context)
{
    I2CTestContext *ctx = (I2CTestContext *)context;
    ctx->transfer_failed = true;
    freertos_mock_semaphore_give(&ctx->completion_semaphore);
}

static I2CHandlerOps make_ops(void)
{
    I2CHandlerOps ops = {
        .is_source_valid = is_source_valid,
        .signal_source_completion = signal_source_completion,
        .is_device_ready = is_device_ready,
        .start_receive = start_receive,
        .start_transmit = start_transmit,
        .wait_for_completion = wait_for_completion,
        .free_payload = free_payload,
        .delay_ms = delay_ms,
    };
    return ops;
}

static I2CHandlerConfig make_config(void)
{
    I2CHandlerConfig config = {
        .trials = 5,
        .blocking_timeout_ms = 100,
        .try_again_delay_ms = 10,
        .drop_budget_ms = 50,
    };
    return config;
}

static I2CTestContext make_context(void)
{
    I2CTestContext ctx = {
        .rom_task = (void *)0xA,
        .display_task = (void *)0xB,
    };

    freertos_mock_semaphore_init(&ctx.completion_semaphore);
    return ctx;
}

static void test_invalid_message_rejected_and_signaled(void)
{
    hal_mock_reset();
    freertos_mock_reset();

    I2CTestContext ctx = make_context();
    I2CHandlerOps ops = make_ops();
    I2CHandlerConfig config = make_config();

    I2CHandlerMessage message = {
        .source_task = (void *)0xDEAD,
        .rx_tx_bar = false,
        .address = 0x3C,
        .payload = malloc(2),
        .items = 2,
        .p_failed = &(bool){false},
    };

    I2CHandlerResult result = i2c_handler_process_message(&message, &config, &ops, &ctx);

    expect_eq_u32(I2C_HANDLER_RESULT_INVALID_MESSAGE, result, "invalid message result");
    expect_true(*(message.p_failed), "invalid message marks failed");
    expect_true(ctx.completion_signaled, "invalid message completion signal");
    expect_eq_u32(1, ctx.free_calls, "invalid tx payload freed");
}

static void test_device_not_ready_retries_until_budget_exhausted(void)
{
    hal_mock_reset();
    freertos_mock_reset();

    g_hal_mock_state.i2c.next_ready_status = HAL_MOCK_STATUS_ERROR;

    I2CTestContext ctx = make_context();
    I2CHandlerOps ops = make_ops();
    I2CHandlerConfig config = make_config();

    bool failed = false;
    I2CHandlerMessage message = {
        .source_task = ctx.rom_task,
        .rx_tx_bar = false,
        .address = 0x3C,
        .payload = malloc(1),
        .items = 1,
        .p_failed = &failed,
    };

    I2CHandlerResult result = i2c_handler_process_message(&message, &config, &ops, &ctx);

    expect_eq_u32(I2C_HANDLER_RESULT_DEVICE_NOT_READY, result, "device not ready result");
    expect_true(failed, "device not ready marks failed");
    expect_true(ctx.completion_signaled, "device not ready completion signal");
    expect_true(ctx.delay_calls > 0, "device not ready retries");
    expect_eq_u32(1, ctx.free_calls, "device not ready frees tx payload");
}

static void test_transmit_success_clears_failed_and_frees_payload(void)
{
    hal_mock_reset();
    freertos_mock_reset();

    I2CTestContext ctx = make_context();
    I2CHandlerOps ops = make_ops();
    I2CHandlerConfig config = make_config();

    HalMockI2CCallbacks callbacks = {
        .tx_complete = on_i2c_tx_complete,
        .rx_complete = on_i2c_rx_complete,
        .error = on_i2c_error,
        .context = &ctx,
    };

    hal_mock_set_i2c_callbacks(callbacks);
    g_hal_mock_state.i2c.auto_invoke_tx_complete = true;

    bool failed = true;
    uint8_t *payload = malloc(3);
    payload[0] = 1;
    payload[1] = 2;
    payload[2] = 3;

    I2CHandlerMessage message = {
        .source_task = ctx.rom_task,
        .rx_tx_bar = false,
        .address = 0xA0,
        .payload = payload,
        .items = 3,
        .p_failed = &failed,
    };

    I2CHandlerResult result = i2c_handler_process_message(&message, &config, &ops, &ctx);

    expect_eq_u32(I2C_HANDLER_RESULT_SUCCESS, result, "tx success result");
    expect_true(!failed, "tx success clears failed");
    expect_eq_u32(1, g_hal_mock_state.i2c.tx_count, "tx success transmit called");
    expect_eq_u32(1, ctx.free_calls, "tx success frees payload");
}

static void test_receive_error_sets_failed_without_free(void)
{
    hal_mock_reset();
    freertos_mock_reset();

    I2CTestContext ctx = make_context();
    I2CHandlerOps ops = make_ops();
    I2CHandlerConfig config = make_config();

    HalMockI2CCallbacks callbacks = {
        .tx_complete = on_i2c_tx_complete,
        .rx_complete = on_i2c_rx_complete,
        .error = on_i2c_error,
        .context = &ctx,
    };

    hal_mock_set_i2c_callbacks(callbacks);
    g_hal_mock_state.i2c.next_rx_status = HAL_MOCK_STATUS_ERROR;
    g_hal_mock_state.i2c.auto_invoke_error = true;

    bool failed = false;
    uint8_t payload[4] = {0};

    I2CHandlerMessage message = {
        .source_task = ctx.rom_task,
        .rx_tx_bar = true,
        .address = 0xA1,
        .payload = payload,
        .items = 4,
        .p_failed = &failed,
    };

    I2CHandlerResult result = i2c_handler_process_message(&message, &config, &ops, &ctx);

    expect_eq_u32(I2C_HANDLER_RESULT_TRANSFER_FAILED, result, "rx error result");
    expect_true(failed, "rx error marks failed");
    expect_eq_u32(0, ctx.free_calls, "rx path does not free payload");
}

int main(void)
{
    test_invalid_message_rejected_and_signaled();
    test_device_not_ready_retries_until_budget_exhausted();
    test_transmit_success_clears_failed_and_frees_payload();
    test_receive_error_sets_failed_without_free();

    if (failures != 0)
    {
        fprintf(stderr, "Firmware i2c handler tests failed: %d\n", failures);
        return 1;
    }

    puts("Firmware i2c handler tests passed");
    return 0;
}
