#ifndef EFFECTS_STATE_MANAGER_H
#define EFFECTS_STATE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define NUM_EFFECTS 3

typedef enum
{
    OVERDRIVE,
    ECHO,
    COMPRESSION,
} Effect;

typedef struct
{
    Effect ordered[NUM_EFFECTS]; // Active effects in order
    bool isEnabled[NUM_EFFECTS]; // Whether each active effect is enabled (indexable by ordered)
    uint8_t activeEffectSelection; // Which effect is currently selected (i.e., an index of ordered)
} EffectsState;

bool effect_is_valid(Effect effect);
void effects_state_set_default_order(EffectsState *state);
bool effects_state_order_valid(const EffectsState *state);
void effects_state_normalize(EffectsState *state);
bool effects_state_get_active_effect(const EffectsState *state, Effect *activeEffect);
size_t map_adc_to_param(uint32_t adcValue, size_t min, size_t max, uint32_t adcMax);

#endif
