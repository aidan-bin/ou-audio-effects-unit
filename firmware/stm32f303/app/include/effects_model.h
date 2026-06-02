#ifndef EFFECTS_MODEL_H
#define EFFECTS_MODEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "effects.h"

#define NUM_EFFECTS 3

typedef enum {
    OVERDRIVE,
    ECHO,
    COMPRESSION,
} Effect;

typedef struct {
    Effect ordered[NUM_EFFECTS];
    bool isEnabled[NUM_EFFECTS];
    uint8_t activeEffectSelection;
} EffectsState;

typedef struct {
    OverdriveParam overdrive;
    EchoParam echo;
    CompressionParam compression;

    OverdriveParam overdriveMin, overdriveMax;
    EchoParam echoMin, echoMax;
    CompressionParam compressionMin, compressionMax;
} EffectsParams;

bool effect_is_valid(Effect effect);
void effects_state_set_default_order(EffectsState *state);
bool effects_state_order_valid(const EffectsState *state);
void effects_state_normalize(EffectsState *state);
bool effects_state_get_active_effect(const EffectsState *state, Effect *activeEffect);
size_t map_adc_to_param(uint32_t adcValue, size_t min, size_t max, uint32_t adcMax);

void effects_state_apply_switches(EffectsState *state, bool switchAEnabled, bool switchBEnabled,
                                  bool switchCEnabled);

bool effects_params_apply_pot_sample(EffectsParams *params, Effect activeEffect, uint8_t potIndex,
                                     uint32_t adcValue, uint32_t adcMax);

#endif