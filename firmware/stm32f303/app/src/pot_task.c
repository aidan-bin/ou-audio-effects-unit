#include "pot_task.h"

bool pot_task_step(const PotTaskContext *taskContext, const PotTaskOps *ops)
{
    if (taskContext == NULL || ops == NULL || ops->start_adc == NULL ||
        ops->wait_for_sample == NULL || ops->read_active_effect == NULL ||
        ops->apply_pot_sample == NULL || taskContext->potCount == 0)
    {
        return false;
    }

    uint32_t ticksToWait = taskContext->samplingFrequencyTicks + taskContext->timeoutSlackTicks;

    for (uint8_t pot = 0; pot < taskContext->potCount; pot++)
    {
        uint32_t adcValue = 0;
        Effect activeEffect;

        if (!ops->start_adc(pot, ops->context))
        {
            continue;
        }

        if (!ops->wait_for_sample(ticksToWait, &adcValue, ops->context))
        {
            continue;
        }

        if (!ops->read_active_effect(&activeEffect, ops->context))
        {
            continue;
        }

        ops->apply_pot_sample(activeEffect, pot, adcValue, taskContext->adcMax, ops->context);
    }

    return true;
}
