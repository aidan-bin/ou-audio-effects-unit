#include "i2c_task_support.h"
#include "peripheral_dispatch.h"

#include "cmsis_os.h"

static bool semaphore_take_adapter(void *semaphore_handle, uint32_t timeout_ticks, void *context)
{
    (void)context;
    return xSemaphoreTake((osSemaphoreId)semaphore_handle, (TickType_t)timeout_ticks) == pdTRUE;
}

static void drain_semaphore(osSemaphoreId semaphore_handle)
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

    peripheral_drain_semaphore(semaphore_handle, &ops);
}

static void give_semaphore_from_isr(osSemaphoreId semaphore_handle)
{
    if (semaphore_handle == NULL)
    {
        return;
    }

    BaseType_t higher_priority_task_woken = pdFALSE;
    xSemaphoreGiveFromISR(semaphore_handle, &higher_priority_task_woken);
    portYIELD_FROM_ISR(higher_priority_task_woken);
}

void i2c_task_support_init(I2CTaskSupportContext *support_context, I2CHandlerConfig *handler_config,
                           I2CHandlerOps *handler_ops, I2C_HandleTypeDef *hi2c,
                           osThreadId rom_handler_task_handle, osThreadId display_handler_task_handle,
                           osSemaphoreId i2c_completion_semaphore_handle,
                           osSemaphoreId i2c_failed_rom_semaphore_handle,
                           osSemaphoreId i2c_failed_display_semaphore_handle,
                           volatile bool *i2c_transfer_failed)
{
    if (support_context == NULL || handler_config == NULL || handler_ops == NULL)
    {
        return;
    }

    support_context->hi2c = hi2c;
    support_context->romHandlerTaskHandle = rom_handler_task_handle;
    support_context->displayHandlerTaskHandle = display_handler_task_handle;
    support_context->i2cCompletionSemaphoreHandle = i2c_completion_semaphore_handle;
    support_context->i2cFailedRomSemaphoreHandle = i2c_failed_rom_semaphore_handle;
    support_context->i2cFailedDisplaySemaphoreHandle = i2c_failed_display_semaphore_handle;
    support_context->i2cTransferFailed = i2c_transfer_failed;

    handler_config->trials = 5;
    handler_config->blockingTimeoutMs = 100;
    handler_config->tryAgainDelayMs = 100;
    handler_config->dropBudgetMs = 5000;

    handler_ops->is_source_valid = i2c_task_source_is_valid;
    handler_ops->signal_source_completion = i2c_task_signal_source_completion;
    handler_ops->is_device_ready = i2c_task_hal_is_device_ready;
    handler_ops->start_receive = i2c_task_hal_start_receive;
    handler_ops->start_transmit = i2c_task_hal_start_transmit;
    handler_ops->wait_for_completion = i2c_task_wait_for_completion;
    handler_ops->free_payload = i2c_task_free_payload;
    handler_ops->delay_ms = i2c_task_delay_ms;
}

bool i2c_task_source_is_valid(void *source_task, void *context)
{
    const I2CTaskSupportContext *ctx = (const I2CTaskSupportContext *)context;

    if (ctx == NULL)
    {
        return false;
    }

    osThreadId thread = (osThreadId)source_task;
    return thread == ctx->romHandlerTaskHandle || thread == ctx->displayHandlerTaskHandle;
}

void i2c_task_signal_source_completion(void *source_task, void *context)
{
    const I2CTaskSupportContext *ctx = (const I2CTaskSupportContext *)context;

    if (ctx == NULL)
    {
        return;
    }

    osThreadId thread = (osThreadId)source_task;
    if (thread == ctx->romHandlerTaskHandle)
    {
        xSemaphoreGive(ctx->i2cFailedRomSemaphoreHandle);
    }
    else if (thread == ctx->displayHandlerTaskHandle)
    {
        xSemaphoreGive(ctx->i2cFailedDisplaySemaphoreHandle);
    }
}

bool i2c_task_queue_message_and_wait(void *queue_handle, void *message, bool *p_failed,
                                     void *failed_semaphore_handle, uint32_t timeout_ticks,
                                     void *context)
{
    (void)context;

    osMessageQId i2c_queue_handle = (osMessageQId)queue_handle;
    osSemaphoreId completion_semaphore_handle = (osSemaphoreId)failed_semaphore_handle;

    if (message == NULL || p_failed == NULL || completion_semaphore_handle == NULL ||
        i2c_queue_handle == NULL)
    {
        return false;
    }

    drain_semaphore(completion_semaphore_handle);
    *p_failed = true;

    if (xQueueSend(i2c_queue_handle, message, (TickType_t)timeout_ticks) != pdPASS)
    {
        return false;
    }

    if (xSemaphoreTake(completion_semaphore_handle, (TickType_t)timeout_ticks) != pdTRUE)
    {
        return false;
    }

    return !(*p_failed);
}

void i2c_task_signal_completion_from_isr(const I2CTaskSupportContext *context,
                                         bool transfer_failed)
{
    if (context == NULL || context->i2cTransferFailed == NULL)
    {
        return;
    }

    *(context->i2cTransferFailed) = transfer_failed;
    give_semaphore_from_isr(context->i2cCompletionSemaphoreHandle);
}

bool i2c_task_hal_is_device_ready(uint16_t address, uint32_t trials, uint32_t timeout_ms,
                                  void *context)
{
    const I2CTaskSupportContext *ctx = (const I2CTaskSupportContext *)context;

    if (ctx == NULL || ctx->hi2c == NULL)
    {
        return false;
    }

    return HAL_I2C_IsDeviceReady(ctx->hi2c, address, trials, timeout_ms) == HAL_OK;
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

bool i2c_task_hal_start_transmit(uint16_t address, uint8_t *payload, uint16_t items,
                                 void *context)
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

bool i2c_task_wait_for_completion(uint32_t timeout_ms, bool *transfer_failed, void *context)
{
    const I2CTaskSupportContext *ctx = (const I2CTaskSupportContext *)context;

    if (ctx == NULL || transfer_failed == NULL || ctx->i2cTransferFailed == NULL)
    {
        return false;
    }

    if (xSemaphoreTake(ctx->i2cCompletionSemaphoreHandle, pdMS_TO_TICKS(timeout_ms)) != pdTRUE)
    {
        return false;
    }

    *transfer_failed = *(ctx->i2cTransferFailed);
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

void i2c_task_delay_ms(uint32_t delay_ms, void *context)
{
    (void)context;
    osDelay(delay_ms);
}
