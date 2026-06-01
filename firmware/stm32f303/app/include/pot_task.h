#ifndef POT_TASK_H
#define POT_TASK_H

#include <stdbool.h>
#include <stdint.h>

#include "effects_control_logic.h"

typedef struct
{
    uint32_t samplingFrequencyTicks;
    uint32_t timeoutSlackTicks;
    uint32_t adcMax;
    uint8_t potCount;
} PotTaskContext;

typedef struct
{
    bool (*start_adc)(uint8_t potIndex, void *context);
    bool (*wait_for_sample)(uint32_t timeoutTicks, uint32_t *valueOut, void *context);
    bool (*read_active_effect)(Effect *activeEffect, void *context);
    void (*apply_pot_sample)(Effect activeEffect, uint8_t potIndex, uint32_t adcValue, uint32_t adcMax,
        void *context);
    void *context;
} PotTaskOps;

bool pot_task_step(const PotTaskContext *taskContext, const PotTaskOps *ops);

#endif