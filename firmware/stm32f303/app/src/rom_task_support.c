#include "rom_task_support.h"

#include "log.h"
#include "i2c_handler.h"
#include "rom_handler.h"

void rom_task_support_init(RomTaskSupportConfig *config, RomTaskSupportOps *ops, void *source_task,
                           void *i2c_queue_handle, void *i2c_failed_rom_semaphore_handle,
                           uint16_t device_address, uint16_t read_address, uint16_t write_address,
                           uint32_t bootstrap_timeout_ticks, uint32_t save_timeout_ticks,
                           bool (*queue_message_and_wait)(void *queue_handle, void *message,
                                                          bool *p_failed,
                                                          void *failed_semaphore_handle,
                                                          uint32_t timeout_ticks, void *context),
                           uint8_t *(*allocate_payload)(size_t size, void *context),
                           void (*free_payload)(uint8_t *payload, void *context),
                           void (*set_write_disable)(bool disable_writes, void *context),
                           void *context)
{
    if (config == NULL || ops == NULL)
    {
        return;
    }

    config->sourceTask = source_task;
    config->i2cQueueHandle = i2c_queue_handle;
    config->i2cFailedRomSemaphoreHandle = i2c_failed_rom_semaphore_handle;
    config->deviceAddress = device_address;
    config->readAddress = read_address;
    config->writeAddress = write_address;
    config->bootstrapTimeoutTicks = bootstrap_timeout_ticks;
    config->saveTimeoutTicks = save_timeout_ticks;

    ops->queue_message_and_wait = queue_message_and_wait;
    ops->allocate_payload = allocate_payload;
    ops->free_payload = free_payload;
    ops->set_write_disable = set_write_disable;
    ops->context = context;
}

static bool queue_message(const RomTaskSupportConfig *config, const RomTaskSupportOps *ops,
                          I2CHandlerMessage *message, bool *failed, uint32_t timeout_ticks)
{
    if (config == NULL || ops == NULL || message == NULL || failed == NULL ||
        ops->queue_message_and_wait == NULL)
    {
        return false;
    }

    return ops->queue_message_and_wait(config->i2cQueueHandle, message, failed,
                                       config->i2cFailedRomSemaphoreHandle, timeout_ticks,
                                       ops->context);
}

static uint8_t *allocate_payload(const RomTaskSupportOps *ops, size_t size)
{
    if (ops == NULL || ops->allocate_payload == NULL)
    {
        return NULL;
    }

    return ops->allocate_payload(size, ops->context);
}

static void free_payload(const RomTaskSupportOps *ops, uint8_t *payload)
{
    if (ops != NULL && ops->free_payload != NULL)
    {
        ops->free_payload(payload, ops->context);
    }
}

static void set_write_disable(const RomTaskSupportOps *ops, bool disable_writes)
{
    if (ops != NULL && ops->set_write_disable != NULL)
    {
        ops->set_write_disable(disable_writes, ops->context);
    }
}

bool rom_task_bootstrap_effects(const RomTaskSupportConfig *config, const RomTaskSupportOps *ops,
                                EffectsState *state, EffectsParams *params)
{
    if (config == NULL || ops == NULL || state == NULL || params == NULL)
    {
        (void)log_write(LOG_LEVEL_ERROR, "rom bootstrap invalid args");
        return false;
    }

    (void)log_write(LOG_LEVEL_INFO, "rom bootstrap start");

    const size_t address_payload_size_bytes = 2;
    const size_t payload_size_bytes = rom_handler_read_payload_size();

    bool failed = true;
    I2CHandlerMessage message = {0};
    message.sourceTask = config->sourceTask;
    message.pFailed = &failed;

    message.payload = allocate_payload(ops, address_payload_size_bytes);
    if (message.payload == NULL)
    {
        (void)log_write(LOG_LEVEL_ERROR, "rom bootstrap address payload alloc failed");
        return false;
    }

    if (!rom_handler_encode_address(config->readAddress, message.payload,
                                    address_payload_size_bytes))
    {
        (void)log_write(LOG_LEVEL_ERROR, "rom bootstrap address encode failed");
        free_payload(ops, message.payload);
        return false;
    }

    message.rxTxBar = false;
    message.address = config->deviceAddress;
    message.items = (uint16_t)address_payload_size_bytes;

    set_write_disable(ops, false);
    if (!queue_message(config, ops, &message, message.pFailed, config->bootstrapTimeoutTicks))
    {
        (void)log_write(LOG_LEVEL_ERROR, "rom bootstrap read-address transfer failed");
        set_write_disable(ops, true);
        free_payload(ops, message.payload);
        return false;
    }

    set_write_disable(ops, true);

    message.payload = allocate_payload(ops, payload_size_bytes);
    if (message.payload == NULL)
    {
        (void)log_write(LOG_LEVEL_ERROR, "rom bootstrap read payload alloc failed");
        return false;
    }

    message.rxTxBar = true;
    message.address = config->deviceAddress | 1U;
    message.items = (uint16_t)payload_size_bytes;

    if (!queue_message(config, ops, &message, message.pFailed, config->bootstrapTimeoutTicks))
    {
        (void)log_write(LOG_LEVEL_ERROR, "rom bootstrap read transfer failed");
        free_payload(ops, message.payload);
        return false;
    }

    EffectsState loaded_state;
    EffectsParams loaded_params;

    if (!rom_handler_decode_read_payload(message.payload, payload_size_bytes, &loaded_state,
                                         &loaded_params))
    {
        (void)log_write(LOG_LEVEL_ERROR, "rom bootstrap decode failed");
        free_payload(ops, message.payload);
        return false;
    }

    free_payload(ops, message.payload);
    *state = loaded_state;
    *params = loaded_params;
    (void)log_write(LOG_LEVEL_INFO, "rom bootstrap complete");
    return true;
}

bool rom_task_save_effects(const RomTaskSupportConfig *config, const RomTaskSupportOps *ops,
                           const EffectsState *state, const EffectsParams *params)
{
    if (config == NULL || ops == NULL || state == NULL || params == NULL)
    {
        (void)log_write(LOG_LEVEL_ERROR, "rom save invalid args");
        return false;
    }

    (void)log_write(LOG_LEVEL_INFO, "rom save start");

    const size_t payload_size_bytes = rom_handler_write_payload_size();

    bool failed = true;
    I2CHandlerMessage message = {0};
    message.sourceTask = config->sourceTask;
    message.pFailed = &failed;

    message.payload = allocate_payload(ops, payload_size_bytes);
    if (message.payload == NULL)
    {
        (void)log_write(LOG_LEVEL_ERROR, "rom save payload alloc failed");
        return false;
    }

    if (!rom_handler_encode_write_payload(config->writeAddress, state, params, message.payload,
                                          payload_size_bytes))
    {
        (void)log_write(LOG_LEVEL_ERROR, "rom save payload encode failed");
        free_payload(ops, message.payload);
        return false;
    }

    message.rxTxBar = false;
    message.address = config->deviceAddress;
    message.items = (uint16_t)payload_size_bytes;

    set_write_disable(ops, false);
    if (!queue_message(config, ops, &message, message.pFailed, config->saveTimeoutTicks))
    {
        (void)log_write(LOG_LEVEL_ERROR, "rom save transfer failed");
        set_write_disable(ops, true);
        free_payload(ops, message.payload);
        return false;
    }

    set_write_disable(ops, true);
    (void)log_write(LOG_LEVEL_INFO, "rom save complete");
    return true;
}