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

bool effects_state_set_order(EffectsState *state, const Effect *order, size_t count)
{
    if (state == NULL || order == NULL || count != NUM_EFFECTS)
    {
        return false;
    }

    bool seen[NUM_EFFECTS] = {false};

    for (size_t i = 0; i < count; i++)
    {
        if (!effect_is_valid(order[i]) || seen[order[i]])
        {
            return false;
        }

        seen[order[i]] = true;
    }

    for (size_t i = 0; i < count; i++)
    {
        state->ordered[i] = order[i];
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

#define NUM_POTS 4

// Mapping of each effect's pot parameters to the EffectsParams struct offsets.
// assignable == false indicates valid pot that drives nothing.
typedef struct
{
    bool assignable;
    size_t value_offset;
    size_t min_offset;
    size_t max_offset;
} PotMapping;

#define POT_FIELD(group, group_min, group_max, group_type, field)           \
    {                                                                       \
        true, offsetof(EffectsParams, group) + offsetof(group_type, field), \
        offsetof(EffectsParams, group_min) + offsetof(group_type, field),   \
        offsetof(EffectsParams, group_max) + offsetof(group_type, field)}
#define POT_OVERDRIVE(field) POT_FIELD(overdrive, overdrive_min, overdrive_max, OverdriveParam, field)
#define POT_ECHO(field) POT_FIELD(echo, echo_min, echo_max, EchoParam, field)
#define POT_COMPRESSION(field) \
    POT_FIELD(compression, compression_min, compression_max, CompressionParam, field)
#define POT_UNUSED {false, 0, 0, 0}

static const PotMapping pot_map[NUM_EFFECTS][NUM_POTS] = {
    [OVERDRIVE] = {POT_OVERDRIVE(gain), POT_OVERDRIVE(level), POT_OVERDRIVE(tone),
                   POT_OVERDRIVE(mix)},
    [ECHO] = {POT_ECHO(pre_delay), POT_ECHO(density), POT_ECHO(attack), POT_ECHO(decay)},
    [COMPRESSION] = {POT_COMPRESSION(threshold), POT_COMPRESSION(ratio), POT_UNUSED, POT_UNUSED},
};

#undef POT_FIELD
#undef POT_OVERDRIVE
#undef POT_ECHO
#undef POT_COMPRESSION
#undef POT_UNUSED

bool effects_params_apply_pot_sample(EffectsParams *params, Effect active_effect, uint8_t pot_index,
                                     uint32_t adc_value, uint32_t adc_max)
{
    if (params == NULL || !effect_is_valid(active_effect) || pot_index >= NUM_POTS)
    {
        return false;
    }

    const PotMapping *mapping = &pot_map[active_effect][pot_index];
    if (mapping->assignable)
    {
        size_t min = *(size_t *)((char *)params + mapping->min_offset);
        size_t max = *(size_t *)((char *)params + mapping->max_offset);
        *(size_t *)((char *)params + mapping->value_offset) =
            map_adc_to_param(adc_value, min, max, adc_max);
    }

    return true;
}