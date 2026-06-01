#ifndef ROM_TASK_SUPPORT_H
#define ROM_TASK_SUPPORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "effects_control_logic.h"
#include "effects_state_manager.h"

typedef struct
{
    void *sourceTask;
    void *i2cQueueHandle;
    void *i2cFailedRomSemaphoreHandle;
    uint16_t deviceAddress;
    uint16_t readAddress;
    uint16_t writeAddress;
    uint32_t bootstrapTimeoutTicks;
    uint32_t saveTimeoutTicks;
} RomTaskSupportConfig;

typedef struct
{
    bool (*queue_message_and_wait)(void *queueHandle, void *message, bool *pFailed,
        void *failedSemaphoreHandle, uint32_t timeoutTicks, void *context);
    uint8_t *(*allocate_payload)(size_t size, void *context);
    void (*free_payload)(uint8_t *payload, void *context);
    void (*set_write_enable)(bool enabled, void *context);
    void *context;
} RomTaskSupportOps;

bool rom_task_bootstrap_effects(const RomTaskSupportConfig *config, const RomTaskSupportOps *ops,
    EffectsState *state, EffectsParams *params);

bool rom_task_save_effects(const RomTaskSupportConfig *config, const RomTaskSupportOps *ops,
    const EffectsState *state, const EffectsParams *params);

#endif