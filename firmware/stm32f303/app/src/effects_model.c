#include "effects_model.h"

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

    uint64_t range = (uint64_t)(max - min);
    uint64_t scaled = (range * adcValue + adcMax / 2U) / adcMax;
    return (size_t)(min + (size_t)scaled);
}

void effects_state_apply_switches(EffectsState *state, bool switchAEnabled, bool switchBEnabled,
    bool switchCEnabled)
{
    if (state == NULL)
    {
        return;
    }

    state->isEnabled[state->ordered[0]] = switchAEnabled;
    state->isEnabled[state->ordered[1]] = switchBEnabled;
    state->isEnabled[state->ordered[2]] = switchCEnabled;
}

bool effects_params_apply_pot_sample(EffectsParams *params, Effect activeEffect, uint8_t potIndex,
    uint32_t adcValue, uint32_t adcMax)
{
    if (params == NULL)
    {
        return false;
    }

    switch (activeEffect)
    {
        case OVERDRIVE:
            switch (potIndex)
            {
                case 0:
                    params->overdrive.gain =
                        map_adc_to_param(adcValue, params->overdriveMin.gain, params->overdriveMax.gain, adcMax);
                    return true;
                case 1:
                    params->overdrive.level = map_adc_to_param(
                        adcValue, params->overdriveMin.level, params->overdriveMax.level, adcMax);
                    return true;
                case 2:
                    params->overdrive.tone =
                        map_adc_to_param(adcValue, params->overdriveMin.tone, params->overdriveMax.tone, adcMax);
                    return true;
                case 3:
                    params->overdrive.mix =
                        map_adc_to_param(adcValue, params->overdriveMin.mix, params->overdriveMax.mix, adcMax);
                    return true;
                default:
                    return false;
            }

        case ECHO:
            switch (potIndex)
            {
                case 0:
                    params->echo.pre_delay = map_adc_to_param(
                        adcValue, params->echoMin.pre_delay, params->echoMax.pre_delay, adcMax);
                    return true;
                case 1:
                    params->echo.density =
                        map_adc_to_param(adcValue, params->echoMin.density, params->echoMax.density, adcMax);
                    return true;
                case 2:
                    params->echo.attack =
                        map_adc_to_param(adcValue, params->echoMin.attack, params->echoMax.attack, adcMax);
                    return true;
                case 3:
                    params->echo.decay =
                        map_adc_to_param(adcValue, params->echoMin.decay, params->echoMax.decay, adcMax);
                    return true;
                default:
                    return false;
            }

        case COMPRESSION:
            switch (potIndex)
            {
                case 0:
                    params->compression.threshold = map_adc_to_param(
                        adcValue, params->compressionMin.threshold, params->compressionMax.threshold, adcMax);
                    return true;
                case 1:
                    params->compression.ratio = map_adc_to_param(
                        adcValue, params->compressionMin.ratio, params->compressionMax.ratio, adcMax);
                    return true;
                case 2:
                case 3:
                    return true;
                default:
                    return false;
            }

        default:
            return false;
    }
}