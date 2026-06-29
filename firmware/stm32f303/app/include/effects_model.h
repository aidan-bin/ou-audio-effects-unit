#ifndef EFFECTS_MODEL_H
#define EFFECTS_MODEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "effects.h"

#define NUM_EFFECTS 3
#define NUM_SWITCHES 3
#define NUM_SLOTS 4

typedef uint8_t EffectId;
#define EFFECT_ID_NONE 0xFFu

typedef enum
{
    OVERDRIVE,
    ECHO,
    COMPRESSION,
} Effect;

typedef struct
{
    EffectId slots[NUM_SLOTS];
    bool slot_enabled[NUM_SLOTS];
    uint8_t active_slot; // which slot the pots are currently controlling (0..NUM_SLOTS-1)
} EffectsState;

typedef struct
{
    OverdriveParam overdrive;
    EchoParam echo;
    CompressionParam compression;

    OverdriveParam overdrive_min, overdrive_max;
    EchoParam echo_min, echo_max;
    CompressionParam compression_min, compression_max;
} EffectsParams;

bool effect_is_valid(Effect effect);
void effects_state_set_default_slots(EffectsState *state);
bool effects_state_slots_valid(const EffectsState *state);
void effects_state_normalize(EffectsState *state);
bool effects_state_assign_slot(EffectsState *state, size_t pos, Effect effect);
bool effects_state_clear_slot(EffectsState *state, size_t pos);
bool effects_state_swap_slots(EffectsState *state, size_t a, size_t b);
bool effects_state_set_slot_enabled(EffectsState *state, size_t pos, bool enabled);
bool effects_state_get_active_effect(const EffectsState *state, Effect *active_effect);
bool effects_state_effect_enabled(const EffectsState *state, Effect effect);
bool effects_state_set_active_slot(EffectsState *state, size_t pos);
bool effect_id_is_empty(EffectId id);

size_t map_adc_to_param(uint32_t adc_value, size_t min, size_t max, uint32_t adc_max);

void effects_state_apply_switches(EffectsState *state, bool switch_a_enabled, bool switch_b_enabled,
                                  bool switch_c_enabled);

bool effects_params_apply_pot_sample(EffectsParams *params, Effect active_effect, uint8_t pot_index,
                                     uint32_t adc_value, uint32_t adc_max);


const void *effects_params_value_for(const EffectsParams *params, Effect effect);

#endif