#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "i2c_handler.h"
#include "mocks/freertos_mock.h"
#include "mocks/hal_mock.h"

#include "harness/expect.h"

int failures = 0;

typedef struct {
    void *romTask;
    void *displayTask;
    bool completionSignaled;
    uint32_t delayCalls;
    uint32_t freeCalls;
    bool transferFailed;
    FreeRtosMockBinarySemaphore completionSemaphore;
} I2CTestContext;

static bool is_source_valid(void *sourceTask, void *context) {
    I2CTestContext *ctx = (I2CTestContext *)context;
    return sourceTask == ctx->romTask || sourceTask == ctx->displayTask;
}

static void signal_source_completion(void *sourceTask, void *context) {
    (void)sourceTask;
    I2CTestContext *ctx = (I2CTestContext *)context;
    ctx->completionSignaled = true;
}

static bool is_device_ready(uint16_t address, uint32_t trials, uint32_t timeoutMs, void *context) {
    (void)context;
    return hal_mock_i2c_is_device_ready(address, trials, timeoutMs) == HAL_MOCK_STATUS_OK;
}

static bool start_receive(uint16_t address, uint8_t *payload, uint16_t items, void *context) {
    (void)context;
    return hal_mock_i2c_master_receive_dma(address, payload, items) == HAL_MOCK_STATUS_OK;
}

static bool start_transmit(uint16_t address, uint8_t *payload, uint16_t items, void *context) {
    (void)context;
    return hal_mock_i2c_master_transmit_dma(address, payload, items) == HAL_MOCK_STATUS_OK;
}

static bool wait_for_completion(uint32_t timeoutMs, bool *transferFailed, void *context) {
    (void)timeoutMs;
    I2CTestContext *ctx = (I2CTestContext *)context;

    if (freertos_mock_semaphore_take(&ctx->completionSemaphore) != FREERTOS_MOCK_OK) {
        return false;
    }

    *transferFailed = ctx->transferFailed;
    return true;
}

static void free_payload(uint8_t *payload, void *context) {
    I2CTestContext *ctx = (I2CTestContext *)context;
    ctx->freeCalls++;
    free(payload);
}

static void delay_ms(uint32_t delayMs, void *context) {
    (void)delayMs;
    I2CTestContext *ctx = (I2CTestContext *)context;
    ctx->delayCalls++;
}

static void on_i2c_tx_complete(void *context) {
    I2CTestContext *ctx = (I2CTestContext *)context;
    ctx->transferFailed = false;
    freertos_mock_semaphore_give(&ctx->completionSemaphore);
}

static void on_i2c_rx_complete(void *context) {
    I2CTestContext *ctx = (I2CTestContext *)context;
    ctx->transferFailed = false;
    freertos_mock_semaphore_give(&ctx->completionSemaphore);
}

static void on_i2c_error(void *context) {
    I2CTestContext *ctx = (I2CTestContext *)context;
    ctx->transferFailed = true;
    freertos_mock_semaphore_give(&ctx->completionSemaphore);
}

static I2CHandlerOps make_ops(void) {
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

static I2CHandlerConfig make_config(void) {
    I2CHandlerConfig config = {
        .trials = 5,
        .blockingTimeoutMs = 100,
        .tryAgainDelayMs = 10,
        .dropBudgetMs = 50,
    };
    return config;
}

static I2CTestContext make_context(void) {
    I2CTestContext ctx = {
        .romTask = (void *)0xA,
        .displayTask = (void *)0xB,
    };

    freertos_mock_semaphore_init(&ctx.completionSemaphore);
    return ctx;
}

static void test_invalid_message_rejected_and_signaled(void) {
    hal_mock_reset();
    freertos_mock_reset();

    I2CTestContext ctx = make_context();
    I2CHandlerOps ops = make_ops();
    I2CHandlerConfig config = make_config();

    I2CHandlerMessage message = {
        .sourceTask = (void *)0xDEAD,
        .rxTxBar = false,
        .address = 0x3C,
        .payload = malloc(2),
        .items = 2,
        .pFailed = &(bool){false},
    };

    I2CHandlerResult result = i2c_handler_process_message(&message, &config, &ops, &ctx);

    expect_eq_u32(I2C_HANDLER_RESULT_INVALID_MESSAGE, result, "invalid message result");
    expect_true(*(message.pFailed), "invalid message marks failed");
    expect_true(ctx.completionSignaled, "invalid message completion signal");
    expect_eq_u32(1, ctx.freeCalls, "invalid tx payload freed");
}

static void test_device_not_ready_retries_until_budget_exhausted(void) {
    hal_mock_reset();
    freertos_mock_reset();

    g_halMockState.i2c.nextReadyStatus = HAL_MOCK_STATUS_ERROR;

    I2CTestContext ctx = make_context();
    I2CHandlerOps ops = make_ops();
    I2CHandlerConfig config = make_config();

    bool failed = false;
    I2CHandlerMessage message = {
        .sourceTask = ctx.romTask,
        .rxTxBar = false,
        .address = 0x3C,
        .payload = malloc(1),
        .items = 1,
        .pFailed = &failed,
    };

    I2CHandlerResult result = i2c_handler_process_message(&message, &config, &ops, &ctx);

    expect_eq_u32(I2C_HANDLER_RESULT_DEVICE_NOT_READY, result, "device not ready result");
    expect_true(failed, "device not ready marks failed");
    expect_true(ctx.completionSignaled, "device not ready completion signal");
    expect_true(ctx.delayCalls > 0, "device not ready retries");
    expect_eq_u32(1, ctx.freeCalls, "device not ready frees tx payload");
}

static void test_transmit_success_clears_failed_and_frees_payload(void) {
    hal_mock_reset();
    freertos_mock_reset();

    I2CTestContext ctx = make_context();
    I2CHandlerOps ops = make_ops();
    I2CHandlerConfig config = make_config();

    HalMockI2CCallbacks callbacks = {
        .txComplete = on_i2c_tx_complete,
        .rxComplete = on_i2c_rx_complete,
        .error = on_i2c_error,
        .context = &ctx,
    };

    hal_mock_set_i2c_callbacks(callbacks);
    g_halMockState.i2c.autoInvokeTxComplete = true;

    bool failed = true;
    uint8_t *payload = malloc(3);
    payload[0] = 1;
    payload[1] = 2;
    payload[2] = 3;

    I2CHandlerMessage message = {
        .sourceTask = ctx.romTask,
        .rxTxBar = false,
        .address = 0xA0,
        .payload = payload,
        .items = 3,
        .pFailed = &failed,
    };

    I2CHandlerResult result = i2c_handler_process_message(&message, &config, &ops, &ctx);

    expect_eq_u32(I2C_HANDLER_RESULT_SUCCESS, result, "tx success result");
    expect_true(!failed, "tx success clears failed");
    expect_eq_u32(1, g_halMockState.i2c.txCount, "tx success transmit called");
    expect_eq_u32(1, ctx.freeCalls, "tx success frees payload");
}

static void test_receive_error_sets_failed_without_free(void) {
    hal_mock_reset();
    freertos_mock_reset();

    I2CTestContext ctx = make_context();
    I2CHandlerOps ops = make_ops();
    I2CHandlerConfig config = make_config();

    HalMockI2CCallbacks callbacks = {
        .txComplete = on_i2c_tx_complete,
        .rxComplete = on_i2c_rx_complete,
        .error = on_i2c_error,
        .context = &ctx,
    };

    hal_mock_set_i2c_callbacks(callbacks);
    g_halMockState.i2c.nextRxStatus = HAL_MOCK_STATUS_ERROR;
    g_halMockState.i2c.autoInvokeError = true;

    bool failed = false;
    uint8_t payload[4] = {0};

    I2CHandlerMessage message = {
        .sourceTask = ctx.romTask,
        .rxTxBar = true,
        .address = 0xA1,
        .payload = payload,
        .items = 4,
        .pFailed = &failed,
    };

    I2CHandlerResult result = i2c_handler_process_message(&message, &config, &ops, &ctx);

    expect_eq_u32(I2C_HANDLER_RESULT_TRANSFER_FAILED, result, "rx error result");
    expect_true(failed, "rx error marks failed");
    expect_eq_u32(0, ctx.freeCalls, "rx path does not free payload");
}

int main(void) {
    test_invalid_message_rejected_and_signaled();
    test_device_not_ready_retries_until_budget_exhausted();
    test_transmit_success_clears_failed_and_frees_payload();
    test_receive_error_sets_failed_without_free();

    if (failures != 0) {
        fprintf(stderr, "Firmware i2c handler tests failed: %d\n", failures);
        return 1;
    }

    puts("Firmware i2c handler tests passed");
    return 0;
}
