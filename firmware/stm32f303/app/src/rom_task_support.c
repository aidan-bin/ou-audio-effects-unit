#include "rom_task_support.h"

#include "i2c_handler.h"
#include "rom_handler.h"

void rom_task_support_init(RomTaskSupportConfig *config, RomTaskSupportOps *ops,
    void *sourceTask, void *i2cQueueHandle, void *i2cFailedRomSemaphoreHandle,
    uint16_t deviceAddress, uint16_t readAddress, uint16_t writeAddress,
    uint32_t bootstrapTimeoutTicks, uint32_t saveTimeoutTicks,
    bool (*queue_message_and_wait)(void *queueHandle, void *message, bool *pFailed,
        void *failedSemaphoreHandle, uint32_t timeoutTicks, void *context),
    uint8_t *(*allocate_payload)(size_t size, void *context),
    void (*free_payload)(uint8_t *payload, void *context),
    void (*set_write_enable)(bool enabled, void *context),
    void *context)
{
    if (config == NULL || ops == NULL)
    {
        return;
    }

    config->sourceTask = sourceTask;
    config->i2cQueueHandle = i2cQueueHandle;
    config->i2cFailedRomSemaphoreHandle = i2cFailedRomSemaphoreHandle;
    config->deviceAddress = deviceAddress;
    config->readAddress = readAddress;
    config->writeAddress = writeAddress;
    config->bootstrapTimeoutTicks = bootstrapTimeoutTicks;
    config->saveTimeoutTicks = saveTimeoutTicks;

    ops->queue_message_and_wait = queue_message_and_wait;
    ops->allocate_payload = allocate_payload;
    ops->free_payload = free_payload;
    ops->set_write_enable = set_write_enable;
    ops->context = context;
}

static bool queue_message(const RomTaskSupportConfig *config, const RomTaskSupportOps *ops,
    I2CHandlerMessage *message, bool *failed, uint32_t timeoutTicks)
{
    if (config == NULL || ops == NULL || message == NULL || failed == NULL ||
        ops->queue_message_and_wait == NULL)
    {
        return false;
    }

    return ops->queue_message_and_wait(config->i2cQueueHandle, message, failed,
        config->i2cFailedRomSemaphoreHandle, timeoutTicks, ops->context);
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

static void set_write_enable(const RomTaskSupportOps *ops, bool enabled)
{
    if (ops != NULL && ops->set_write_enable != NULL)
    {
        ops->set_write_enable(enabled, ops->context);
    }
}

bool rom_task_bootstrap_effects(const RomTaskSupportConfig *config, const RomTaskSupportOps *ops,
    EffectsState *state, EffectsParams *params)
{
    if (config == NULL || ops == NULL || state == NULL || params == NULL)
    {
        return false;
    }

    const size_t addressPayloadSizeBytes = 2;
    const size_t payloadSizeBytes = rom_handler_read_payload_size();

    bool failed = true;
    I2CHandlerMessage message = {0};
    message.sourceTask = config->sourceTask;
    message.pFailed = &failed;

    message.payload = allocate_payload(ops, addressPayloadSizeBytes);
    if (message.payload == NULL)
    {
        return false;
    }

    if (!rom_handler_encode_address(config->readAddress, message.payload, addressPayloadSizeBytes))
    {
        free_payload(ops, message.payload);
        return false;
    }

    message.rxTxBar = false;
    message.address = config->deviceAddress;
    message.items = (uint16_t)addressPayloadSizeBytes;

    set_write_enable(ops, false);
    if (!queue_message(config, ops, &message, message.pFailed, config->bootstrapTimeoutTicks))
    {
        set_write_enable(ops, true);
        free_payload(ops, message.payload);
        return false;
    }

    set_write_enable(ops, true);

    message.payload = allocate_payload(ops, payloadSizeBytes);
    if (message.payload == NULL)
    {
        return false;
    }

    message.rxTxBar = true;
    message.address = config->deviceAddress | 1U;
    message.items = (uint16_t)payloadSizeBytes;

    if (!queue_message(config, ops, &message, message.pFailed, config->bootstrapTimeoutTicks))
    {
        free_payload(ops, message.payload);
        return false;
    }

    EffectsState loadedState;
    EffectsParams loadedParams;

    if (!rom_handler_decode_read_payload(message.payload, payloadSizeBytes, &loadedState, &loadedParams))
    {
        free_payload(ops, message.payload);
        return false;
    }

    free_payload(ops, message.payload);
    *state = loadedState;
    *params = loadedParams;
    return true;
}

bool rom_task_save_effects(const RomTaskSupportConfig *config, const RomTaskSupportOps *ops,
    const EffectsState *state, const EffectsParams *params)
{
    if (config == NULL || ops == NULL || state == NULL || params == NULL)
    {
        return false;
    }

    const size_t payloadSizeBytes = rom_handler_write_payload_size();

    bool failed = true;
    I2CHandlerMessage message = {0};
    message.sourceTask = config->sourceTask;
    message.pFailed = &failed;

    message.payload = allocate_payload(ops, payloadSizeBytes);
    if (message.payload == NULL)
    {
        return false;
    }

    if (!rom_handler_encode_write_payload(config->writeAddress, state, params, message.payload,
            payloadSizeBytes))
    {
        free_payload(ops, message.payload);
        return false;
    }

    message.rxTxBar = false;
    message.address = config->deviceAddress;
    message.items = (uint16_t)payloadSizeBytes;

    set_write_enable(ops, false);
    if (!queue_message(config, ops, &message, message.pFailed, config->saveTimeoutTicks))
    {
        set_write_enable(ops, true);
        free_payload(ops, message.payload);
        return false;
    }

    set_write_enable(ops, true);
    return true;
}