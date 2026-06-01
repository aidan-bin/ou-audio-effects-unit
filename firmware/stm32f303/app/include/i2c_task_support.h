#ifndef I2C_TASK_SUPPORT_H
#define I2C_TASK_SUPPORT_H

#include <stdbool.h>
#include <stdint.h>

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

bool i2c_task_source_is_valid(const I2CTaskSupportContext *context, osThreadId sourceTask);
void i2c_task_signal_source_completion(const I2CTaskSupportContext *context, osThreadId sourceTask);

bool i2c_task_queue_message_and_wait(osMessageQId i2cQueueHandle, void *message, bool *pFailed,
    osSemaphoreId completionSemaphoreHandle, TickType_t timeoutTicks);

void i2c_task_signal_completion_from_isr(const I2CTaskSupportContext *context, bool transferFailed);

bool i2c_task_source_is_valid_adapter(void *sourceTask, void *context);
void i2c_task_signal_source_completion_adapter(void *sourceTask, void *context);

bool i2c_task_hal_is_device_ready(uint16_t address, uint32_t trials, uint32_t timeoutMs, void *context);
bool i2c_task_hal_start_receive(uint16_t address, uint8_t *payload, uint16_t items, void *context);
bool i2c_task_hal_start_transmit(uint16_t address, uint8_t *payload, uint16_t items, void *context);
bool i2c_task_wait_for_completion(uint32_t timeoutMs, bool *transferFailed, void *context);

void i2c_task_free_payload(uint8_t *payload, void *context);
void i2c_task_delay_ms(uint32_t delayMs, void *context);

#endif
