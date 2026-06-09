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

    if (state->active_effect_selection >= NUM_EFFECTS)
    {
        state->active_effect_selection = 0;
    }
}

bool effects_state_get_active_effect(const EffectsState *state, Effect *active_effect)
{
    if (state == NULL || active_effect == NULL)
    {
        return false;
    }

    if (!effects_state_order_valid(state))
    {
        return false;
    }

    if (state->active_effect_selection >= NUM_EFFECTS)
    {
        return false;
    }

    *active_effect = state->ordered[state->active_effect_selection];
    return effect_is_valid(*active_effect);
}

size_t map_adc_to_param(uint32_t adc_value, size_t min, size_t max, uint32_t adc_max)
{
    if (adc_max == 0)
    {
        return min;
    }

    if (max < min)
    {
        size_t temp = min;
        min = max;
        max = temp;
    }

    if (adc_value > adc_max)
    {
        adc_value = adc_max;
    }

    uint64_t range = (uint64_t)(max - min);
    uint64_t scaled = (range * adc_value + adc_max / 2U) / adc_max;
    return (size_t)(min + (size_t)scaled);
}

void effects_state_apply_switches(EffectsState *state, bool switch_a_enabled, bool switch_b_enabled,
                                  bool switch_c_enabled)
{
    if (state == NULL)
    {
        return;
    }

    state->is_enabled[state->ordered[0]] = switch_a_enabled;
    state->is_enabled[state->ordered[1]] = switch_b_enabled;
    state->is_enabled[state->ordered[2]] = switch_c_enabled;
}

bool effects_params_apply_pot_sample(EffectsParams *params, Effect active_effect, uint8_t pot_index,
                                     uint32_t adc_value, uint32_t adc_max)
{
    if (params == NULL)
    {
        return false;
    }

    switch (active_effect)
    {
    case OVERDRIVE:
        switch (pot_index)
        {
        case 0:
            params->overdrive.gain = map_adc_to_param(adc_value, params->overdrive_min.gain,
                                                      params->overdrive_max.gain, adc_max);
            return true;
        case 1:
            params->overdrive.level = map_adc_to_param(adc_value, params->overdrive_min.level,
                                                       params->overdrive_max.level, adc_max);
            return true;
        case 2:
            params->overdrive.tone = map_adc_to_param(adc_value, params->overdrive_min.tone,
                                                      params->overdrive_max.tone, adc_max);
            return true;
        case 3:
            params->overdrive.mix = map_adc_to_param(adc_value, params->overdrive_min.mix,
                                                     params->overdrive_max.mix, adc_max);
            return true;
        default:
            return false;
        }

    case ECHO:
        switch (pot_index)
        {
        case 0:
            params->echo.pre_delay = map_adc_to_param(adc_value, params->echo_min.pre_delay,
                                                      params->echo_max.pre_delay, adc_max);
            return true;
        case 1:
            params->echo.density = map_adc_to_param(adc_value, params->echo_min.density,
                                                    params->echo_max.density, adc_max);
            return true;
        case 2:
            params->echo.attack =
                map_adc_to_param(adc_value, params->echo_min.attack, params->echo_max.attack, adc_max);
            return true;
        case 3:
            params->echo.decay =
                map_adc_to_param(adc_value, params->echo_min.decay, params->echo_max.decay, adc_max);
            return true;
        default:
            return false;
        }

    case COMPRESSION:
        switch (pot_index)
        {
        case 0:
            params->compression.threshold =
                map_adc_to_param(adc_value, params->compression_min.threshold,
                                 params->compression_max.threshold, adc_max);
            return true;
        case 1:
            params->compression.ratio = map_adc_to_param(adc_value, params->compression_min.ratio,
                                                         params->compression_max.ratio, adc_max);
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