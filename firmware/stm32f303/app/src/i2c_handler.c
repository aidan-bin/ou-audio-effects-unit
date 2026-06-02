#include "i2c_handler.h"

#include <stddef.h>

static I2CHandlerResult finish_message(const I2CHandlerMessage *message, bool failed,
                                       I2CHandlerResult result, const I2CHandlerOps *ops,
                                       void *context) {
    if (message->pFailed != NULL) {
        *(message->pFailed) = failed;
    }

    if (!message->rxTxBar && message->payload != NULL && ops->free_payload != NULL) {
        ops->free_payload(message->payload, context);
    }

    if (ops->signal_source_completion != NULL) {
        ops->signal_source_completion(message->sourceTask, context);
    }

    return result;
}

static bool message_valid(const I2CHandlerMessage *message, const I2CHandlerOps *ops,
                          void *context) {
    if (message == NULL || ops == NULL) {
        return false;
    }

    if (message->pFailed == NULL || message->payload == NULL || message->items == 0) {
        return false;
    }

    if (ops->is_source_valid == NULL) {
        return false;
    }

    return ops->is_source_valid(message->sourceTask, context);
}

I2CHandlerResult i2c_handler_process_message(const I2CHandlerMessage *message,
                                             const I2CHandlerConfig *config,
                                             const I2CHandlerOps *ops, void *context) {
    if (message == NULL || config == NULL || ops == NULL) {
        return I2C_HANDLER_RESULT_INVALID_MESSAGE;
    }

    if (ops->is_device_ready == NULL || ops->start_receive == NULL || ops->start_transmit == NULL ||
        ops->wait_for_completion == NULL) {
        return I2C_HANDLER_RESULT_INVALID_MESSAGE;
    }

    if (!message_valid(message, ops, context)) {
        if (message->pFailed != NULL) {
            *(message->pFailed) = true;
        }

        if (!message->rxTxBar && message->payload != NULL && ops->free_payload != NULL) {
            ops->free_payload(message->payload, context);
        }

        if (ops->signal_source_completion != NULL) {
            ops->signal_source_completion(message->sourceTask, context);
        }

        return I2C_HANDLER_RESULT_INVALID_MESSAGE;
    }

    uint32_t remainingBudgetMs = config->dropBudgetMs;
    bool ready = false;

    while (!ready) {
        ready = ops->is_device_ready(message->address, config->trials, config->blockingTimeoutMs,
                                     context);

        if (ready) {
            break;
        }

        if (config->tryAgainDelayMs == 0 || remainingBudgetMs <= config->tryAgainDelayMs) {
            return finish_message(message, true, I2C_HANDLER_RESULT_DEVICE_NOT_READY, ops, context);
        }

        if (ops->delay_ms != NULL) {
            ops->delay_ms(config->tryAgainDelayMs, context);
        }

        remainingBudgetMs -= config->tryAgainDelayMs;
    }

    bool transferStarted = false;

    if (message->rxTxBar) {
        transferStarted =
            ops->start_receive(message->address, message->payload, message->items, context);
    } else {
        transferStarted =
            ops->start_transmit(message->address, message->payload, message->items, context);
    }

    if (!transferStarted) {
        return finish_message(message, true, I2C_HANDLER_RESULT_TRANSFER_FAILED, ops, context);
    }

    bool transferFailed = true;
    bool transferCompleted = ops->wait_for_completion(remainingBudgetMs, &transferFailed, context);

    if (!transferCompleted || transferFailed) {
        return finish_message(message, true, I2C_HANDLER_RESULT_TRANSFER_FAILED, ops, context);
    }

    return finish_message(message, false, I2C_HANDLER_RESULT_SUCCESS, ops, context);
}
