#include "effects_model.h"

#include <stddef.h>

bool effect_is_valid(Effect effect)
{
    return (int)effect >= 0 && (int)effect < NUM_EFFECTS;
}

bool effect_id_is_empty(EffectId id)
{
    return id == EFFECT_ID_NONE;
}

static size_t find_effect_slot(const EffectsState *state, Effect effect)
{
    for (size_t i = 0; i < NUM_SLOTS; i++)
    {
        if (state->slots[i] == (uint8_t)effect)
        {
            return i;
        }
    }
    return NUM_SLOTS;
}

void effects_state_set_default_slots(EffectsState *state)
{
    if (state == NULL)
    {
        return;
    }

    for (size_t i = 0; i < NUM_SLOTS; i++)
    {
        state->slots[i] = EFFECT_ID_NONE;
        state->slot_enabled[i] = false;
    }

    state->slots[0] = OVERDRIVE;
    state->slots[1] = ECHO;
    state->slots[2] = COMPRESSION;
    state->slot_enabled[0] = true;
    state->active_slot = 0;
}

bool effects_state_slots_valid(const EffectsState *state)
{
    if (state == NULL)
    {
        return false;
    }

    bool seen[NUM_EFFECTS] = {false};

    for (size_t i = 0; i < NUM_SLOTS; i++)
    {
        uint8_t slot = state->slots[i];
        if (effect_id_is_empty(slot))
        {
            continue;
        }

        if (!effect_is_valid((Effect)slot) || seen[slot])
        {
            return false;
        }

        seen[slot] = true;
    }

    return true;
}

void effects_state_normalize(EffectsState *state)
{
    if (state == NULL)
    {
        return;
    }

    // Clear any invalid id or duplicate, keeping the first occurrence.
    bool seen[NUM_EFFECTS] = {false};
    for (size_t i = 0; i < NUM_SLOTS; i++)
    {
        uint8_t slot = state->slots[i];
        if (effect_id_is_empty(slot))
        {
            continue;
        }

        if (!effect_is_valid((Effect)slot) || seen[slot])
        {
            state->slots[i] = EFFECT_ID_NONE;
            state->slot_enabled[i] = false;
            continue;
        }

        seen[slot] = true;
    }

    if (state->active_slot >= NUM_SLOTS)
    {
        state->active_slot = 0;
    }
}

bool effects_state_assign_slot(EffectsState *state, size_t pos, Effect effect)
{
    if (state == NULL || pos >= NUM_SLOTS || !effect_is_valid(effect))
    {
        return false;
    }

    // One instance per effect: vacate any other slot already holding it.
    // Vacated slot becomes empty + disabled.
    size_t existing = find_effect_slot(state, effect);
    if (existing != NUM_SLOTS && existing != pos)
    {
        state->slots[existing] = EFFECT_ID_NONE;
        state->slot_enabled[existing] = false;
    }

    state->slots[pos] = (uint8_t)effect;
    return true;
}

bool effects_state_clear_slot(EffectsState *state, size_t pos)
{
    if (state == NULL || pos >= NUM_SLOTS)
    {
        return false;
    }

    state->slots[pos] = EFFECT_ID_NONE;
    state->slot_enabled[pos] = false;
    return true;
}

bool effects_state_swap_slots(EffectsState *state, size_t a, size_t b)
{
    if (state == NULL || a >= NUM_SLOTS || b >= NUM_SLOTS)
    {
        return false;
    }

    uint8_t tmp_slot = state->slots[a];
    state->slots[a] = state->slots[b];
    state->slots[b] = tmp_slot;

    bool tmp_en = state->slot_enabled[a];
    state->slot_enabled[a] = state->slot_enabled[b];
    state->slot_enabled[b] = tmp_en;
    return true;
}

bool effects_state_set_slot_enabled(EffectsState *state, size_t pos, bool enabled)
{
    if (state == NULL || pos >= NUM_SLOTS)
    {
        return false;
    }

    state->slot_enabled[pos] = enabled;
    return true;
}

bool effects_state_get_active_effect(const EffectsState *state, Effect *active_effect)
{
    if (state == NULL || active_effect == NULL || state->active_slot >= NUM_SLOTS)
    {
        return false;
    }

    uint8_t slot = state->slots[state->active_slot];
    if (effect_id_is_empty(slot) || !effect_is_valid((Effect)slot))
    {
        return false;
    }

    *active_effect = (Effect)slot;
    return true;
}

bool effects_state_effect_enabled(const EffectsState *state, Effect effect)
{
    if (state == NULL || !effect_is_valid(effect))
    {
        return false;
    }

    size_t pos = find_effect_slot(state, effect);
    return pos != NUM_SLOTS && state->slot_enabled[pos];
}

bool effects_state_set_active_slot(EffectsState *state, size_t pos)
{
    if (state == NULL || pos >= NUM_SLOTS)
    {
        return false;
    }

    state->active_slot = (uint8_t)pos;
    return true;
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

    // TODO: Only three switches, so any effect above slot 2 needs to be enabled via CLI.
    state->slot_enabled[0] = switch_a_enabled;
    state->slot_enabled[1] = switch_b_enabled;
    state->slot_enabled[2] = switch_c_enabled;
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

#define POT_FIELD(group, group_min, group_max, group_type, field)             \
    {                                                                         \
        true, offsetof(EffectsParams, group) + offsetof(group_type, field),   \
            offsetof(EffectsParams, group_min) + offsetof(group_type, field), \
            offsetof(EffectsParams, group_max) + offsetof(group_type, field)  \
    }
#define POT_OVERDRIVE(field) POT_FIELD(overdrive, overdrive_min, overdrive_max, OverdriveParam, field)
#define POT_ECHO(field) POT_FIELD(echo, echo_min, echo_max, EchoParam, field)
#define POT_COMPRESSION(field) \
    POT_FIELD(compression, compression_min, compression_max, CompressionParam, field)
#define POT_UNUSED     \
    {                  \
        false, 0, 0, 0 \
    }

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

static const size_t param_member_offset[NUM_EFFECTS] = {
    [OVERDRIVE] = offsetof(EffectsParams, overdrive),
    [ECHO] = offsetof(EffectsParams, echo),
    [COMPRESSION] = offsetof(EffectsParams, compression),
};

const void *effects_params_value_for(const EffectsParams *params, Effect effect)
{
    if (params == NULL || !effect_is_valid(effect))
    {
        return NULL;
    }

    return (const char *)params + param_member_offset[effect];
}