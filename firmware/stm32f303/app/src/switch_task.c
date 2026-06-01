#include "switch_task.h"

bool switch_task_step(const SwitchTaskContext *taskContext, const SwitchTaskOps *ops)
{
    if (taskContext == NULL || ops == NULL || ops->read_switch == NULL || ops->read_state == NULL ||
        ops->write_state == NULL || ops->sleep_ms == NULL)
    {
        return false;
    }

    bool switchAEnabled = false;
    bool switchBEnabled = false;
    bool switchCEnabled = false;

    if (!ops->read_switch(0, &switchAEnabled, ops->context) ||
        !ops->read_switch(1, &switchBEnabled, ops->context) ||
        !ops->read_switch(2, &switchCEnabled, ops->context))
    {
        return false;
    }

    EffectsState latchedEffectsState;
    if (!ops->read_state(&latchedEffectsState, ops->context))
    {
        return false;
    }

    effects_state_normalize(&latchedEffectsState);
    effects_state_apply_switches(&latchedEffectsState, switchAEnabled, switchBEnabled, switchCEnabled);

    if (!ops->write_state(&latchedEffectsState, ops->context))
    {
        return false;
    }

    ops->sleep_ms(taskContext->pollingFrequencyMs, ops->context);
    return true;
}
