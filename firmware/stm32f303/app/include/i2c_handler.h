#ifndef I2C_HANDLER_H
#define I2C_HANDLER_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    void *sourceTask;
    bool rxTxBar;
    uint16_t address;
    uint8_t *payload;
    uint16_t items;
    bool *pFailed;
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
    uint32_t blockingTimeoutMs;
    uint32_t tryAgainDelayMs;
    uint32_t dropBudgetMs;
} I2CHandlerConfig;

typedef struct
{
    bool (*is_source_valid)(void *sourceTask, void *context);
    void (*signal_source_completion)(void *sourceTask, void *context);

    bool (*is_device_ready)(uint16_t address, uint32_t trials, uint32_t timeoutMs, void *context);
    bool (*start_receive)(uint16_t address, uint8_t *payload, uint16_t items, void *context);
    bool (*start_transmit)(uint16_t address, uint8_t *payload, uint16_t items, void *context);
    bool (*wait_for_completion)(uint32_t timeoutMs, bool *transferFailed, void *context);

    void (*free_payload)(uint8_t *payload, void *context);
    void (*delay_ms)(uint32_t delayMs, void *context);
} I2CHandlerOps;

I2CHandlerResult i2c_handler_process_message(const I2CHandlerMessage *message,
                                             const I2CHandlerConfig *config,
                                             const I2CHandlerOps *ops, void *context);

#endif
