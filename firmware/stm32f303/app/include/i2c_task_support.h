#ifndef I2C_TASK_SUPPORT_H
#define I2C_TASK_SUPPORT_H

#include <stdbool.h>
#include <stdint.h>

#include "i2c_handler.h"
#include "cmsis_os.h"
#include "main.h"

typedef struct
{
    I2C_HandleTypeDef *hi2c;
    osThreadId romHandlerTaskHandle;
    osThreadId displayHandlerTaskHandle;
    osSemaphoreId i2cCompletionSemaphoreHandle;
    osSemaphoreId i2cFailedRomSemaphoreHandle;
    osSemaphoreId i2cFailedDisplaySemaphoreHandle;
    volatile bool *i2cTransferFailed;
} I2CTaskSupportContext;

void i2c_task_support_init(I2CTaskSupportContext *supportContext, I2CHandlerConfig *handlerConfig,
    I2CHandlerOps *handlerOps, I2C_HandleTypeDef *hi2c, osThreadId romHandlerTaskHandle,
    osThreadId displayHandlerTaskHandle, osSemaphoreId i2cCompletionSemaphoreHandle,
    osSemaphoreId i2cFailedRomSemaphoreHandle, osSemaphoreId i2cFailedDisplaySemaphoreHandle,
    volatile bool *i2cTransferFailed);

bool i2c_task_source_is_valid(void *sourceTask, void *context);
void i2c_task_signal_source_completion(void *sourceTask, void *context);

bool i2c_task_queue_message_and_wait(void *queueHandle, void *message, bool *pFailed,
    void *failedSemaphoreHandle, uint32_t timeoutTicks, void *context);

void i2c_task_signal_completion_from_isr(const I2CTaskSupportContext *context, bool transferFailed);

bool i2c_task_hal_is_device_ready(uint16_t address, uint32_t trials, uint32_t timeoutMs, void *context);
bool i2c_task_hal_start_receive(uint16_t address, uint8_t *payload, uint16_t items, void *context);
bool i2c_task_hal_start_transmit(uint16_t address, uint8_t *payload, uint16_t items, void *context);
bool i2c_task_wait_for_completion(uint32_t timeoutMs, bool *transferFailed, void *context);

void i2c_task_free_payload(uint8_t *payload, void *context);
void i2c_task_delay_ms(uint32_t delayMs, void *context);

#endif
