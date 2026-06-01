#include "host_ops.h"

#include <stddef.h>

typedef struct
{
    const HostHarness *harness;
} HostDispatchOpsContext;

static HostDispatchOpsContext g_dispatchOpsContext;

static bool host_semaphore_take(void *semaphoreHandle, uint32_t timeoutTicks, void *context)
{
    (void)semaphoreHandle;
    (void)timeoutTicks;

    HostDispatchOpsContext *ctx = (HostDispatchOpsContext *)context;
    if (ctx == NULL || ctx->harness == NULL)
    {
        return false;
    }

    return freertos_mock_semaphore_take(&ctx->harness->rtos->semaphore) == FREERTOS_MOCK_OK;
}

static void host_task_notify_from_isr(void *taskHandle, uint32_t value, void *context)
{
    (void)taskHandle;

    HostDispatchOpsContext *ctx = (HostDispatchOpsContext *)context;
    if (ctx == NULL || ctx->harness == NULL)
    {
        return;
    }

    freertos_mock_task_notify(&ctx->harness->rtos->notify, value);
}

static void host_semaphore_give_from_isr(void *semaphoreHandle, void *context)
{
    (void)semaphoreHandle;

    HostDispatchOpsContext *ctx = (HostDispatchOpsContext *)context;
    if (ctx == NULL || ctx->harness == NULL)
    {
        return;
    }

    freertos_mock_semaphore_give(&ctx->harness->rtos->semaphore);
}

static bool host_dma_start_it(void *dmaHandle, uint32_t srcAddress, uint32_t dstAddress,
    uint32_t dataLength, void *context)
{
    (void)dmaHandle;
    (void)srcAddress;
    (void)dstAddress;
    (void)dataLength;

    HostDispatchOpsContext *ctx = (HostDispatchOpsContext *)context;
    if (ctx == NULL || ctx->harness == NULL)
    {
        return false;
    }

    return !ctx->harness->hal->adc.nextStartStatus;
}

static bool host_adc_config_channel(void *adcHandle, void *config, void *context)
{
    (void)adcHandle;
    (void)config;
    (void)context;
    return true;
}

static bool host_adc_start_it(void *adcHandle, void *context)
{
    (void)adcHandle;
    (void)context;
    return true;
}

static uint32_t host_adc_get_value(void *adcHandle, void *context)
{
    (void)adcHandle;

    HostDispatchOpsContext *ctx = (HostDispatchOpsContext *)context;
    if (ctx == NULL || ctx->harness == NULL)
    {
        return 0;
    }

    return ctx->harness->potSamples[0];
}

static void host_i2c_signal_completion_from_isr(void *i2cTaskSupportContext, bool transferFailed,
    void *context)
{
    (void)i2cTaskSupportContext;
    (void)transferFailed;

    HostDispatchOpsContext *ctx = (HostDispatchOpsContext *)context;
    if (ctx == NULL || ctx->harness == NULL)
    {
        return;
    }

    freertos_mock_semaphore_give(&ctx->harness->rtos->semaphore);
}

void host_ops_init_peripheral_dispatch(const HostHarness *harness, PeripheralDispatchContext *dispatch,
    PeripheralDispatchOps *ops)
{
    if (harness == NULL || dispatch == NULL || ops == NULL)
    {
        return;
    }

    *dispatch = (PeripheralDispatchContext){
        .effectsTaskHandle = harness->effectsTaskHandle,
        .potHandlerTaskHandle = harness->potTaskHandle,
        .btnHandlerTaskHandle = harness->buttonTaskHandle,
        .effectsAdcHandle = harness->adcBufA,
        .potAdcHandle = harness->adcBufB,
        .i2cHandle = harness->i2cTaskHandle,
        .memToMemDmaHandle = harness->i2cTaskHandle,
        .i2cTaskSupportContext = NULL,
        .delaySamplesDmaSemaphoreHandle = &harness->rtos->semaphore,
        .adcBufA = harness->adcBufA,
        .adcBufB = harness->adcBufB,
        .dacBufA = harness->dacBufA,
        .dacBufB = harness->dacBufB,
    };

    g_dispatchOpsContext.harness = harness;

    *ops = (PeripheralDispatchOps){
        .semaphore_take = host_semaphore_take,
        .task_notify_from_isr = host_task_notify_from_isr,
        .semaphore_give_from_isr = host_semaphore_give_from_isr,
        .dma_start_it = host_dma_start_it,
        .adc_config_channel = host_adc_config_channel,
        .adc_start_it = host_adc_start_it,
        .adc_get_value = host_adc_get_value,
        .i2c_signal_completion_from_isr = host_i2c_signal_completion_from_isr,
        .context = &g_dispatchOpsContext,
    };
}
