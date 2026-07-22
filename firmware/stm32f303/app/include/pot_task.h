/*
 * pot_task — reads pots and applies to the selected effect's parameters.
 */

#ifndef POT_TASK_H
#define POT_TASK_H

#include <stdbool.h>
#include <stdint.h>

#include "effects_model.h"

typedef struct
{
    uint32_t sampling_frequency_ticks;
    uint32_t timeout_slack_ticks;
    uint32_t adc_max;
    uint8_t pot_count;
} PotTaskContext;

typedef struct
{
    bool (*start_adc)(uint8_t pot_index, void *context);
    bool (*wait_for_sample)(uint32_t timeout_ticks, uint32_t *value_out, void *context);
    bool (*read_active_effect)(EffectType *active_effect, void *context);
    void (*apply_pot_sample)(EffectType active_effect, uint8_t pot_index, uint32_t adc_value,
                             uint32_t adc_max, void *context);
    void *context;
} PotTaskOps;

bool pot_task_step(const PotTaskContext *task_context, const PotTaskOps *ops);

#endif