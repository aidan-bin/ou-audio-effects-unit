#ifndef EFFECTS_MODEL_H
#define EFFECTS_MODEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "effects.h"

#define NUM_EFFECTS 3

typedef enum
{
    OVERDRIVE,
    ECHO,
    COMPRESSION,
} Effect;

typedef struct
{
    Effect ordered[NUM_EFFECTS];
    bool is_enabled[NUM_EFFECTS];
    uint8_t active_effect_selection;
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
void effects_state_set_default_order(EffectsState *state);
bool effects_state_order_valid(const EffectsState *state);
bool effects_state_set_order(EffectsState *state, const Effect *order, size_t count);
void effects_state_normalize(EffectsState *state);
bool effects_state_get_active_effect(const EffectsState *state, Effect *active_effect);
size_t map_adc_to_param(uint32_t adc_value, size_t min, size_t max, uint32_t adc_max);

void effects_state_apply_switches(EffectsState *state, bool switch_a_enabled, bool switch_b_enabled,
                                  bool switch_c_enabled);

bool effects_params_apply_pot_sample(EffectsParams *params, Effect active_effect, uint8_t pot_index,
                                     uint32_t adc_value, uint32_t adc_max);

#endif