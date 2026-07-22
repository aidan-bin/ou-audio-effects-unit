#include "effects_model.h"

#include <stddef.h>
#include <string.h>

bool effect_is_valid(EffectType effect)
{
    return (int)effect >= 0 && (int)effect < NUM_EFFECTS;
}

bool effect_id_is_empty(EffectId id)
{
    return id == EFFECT_ID_NONE;
}

static size_t find_effect_slot(const EffectsState *state, EffectType effect)
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

    state->slots[0] = EFFECT_TYPE_OVERDRIVE;
    state->slots[1] = EFFECT_TYPE_ECHO;
    state->slots[2] = EFFECT_TYPE_COMPRESSION;
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

        if (!effect_is_valid((EffectType)slot) || seen[slot])
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

        if (!effect_is_valid((EffectType)slot) || seen[slot])
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

bool effects_state_assign_slot(EffectsState *state, size_t pos, EffectType effect)
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

bool effects_state_get_active_effect(const EffectsState *state, EffectType *active_effect)
{
    if (state == NULL || active_effect == NULL || state->active_slot >= NUM_SLOTS)
    {
        return false;
    }

    uint8_t slot = state->slots[state->active_slot];
    if (effect_id_is_empty(slot) || !effect_is_valid((EffectType)slot))
    {
        return false;
    }

    *active_effect = (EffectType)slot;
    return true;
}

bool effects_state_effect_enabled(const EffectsState *state, EffectType effect)
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

static const EffectParamMeta param_meta[] = {
    {EFFECT_TYPE_OVERDRIVE, "overdrive.gain", offsetof(EffectsParams, overdrive.gain), 0, MAX_OVERDRIVE_GAIN, 0},
    {EFFECT_TYPE_OVERDRIVE, "overdrive.level", offsetof(EffectsParams, overdrive.level), 0, MAX_OVERDRIVE_LEVEL, 1},
    {EFFECT_TYPE_OVERDRIVE, "overdrive.tone", offsetof(EffectsParams, overdrive.tone), MIN_OVERDRIVE_TONE, MAX_OVERDRIVE_TONE, 2},
    {EFFECT_TYPE_OVERDRIVE, "overdrive.mix", offsetof(EffectsParams, overdrive.mix), 0, MAX_OVERDRIVE_MIX, 3},
    {EFFECT_TYPE_ECHO, "echo.delay_samples", offsetof(EffectsParams, echo.delay_samples), 0, MAX_ECHO_DELAY_SAMPLES, -1},
    {EFFECT_TYPE_ECHO, "echo.pre_delay", offsetof(EffectsParams, echo.pre_delay), MIN_ECHO_PRE_DELAY, MAX_ECHO_PRE_DELAY, 0},
    {EFFECT_TYPE_ECHO, "echo.density", offsetof(EffectsParams, echo.density), 0, MAX_ECHO_DENSITY, 1},
    {EFFECT_TYPE_ECHO, "echo.attack", offsetof(EffectsParams, echo.attack), 0, MAX_ECHO_ATTACK, 2},
    {EFFECT_TYPE_ECHO, "echo.decay", offsetof(EffectsParams, echo.decay), 0, MAX_ECHO_DECAY, 3},
    {EFFECT_TYPE_ECHO, "echo.feedback", offsetof(EffectsParams, echo.feedback), 0, MAX_ECHO_FEEDBACK, -1},
    {EFFECT_TYPE_ECHO, "echo.feedback_delay", offsetof(EffectsParams, echo.feedback_delay), 0, MAX_ECHO_FEEDBACK_DELAY, -1},
    {EFFECT_TYPE_ECHO, "echo.damping", offsetof(EffectsParams, echo.damping), 0, MAX_ECHO_DAMPING, -1},
    {EFFECT_TYPE_COMPRESSION, "compression.threshold", offsetof(EffectsParams, compression.threshold), 0, MAX_COMPRESSION_THRESHOLD, 0},
    {EFFECT_TYPE_COMPRESSION, "compression.ratio", offsetof(EffectsParams, compression.ratio), 0, MAX_COMPRESSION_RATIO, 1},
};

#define PARAM_META_COUNT (sizeof(param_meta) / sizeof(param_meta[0]))

const EffectParamMeta *effect_params_meta_find(const char *key)
{
    if (key == NULL)
    {
        return NULL;
    }

    for (size_t i = 0; i < PARAM_META_COUNT; i++)
    {
        if (strcmp(param_meta[i].key, key) == 0)
        {
            return &param_meta[i];
        }
    }
    return NULL;
}

bool effects_params_apply_pot_sample(EffectsParams *params, EffectType active_effect, uint8_t pot_index,
                                     uint32_t adc_value, uint32_t adc_max)
{
    if (params == NULL || !effect_is_valid(active_effect) || pot_index >= NUM_POTS)
    {
        return false;
    }

    for (size_t i = 0; i < PARAM_META_COUNT; i++)
    {
        const EffectParamMeta *meta = &param_meta[i];
        if (meta->effect == active_effect && meta->pot == (int)pot_index)
        {
            *(size_t *)((char *)params + meta->offset) =
                map_adc_to_param(adc_value, meta->min, meta->max, adc_max);
            break;
        }
    }

    return true;
}

static const size_t param_member_offset[NUM_EFFECTS] = {
    [EFFECT_TYPE_OVERDRIVE] = offsetof(EffectsParams, overdrive),
    [EFFECT_TYPE_ECHO] = offsetof(EffectsParams, echo),
    [EFFECT_TYPE_COMPRESSION] = offsetof(EffectsParams, compression),
};

const void *effects_params_value_for(const EffectsParams *params, EffectType effect)
{
    if (params == NULL || !effect_is_valid(effect))
    {
        return NULL;
    }

    return (const char *)params + param_member_offset[effect];
}