/*
 * switch_task — reads the toggle switches and drives the enable mask.
 */

#ifndef SWITCH_TASK_H
#define SWITCH_TASK_H

#include <stdbool.h>
#include <stdint.h>

#include "effects_model.h"

typedef struct
{
    uint32_t polling_frequency_ms;
} SwitchTaskContext;

typedef struct
{
    bool (*read_switch)(uint8_t index, bool *enabled_out, void *context);
    bool (*read_state)(EffectsState *state_out, void *context);
    bool (*write_state)(const EffectsState *state, void *context);
    void (*sleep_ms)(uint32_t ms, void *context);
    void *context;
} SwitchTaskOps;

bool switch_task_step(const SwitchTaskContext *task_context, const SwitchTaskOps *ops);

#endif