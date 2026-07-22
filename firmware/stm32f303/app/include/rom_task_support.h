/*
 * rom_task_support — dependency-injection layer for ROM operations.
 */

#ifndef ROM_TASK_SUPPORT_H
#define ROM_TASK_SUPPORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "effects_model.h"

typedef struct
{
    void *source_task;
    void *i2c_queue_handle;
    void *i2c_failed_rom_semaphore_handle;
    uint16_t device_address;
    uint16_t read_address;
    uint16_t write_address;
    uint32_t bootstrap_timeout_ticks;
    uint32_t save_timeout_ticks;
} RomTaskSupportConfig;

typedef struct
{
    bool (*queue_message_and_wait)(void *queue_handle, void *message, bool *p_failed,
                                   void *failed_semaphore_handle, uint32_t timeout_ticks,
                                   void *context);
    uint8_t *(*allocate_payload)(size_t size, void *context);
    void (*free_payload)(uint8_t *payload, void *context);
    void (*set_write_disable)(bool disable_writes, void *context);
    void *context;
} RomTaskSupportOps;

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
                           void *context);

bool rom_task_bootstrap_effects(const RomTaskSupportConfig *config, const RomTaskSupportOps *ops,
                                EffectsState *state, EffectsParams *params);

bool rom_task_save_effects(const RomTaskSupportConfig *config, const RomTaskSupportOps *ops,
                           const EffectsState *state, const EffectsParams *params);

#endif