#include "switch_task.h"

bool switch_task_step(const SwitchTaskContext *task_context, const SwitchTaskOps *ops)
{
    if (task_context == NULL || ops == NULL || ops->read_switch == NULL || ops->read_state == NULL ||
        ops->write_state == NULL || ops->sleep_ms == NULL)
    {
        return false;
    }

    bool switch_a_enabled = false;
    bool switch_b_enabled = false;
    bool switch_c_enabled = false;

    if (!ops->read_switch(0, &switch_a_enabled, ops->context) ||
        !ops->read_switch(1, &switch_b_enabled, ops->context) ||
        !ops->read_switch(2, &switch_c_enabled, ops->context))
    {
        return false;
    }

    EffectsState latched_effects_state;
    if (!ops->read_state(&latched_effects_state, ops->context))
    {
        return false;
    }

    effects_state_normalize(&latched_effects_state);
    effects_state_apply_switches(&latched_effects_state, switch_a_enabled, switch_b_enabled,
                                 switch_c_enabled);

    if (!ops->write_state(&latched_effects_state, ops->context))
    {
        return false;
    }

    ops->sleep_ms(task_context->pollingFrequencyMs, ops->context);
    return true;
}
