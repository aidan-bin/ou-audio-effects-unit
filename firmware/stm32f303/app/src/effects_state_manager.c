#include "effects_state_manager.h"

#include "effects.h"

bool effect_is_valid(Effect effect)
{
    return effect >= OVERDRIVE && effect <= COMPRESSION;
}

void effects_state_set_default_order(EffectsState *state)
{
    if (state == NULL)
    {
        return;
    }

    state->ordered[0] = OVERDRIVE;
    state->ordered[1] = ECHO;
    state->ordered[2] = COMPRESSION;
}

bool effects_state_order_valid(const EffectsState *state)
{
    if (state == NULL)
    {
        return false;
    }

    bool seen[NUM_EFFECTS] = {false};

    for (int i = 0; i < NUM_EFFECTS; i++)
    {
        if (!effect_is_valid(state->ordered[i]))
        {
            return false;
        }

        if (seen[state->ordered[i]])
        {
            return false;
        }

        seen[state->ordered[i]] = true;
    }

    return true;
}

void effects_state_normalize(EffectsState *state)
{
    if (state == NULL)
    {
        return;
    }

    if (!effects_state_order_valid(state))
    {
        effects_state_set_default_order(state);
    }

    if (state->activeEffectSelection >= NUM_EFFECTS)
    {
        state->activeEffectSelection = 0;
    }
}

bool effects_state_get_active_effect(const EffectsState *state, Effect *activeEffect)
{
    if (state == NULL || activeEffect == NULL)
    {
        return false;
    }

    if (!effects_state_order_valid(state))
    {
        return false;
    }

    if (state->activeEffectSelection >= NUM_EFFECTS)
    {
        return false;
    }

    *activeEffect = state->ordered[state->activeEffectSelection];
    return effect_is_valid(*activeEffect);
}

size_t map_adc_to_param(uint32_t adcValue, size_t min, size_t max, uint32_t adcMax)
{
    if (adcMax == 0)
    {
        return min;
    }

    if (max < min)
    {
        size_t temp = min;
        min = max;
        max = temp;
    }

    if (adcValue > adcMax)
    {
        adcValue = adcMax;
    }

    return (((uint32_t)adcValue << FIXED_POINT_Q) / adcMax * (max - min) + min) >> FIXED_POINT_Q;
}
