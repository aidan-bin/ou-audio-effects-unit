#include "rom_task_support.h"

#include "log.h"
#include "i2c_handler.h"
#include "rom_handler.h"

#define I2C_READ_FLAG 1U

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

    config->source_task = source_task;
    config->i2c_queue_handle = i2c_queue_handle;
    config->i2c_failed_rom_semaphore_handle = i2c_failed_rom_semaphore_handle;
    config->device_address = device_address;
    config->read_address = read_address;
    config->write_address = write_address;
    config->bootstrap_timeout_ticks = bootstrap_timeout_ticks;
    config->save_timeout_ticks = save_timeout_ticks;

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

    return ops->queue_message_and_wait(config->i2c_queue_handle, message, failed,
                                       config->i2c_failed_rom_semaphore_handle, timeout_ticks,
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

// Queues a write transfer bracketed by enabling then re-disabling EEPROM writes.
// Writes are always re-disabled (success or failure); the caller frees the payload.
static bool queue_write_message(const RomTaskSupportConfig *config, const RomTaskSupportOps *ops,
                                I2CHandlerMessage *message, uint32_t timeout_ticks,
                                const char *fail_msg)
{
    set_write_disable(ops, false);
    bool ok = queue_message(config, ops, message, message->p_failed, timeout_ticks);
    set_write_disable(ops, true);
    if (!ok)
    {
        (void)log_write(LOG_LEVEL_ERROR, fail_msg);
    }
    return ok;
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

    const size_t address_payload_size_bytes = ROM_HANDLER_ADDRESS_BYTES;
    const size_t payload_size_bytes = rom_handler_read_payload_size();

    bool failed = true;
    I2CHandlerMessage message = {0};
    message.source_task = config->source_task;
    message.p_failed = &failed;

    message.payload = allocate_payload(ops, address_payload_size_bytes);
    if (message.payload == NULL)
    {
        (void)log_write(LOG_LEVEL_ERROR, "rom bootstrap address payload alloc failed");
        return false;
    }

    if (!rom_handler_encode_address(config->read_address, message.payload,
                                    address_payload_size_bytes))
    {
        (void)log_write(LOG_LEVEL_ERROR, "rom bootstrap address encode failed");
        free_payload(ops, message.payload);
        return false;
    }

    message.rx_tx_bar = false;
    message.address = config->device_address;
    message.items = (uint16_t)address_payload_size_bytes;

    if (!queue_write_message(config, ops, &message, config->bootstrap_timeout_ticks,
                             "rom bootstrap read-address transfer failed"))
    {
        free_payload(ops, message.payload);
        return false;
    }

    message.payload = allocate_payload(ops, payload_size_bytes);
    if (message.payload == NULL)
    {
        (void)log_write(LOG_LEVEL_ERROR, "rom bootstrap read payload alloc failed");
        return false;
    }

    message.rx_tx_bar = true;
    message.address = config->device_address | I2C_READ_FLAG;
    message.items = (uint16_t)payload_size_bytes;

    if (!queue_message(config, ops, &message, message.p_failed, config->bootstrap_timeout_ticks))
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
    message.source_task = config->source_task;
    message.p_failed = &failed;

    message.payload = allocate_payload(ops, payload_size_bytes);
    if (message.payload == NULL)
    {
        (void)log_write(LOG_LEVEL_ERROR, "rom save payload alloc failed");
        return false;
    }

    if (!rom_handler_encode_write_payload(config->write_address, state, params, message.payload,
                                          payload_size_bytes))
    {
        (void)log_write(LOG_LEVEL_ERROR, "rom save payload encode failed");
        free_payload(ops, message.payload);
        return false;
    }

    message.rx_tx_bar = false;
    message.address = config->device_address;
    message.items = (uint16_t)payload_size_bytes;

    if (!queue_write_message(config, ops, &message, config->save_timeout_ticks,
                             "rom save transfer failed"))
    {
        free_payload(ops, message.payload);
        return false;
    }

    (void)log_write(LOG_LEVEL_INFO, "rom save complete");
    return true;
}