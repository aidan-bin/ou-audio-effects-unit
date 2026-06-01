#ifndef SWITCH_TASK_H
#define SWITCH_TASK_H

#include <stdbool.h>
#include <stdint.h>

#include "effects_model.h"

typedef struct
{
    uint32_t pollingFrequencyMs;
} SwitchTaskContext;

typedef struct
{
    bool (*read_switch)(uint8_t index, bool *enabledOut, void *context);
    bool (*read_state)(EffectsState *stateOut, void *context);
    bool (*write_state)(const EffectsState *state, void *context);
    void (*sleep_ms)(uint32_t ms, void *context);
    void *context;
} SwitchTaskOps;

bool switch_task_step(const SwitchTaskContext *taskContext, const SwitchTaskOps *ops);

#endif