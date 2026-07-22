/*
 * i2c_handler — I2C message wrapper/sequencing.
 */

#ifndef I2C_HANDLER_H
#define I2C_HANDLER_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    void *source_task;
    bool rx_tx_bar;
    uint16_t address;
    uint8_t *payload;
    uint16_t items;
    bool *p_failed;
} I2CHandlerMessage;

typedef enum
{
    I2C_HANDLER_RESULT_SUCCESS = 0,
    I2C_HANDLER_RESULT_INVALID_MESSAGE = 1,
    I2C_HANDLER_RESULT_DEVICE_NOT_READY = 2,
    I2C_HANDLER_RESULT_TRANSFER_FAILED = 3,
} I2CHandlerResult;

typedef struct
{
    uint32_t trials;
    uint32_t blocking_timeout_ms;
    uint32_t try_again_delay_ms;
    uint32_t drop_budget_ms;
} I2CHandlerConfig;

typedef struct
{
    bool (*is_source_valid)(void *source_task, void *context);
    void (*signal_source_completion)(void *source_task, void *context);

    bool (*is_device_ready)(uint16_t address, uint32_t trials, uint32_t timeout_ms, void *context);
    bool (*start_receive)(uint16_t address, uint8_t *payload, uint16_t items, void *context);
    bool (*start_transmit)(uint16_t address, uint8_t *payload, uint16_t items, void *context);
    bool (*wait_for_completion)(uint32_t timeout_ms, bool *transfer_failed, void *context);

    void (*free_payload)(uint8_t *payload, void *context);
    void (*delay_ms)(uint32_t delay_ms, void *context);
} I2CHandlerOps;

I2CHandlerResult i2c_handler_process_message(const I2CHandlerMessage *message,
                                             const I2CHandlerConfig *config,
                                             const I2CHandlerOps *ops, void *context);

#endif
