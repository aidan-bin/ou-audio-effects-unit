#include "i2c_handler.h"

#include <stddef.h>

static I2CHandlerResult finish_message(const I2CHandlerMessage *message, bool failed,
                                       I2CHandlerResult result, const I2CHandlerOps *ops,
                                       void *context)
{
    if (message->p_failed != NULL)
    {
        *(message->p_failed) = failed;
    }

    if (!message->rx_tx_bar && message->payload != NULL && ops->free_payload != NULL)
    {
        ops->free_payload(message->payload, context);
    }

    if (ops->signal_source_completion != NULL)
    {
        ops->signal_source_completion(message->source_task, context);
    }

    return result;
}

static bool message_valid(const I2CHandlerMessage *message, const I2CHandlerOps *ops,
                          void *context)
{
    if (message == NULL || ops == NULL)
    {
        return false;
    }

    if (message->p_failed == NULL || message->payload == NULL || message->items == 0)
    {
        return false;
    }

    if (ops->is_source_valid == NULL)
    {
        return false;
    }

    return ops->is_source_valid(message->source_task, context);
}

I2CHandlerResult i2c_handler_process_message(const I2CHandlerMessage *message,
                                             const I2CHandlerConfig *config,
                                             const I2CHandlerOps *ops, void *context)
{
    if (message == NULL || config == NULL || ops == NULL)
    {
        return I2C_HANDLER_RESULT_INVALID_MESSAGE;
    }

    if (ops->is_device_ready == NULL || ops->start_receive == NULL || ops->start_transmit == NULL ||
        ops->wait_for_completion == NULL)
    {
        return I2C_HANDLER_RESULT_INVALID_MESSAGE;
    }

    if (!message_valid(message, ops, context))
    {
        if (message->p_failed != NULL)
        {
            *(message->p_failed) = true;
        }

        if (!message->rx_tx_bar && message->payload != NULL && ops->free_payload != NULL)
        {
            ops->free_payload(message->payload, context);
        }

        if (ops->signal_source_completion != NULL)
        {
            ops->signal_source_completion(message->source_task, context);
        }

        return I2C_HANDLER_RESULT_INVALID_MESSAGE;
    }

    uint32_t remaining_budget_ms = config->drop_budget_ms;
    bool ready = false;

    while (!ready)
    {
        ready = ops->is_device_ready(message->address, config->trials, config->blocking_timeout_ms,
                                     context);

        if (ready)
        {
            break;
        }

        if (config->try_again_delay_ms == 0 || remaining_budget_ms <= config->try_again_delay_ms)
        {
            return finish_message(message, true, I2C_HANDLER_RESULT_DEVICE_NOT_READY, ops, context);
        }

        if (ops->delay_ms != NULL)
        {
            ops->delay_ms(config->try_again_delay_ms, context);
        }

        remaining_budget_ms -= config->try_again_delay_ms;
    }

    bool transfer_started = false;

    if (message->rx_tx_bar)
    {
        transfer_started =
            ops->start_receive(message->address, message->payload, message->items, context);
    }
    else
    {
        transfer_started =
            ops->start_transmit(message->address, message->payload, message->items, context);
    }

    if (!transfer_started)
    {
        return finish_message(message, true, I2C_HANDLER_RESULT_TRANSFER_FAILED, ops, context);
    }

    bool transfer_failed = true;
    bool transfer_completed = ops->wait_for_completion(remaining_budget_ms, &transfer_failed, context);

    if (!transfer_completed || transfer_failed)
    {
        return finish_message(message, true, I2C_HANDLER_RESULT_TRANSFER_FAILED, ops, context);
    }

    return finish_message(message, false, I2C_HANDLER_RESULT_SUCCESS, ops, context);
}
