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
    osThreadId rom_handler_task_handle;
    osThreadId display_handler_task_handle;
    osSemaphoreId i2c_completion_semaphore_handle;
    osSemaphoreId i2c_failed_rom_semaphore_handle;
    osSemaphoreId i2c_failed_display_semaphore_handle;
    volatile bool *i2c_transfer_failed;
} I2CTaskSupportContext;

void i2c_task_support_init(I2CTaskSupportContext *support_context, I2CHandlerConfig *handler_config,
                           I2CHandlerOps *handler_ops, I2C_HandleTypeDef *hi2c,
                           osThreadId rom_handler_task_handle, osThreadId display_handler_task_handle,
                           osSemaphoreId i2c_completion_semaphore_handle,
                           osSemaphoreId i2c_failed_rom_semaphore_handle,
                           osSemaphoreId i2c_failed_display_semaphore_handle,
                           volatile bool *i2c_transfer_failed);

bool i2c_task_source_is_valid(void *source_task, void *context);
void i2c_task_signal_source_completion(void *source_task, void *context);

bool i2c_task_queue_message_and_wait(void *queue_handle, void *message, bool *p_failed,
                                     void *failed_semaphore_handle, uint32_t timeout_ticks,
                                     void *context);

void i2c_task_signal_completion_from_isr(const I2CTaskSupportContext *context, bool transfer_failed);

bool i2c_task_hal_is_device_ready(uint16_t address, uint32_t trials, uint32_t timeout_ms,
                                  void *context);
bool i2c_task_hal_start_receive(uint16_t address, uint8_t *payload, uint16_t items, void *context);
bool i2c_task_hal_start_transmit(uint16_t address, uint8_t *payload, uint16_t items, void *context);
bool i2c_task_wait_for_completion(uint32_t timeout_ms, bool *transfer_failed, void *context);

void i2c_task_free_payload(uint8_t *payload, void *context);
void i2c_task_delay_ms(uint32_t delay_ms, void *context);

#endif
