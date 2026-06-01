#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "peripheral_dispatch.h"

static int failures = 0;

typedef struct
{
    uint32_t notifyCount;
    void *lastNotifiedTask;
    uint32_t lastNotifyValue;

    uint32_t giveCount;
    void *lastGivenSemaphore;

    uint32_t adcGetCount;
    uint32_t nextAdcValue;

    uint32_t i2cSignalCount;
    void *lastI2CContext;
    bool lastI2CTransferFailed;

    uint32_t semaphoreTakeCount;
    uint32_t semaphoreTakeSuccessesRemaining;

    uint32_t dmaStartCount;
    bool dmaStartResult;

    uint32_t adcConfigCount;
    bool adcConfigResult;

    uint32_t adcStartCount;
    bool adcStartResult;
} PeripheralDispatchTestState;

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

static void expect_ptr(void *expected, void *actual, const char *label)
{
    if (expected != actual)
    {
        fprintf(stderr, "FAIL: %s expected=%p actual=%p\n", label, expected, actual);
        failures++;
    }
}

static bool test_semaphore_take(void *semaphoreHandle, uint32_t timeoutTicks, void *context)
{
    (void)semaphoreHandle;

    PeripheralDispatchTestState *state = (PeripheralDispatchTestState *)context;
    state->semaphoreTakeCount++;

    if (timeoutTicks > 0)
    {
        return true;
    }

    if (state->semaphoreTakeSuccessesRemaining == 0)
    {
        return false;
    }

    state->semaphoreTakeSuccessesRemaining--;
    return true;
}

static void test_notify_task_from_isr(void *taskHandle, uint32_t value, void *context)
{
    PeripheralDispatchTestState *state = (PeripheralDispatchTestState *)context;
    state->notifyCount++;
    state->lastNotifiedTask = taskHandle;
    state->lastNotifyValue = value;
}

static void test_give_semaphore_from_isr(void *semaphoreHandle, void *context)
{
    PeripheralDispatchTestState *state = (PeripheralDispatchTestState *)context;
    state->giveCount++;
    state->lastGivenSemaphore = semaphoreHandle;
}

static bool test_dma_start_it(void *dmaHandle, uint32_t srcAddress, uint32_t dstAddress,
    uint32_t dataLength, void *context)
{
    (void)dmaHandle;
    (void)srcAddress;
    (void)dstAddress;
    (void)dataLength;

    PeripheralDispatchTestState *state = (PeripheralDispatchTestState *)context;
    state->dmaStartCount++;
    return state->dmaStartResult;
}

static bool test_adc_config_channel(void *adcHandle, void *config, void *context)
{
    (void)adcHandle;
    (void)config;

    PeripheralDispatchTestState *state = (PeripheralDispatchTestState *)context;
    state->adcConfigCount++;
    return state->adcConfigResult;
}

static bool test_adc_start_it(void *adcHandle, void *context)
{
    (void)adcHandle;

    PeripheralDispatchTestState *state = (PeripheralDispatchTestState *)context;
    state->adcStartCount++;
    return state->adcStartResult;
}

static uint32_t test_adc_get_value(void *adcHandle, void *context)
{
    (void)adcHandle;

    PeripheralDispatchTestState *state = (PeripheralDispatchTestState *)context;
    state->adcGetCount++;
    return state->nextAdcValue;
}

static void test_i2c_signal_completion_from_isr(void *i2cTaskSupportContext, bool transferFailed,
    void *context)
{
    PeripheralDispatchTestState *state = (PeripheralDispatchTestState *)context;
    state->i2cSignalCount++;
    state->lastI2CContext = i2cTaskSupportContext;
    state->lastI2CTransferFailed = transferFailed;
}

static PeripheralDispatchOps make_ops(PeripheralDispatchTestState *state)
{
    PeripheralDispatchOps ops = {
        .semaphore_take = test_semaphore_take,
        .task_notify_from_isr = test_notify_task_from_isr,
        .semaphore_give_from_isr = test_give_semaphore_from_isr,
        .dma_start_it = test_dma_start_it,
        .adc_config_channel = test_adc_config_channel,
        .adc_start_it = test_adc_start_it,
        .adc_get_value = test_adc_get_value,
        .i2c_signal_completion_from_isr = test_i2c_signal_completion_from_isr,
        .context = state,
    };

    return ops;
}

static PeripheralDispatchContext make_context(void)
{
    PeripheralDispatchContext dispatch = {
        .effectsTaskHandle = (void *)0x1001,
        .potHandlerTaskHandle = (void *)0x1002,
        .btnHandlerTaskHandle = (void *)0x1003,
        .effectsAdcHandle = (void *)0x2001,
        .potAdcHandle = (void *)0x2002,
        .i2cHandle = (void *)0x3001,
        .memToMemDmaHandle = (void *)0x4001,
        .i2cTaskSupportContext = (void *)0x5001,
        .delaySamplesDmaSemaphoreHandle = (void *)0x6001,
        .adcBufA = (void *)0x7001,
        .adcBufB = (void *)0x7002,
        .dacBufA = (void *)0x8001,
        .dacBufB = (void *)0x8002,
    };

    return dispatch;
}

static void test_callbacks_dispatch_expected_notifications(void)
{
    PeripheralDispatchTestState state = {0};
    state.nextAdcValue = 123U;

    PeripheralDispatchOps ops = make_ops(&state);
    PeripheralDispatchContext dispatch = make_context();

    peripheral_on_gpio_exti(&dispatch, &ops, 9U);
    expect_eq_u32(1, state.notifyCount, "exti notifies button task");
    expect_ptr(dispatch.btnHandlerTaskHandle, state.lastNotifiedTask, "exti task handle");
    expect_eq_u32(9U, state.lastNotifyValue, "exti pin value");

    peripheral_on_adc_half_complete(&dispatch, &ops, dispatch.effectsAdcHandle);
    expect_eq_u32(2, state.notifyCount, "adc half complete notifies effects task");
    expect_ptr(dispatch.effectsTaskHandle, state.lastNotifiedTask, "adc half task handle");
    expect_eq_u32((uint32_t)(uintptr_t)dispatch.adcBufA, state.lastNotifyValue, "adc half buffer value");

    peripheral_on_adc_complete(&dispatch, &ops, dispatch.potAdcHandle);
    expect_eq_u32(3, state.notifyCount, "pot adc complete notifies pot task");
    expect_ptr(dispatch.potHandlerTaskHandle, state.lastNotifiedTask, "pot adc task handle");
    expect_eq_u32(123U, state.lastNotifyValue, "pot adc value");
    expect_eq_u32(1, state.adcGetCount, "pot adc reads value once");

    peripheral_on_adc_complete(&dispatch, &ops, dispatch.effectsAdcHandle);
    expect_eq_u32(4, state.notifyCount, "effects adc complete notifies effects task");
    expect_eq_u32((uint32_t)(uintptr_t)dispatch.adcBufB, state.lastNotifyValue, "effects adc buffer value");

    peripheral_on_dac_half_complete(&dispatch, &ops, (void *)0xDEAD);
    expect_eq_u32(5, state.notifyCount, "dac half complete notifies effects task");
    expect_eq_u32((uint32_t)(uintptr_t)dispatch.dacBufA, state.lastNotifyValue, "dac half buffer value");

    peripheral_on_dac_complete(&dispatch, &ops, (void *)0xBEEF);
    expect_eq_u32(6, state.notifyCount, "dac complete notifies effects task");
    expect_eq_u32((uint32_t)(uintptr_t)dispatch.dacBufB, state.lastNotifyValue, "dac complete buffer value");

    peripheral_on_i2c_tx_complete(&dispatch, &ops, dispatch.i2cHandle);
    expect_eq_u32(1, state.i2cSignalCount, "i2c tx signals completion");
    expect_ptr(dispatch.i2cTaskSupportContext, state.lastI2CContext, "i2c context pointer");
    expect_true(!state.lastI2CTransferFailed, "i2c tx marks success");

    peripheral_on_i2c_rx_complete(&dispatch, &ops, dispatch.i2cHandle);
    expect_eq_u32(2, state.i2cSignalCount, "i2c rx signals completion");
    expect_true(!state.lastI2CTransferFailed, "i2c rx marks success");

    peripheral_on_i2c_error(&dispatch, &ops, dispatch.i2cHandle);
    expect_eq_u32(3, state.i2cSignalCount, "i2c error signals completion");
    expect_true(state.lastI2CTransferFailed, "i2c error marks failure");

    peripheral_on_i2c_abort_complete(&dispatch, &ops, dispatch.i2cHandle);
    expect_eq_u32(4, state.i2cSignalCount, "i2c abort signals completion");
    expect_true(state.lastI2CTransferFailed, "i2c abort marks failure");

    peripheral_on_dma_complete(&dispatch, &ops, dispatch.memToMemDmaHandle);
    expect_eq_u32(1, state.giveCount, "dma complete gives semaphore");
    expect_ptr(dispatch.delaySamplesDmaSemaphoreHandle, state.lastGivenSemaphore, "dma semaphore handle");
}

static void test_callbacks_ignore_unrelated_handles(void)
{
    PeripheralDispatchTestState state = {0};
    PeripheralDispatchOps ops = make_ops(&state);
    PeripheralDispatchContext dispatch = make_context();

    peripheral_on_adc_half_complete(&dispatch, &ops, (void *)0xAAAA);
    peripheral_on_adc_complete(&dispatch, &ops, (void *)0xBBBB);
    peripheral_on_i2c_tx_complete(&dispatch, &ops, (void *)0xCCCC);
    peripheral_on_i2c_rx_complete(&dispatch, &ops, (void *)0xCCCC);
    peripheral_on_i2c_error(&dispatch, &ops, (void *)0xCCCC);
    peripheral_on_i2c_abort_complete(&dispatch, &ops, (void *)0xCCCC);
    peripheral_on_dma_complete(&dispatch, &ops, (void *)0xDDDD);

    expect_eq_u32(0, state.notifyCount, "unrelated adc handles ignored");
    expect_eq_u32(0, state.i2cSignalCount, "unrelated i2c handle ignored");
    expect_eq_u32(0, state.giveCount, "unrelated dma handle ignored");
}

static void test_helper_functions(void)
{
    PeripheralDispatchTestState state = {0};
    state.semaphoreTakeSuccessesRemaining = 2;
    state.dmaStartResult = true;
    state.adcConfigResult = true;
    state.adcStartResult = true;

    PeripheralDispatchOps ops = make_ops(&state);

    peripheral_drain_semaphore((void *)0x1111, &ops);
    expect_eq_u32(3, state.semaphoreTakeCount, "drain consumes until empty");

    state.semaphoreTakeSuccessesRemaining = 2;
    bool dmaOk = peripheral_start_dma_transfer_and_wait((void *)0x2222, 1, 2, 3, (void *)0x3333, 4, &ops);
    expect_true(dmaOk, "dma helper succeeds");
    expect_eq_u32(1, state.dmaStartCount, "dma start called once");

    bool adcOk = peripheral_start_adc_conversion_it((void *)0x4444, (void *)0x5555, &ops);
    expect_true(adcOk, "adc helper succeeds");
    expect_eq_u32(1, state.adcConfigCount, "adc config called once");
    expect_eq_u32(1, state.adcStartCount, "adc start called once");
}

int main(void)
{
    test_callbacks_dispatch_expected_notifications();
    test_callbacks_ignore_unrelated_handles();
    test_helper_functions();

    if (failures != 0)
    {
        fprintf(stderr, "test_firmware_peripheral_dispatch: %d failure(s)\n", failures);
        return 1;
    }

    return 0;
}
