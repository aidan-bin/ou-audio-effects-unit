#include "pot_task.h"

bool pot_task_step(const PotTaskContext *task_context, const PotTaskOps *ops)
{
    if (task_context == NULL || ops == NULL || ops->start_adc == NULL ||
        ops->wait_for_sample == NULL || ops->read_active_effect == NULL ||
        ops->apply_pot_sample == NULL || task_context->pot_count == 0)
    {
        return false;
    }

    uint32_t ticks_to_wait = task_context->sampling_frequency_ticks + task_context->timeout_slack_ticks;

    for (uint8_t pot = 0; pot < task_context->pot_count; pot++)
    {
        uint32_t adc_value = 0;
        Effect active_effect;

        if (!ops->start_adc(pot, ops->context))
        {
            continue;
        }

        if (!ops->wait_for_sample(ticks_to_wait, &adc_value, ops->context))
        {
            continue;
        }

        if (!ops->read_active_effect(&active_effect, ops->context))
        {
            continue;
        }

        ops->apply_pot_sample(active_effect, pot, adc_value, task_context->adc_max, ops->context);
    }

    return true;
}
