#include "i2c_task_support.h"
#include "peripheral_dispatch.h"

#include "cmsis_os.h"

static bool semaphore_take_adapter(void *semaphoreHandle, uint32_t timeoutTicks, void *context)
{
    (void)context;
    return xSemaphoreTake((osSemaphoreId)semaphoreHandle, (TickType_t)timeoutTicks) == pdTRUE;
}

static void drain_semaphore(osSemaphoreId semaphoreHandle)
{
    PeripheralDispatchOps ops = {
        .semaphore_take = semaphore_take_adapter,
        .task_notify_from_isr = NULL,
        .semaphore_give_from_isr = NULL,
        .dma_start_it = NULL,
        .adc_config_channel = NULL,
        .adc_start_it = NULL,
        .adc_get_value = NULL,
        .i2c_signal_completion_from_isr = NULL,
        .context = NULL,
    };

    peripheral_drain_semaphore(semaphoreHandle, &ops);
}

static void give_semaphore_from_isr(osSemaphoreId semaphoreHandle)
{
    if (semaphoreHandle == NULL)
    {
        return;
    }

    BaseType_t higherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(semaphoreHandle, &higherPriorityTaskWoken);
    portYIELD_FROM_ISR(higherPriorityTaskWoken);
}

bool i2c_task_source_is_valid(void *sourceTask, void *context)
{
    const I2CTaskSupportContext *ctx = (const I2CTaskSupportContext *)context;

    if (ctx == NULL)
    {
        return false;
    }

    osThreadId thread = (osThreadId)sourceTask;
    return thread == ctx->romHandlerTaskHandle || thread == ctx->displayHandlerTaskHandle;
}

void i2c_task_signal_source_completion(void *sourceTask, void *context)
{
    const I2CTaskSupportContext *ctx = (const I2CTaskSupportContext *)context;

    if (ctx == NULL)
    {
        return;
    }

    osThreadId thread = (osThreadId)sourceTask;
    if (thread == ctx->romHandlerTaskHandle)
    {
        xSemaphoreGive(ctx->i2cFailedRomSemaphoreHandle);
    }
    else if (thread == ctx->displayHandlerTaskHandle)
    {
        xSemaphoreGive(ctx->i2cFailedDisplaySemaphoreHandle);
    }
}

bool i2c_task_queue_message_and_wait(void *queueHandle, void *message, bool *pFailed,
    void *failedSemaphoreHandle, uint32_t timeoutTicks, void *context)
{
    (void)context;

    osMessageQId i2cQueueHandle = (osMessageQId)queueHandle;
    osSemaphoreId completionSemaphoreHandle = (osSemaphoreId)failedSemaphoreHandle;

    if (message == NULL || pFailed == NULL || completionSemaphoreHandle == NULL || i2cQueueHandle == NULL)
    {
        return false;
    }

    drain_semaphore(completionSemaphoreHandle);
    *pFailed = true;

    if (xQueueSend(i2cQueueHandle, message, (TickType_t)timeoutTicks) != pdPASS)
    {
        return false;
    }

    if (xSemaphoreTake(completionSemaphoreHandle, (TickType_t)timeoutTicks) != pdTRUE)
    {
        return false;
    }

    return !(*pFailed);
}

void i2c_task_signal_completion_from_isr(const I2CTaskSupportContext *context, bool transferFailed)
{
    if (context == NULL || context->i2cTransferFailed == NULL)
    {
        return;
    }

    *(context->i2cTransferFailed) = transferFailed;
    give_semaphore_from_isr(context->i2cCompletionSemaphoreHandle);
}

bool i2c_task_hal_is_device_ready(uint16_t address, uint32_t trials, uint32_t timeoutMs, void *context)
{
    const I2CTaskSupportContext *ctx = (const I2CTaskSupportContext *)context;

    if (ctx == NULL || ctx->hi2c == NULL)
    {
        return false;
    }

    return HAL_I2C_IsDeviceReady(ctx->hi2c, address, trials, timeoutMs) == HAL_OK;
}

bool i2c_task_hal_start_receive(uint16_t address, uint8_t *payload, uint16_t items, void *context)
{
    I2CTaskSupportContext *ctx = (I2CTaskSupportContext *)context;

    if (ctx == NULL || ctx->hi2c == NULL || ctx->i2cTransferFailed == NULL)
    {
        return false;
    }

    drain_semaphore(ctx->i2cCompletionSemaphoreHandle);
    *(ctx->i2cTransferFailed) = true;

    return HAL_I2C_Master_Receive_IT(ctx->hi2c, address, payload, items) == HAL_OK;
}

bool i2c_task_hal_start_transmit(uint16_t address, uint8_t *payload, uint16_t items, void *context)
{
    I2CTaskSupportContext *ctx = (I2CTaskSupportContext *)context;

    if (ctx == NULL || ctx->hi2c == NULL || ctx->i2cTransferFailed == NULL)
    {
        return false;
    }

    drain_semaphore(ctx->i2cCompletionSemaphoreHandle);
    *(ctx->i2cTransferFailed) = true;

    return HAL_I2C_Master_Transmit_IT(ctx->hi2c, address, payload, items) == HAL_OK;
}

bool i2c_task_wait_for_completion(uint32_t timeoutMs, bool *transferFailed, void *context)
{
    const I2CTaskSupportContext *ctx = (const I2CTaskSupportContext *)context;

    if (ctx == NULL || transferFailed == NULL || ctx->i2cTransferFailed == NULL)
    {
        return false;
    }

    if (xSemaphoreTake(ctx->i2cCompletionSemaphoreHandle, pdMS_TO_TICKS(timeoutMs)) != pdTRUE)
    {
        return false;
    }

    *transferFailed = *(ctx->i2cTransferFailed);
    return true;
}

void i2c_task_free_payload(uint8_t *payload, void *context)
{
    (void)context;

    if (payload != NULL)
    {
        vPortFree(payload);
    }
}

void i2c_task_delay_ms(uint32_t delayMs, void *context)
{
    (void)context;
    osDelay(delayMs);
}
