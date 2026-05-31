#ifndef EFFECTS_CONTROL_LOGIC_H
#define EFFECTS_CONTROL_LOGIC_H

#include <stdbool.h>
#include <stdint.h>

#include "effects.h"
#include "effects_state_manager.h"

typedef struct
{
    /* Effects configurations */
    OverdriveParam overdrive;
    EchoParam echo;
    CompressionParam compression;

    /* Effects limits (min and max values for each effect, corresponding to each end of pot) */
    OverdriveParam overdriveMin, overdriveMax;
    EchoParam echoMin, echoMax;
    CompressionParam compressionMin, compressionMax;
} EffectsParams;

void effects_state_apply_switches(EffectsState *state, bool switchAEnabled, bool switchBEnabled,
    bool switchCEnabled);

bool effects_params_apply_pot_sample(EffectsParams *params, Effect activeEffect, uint8_t potIndex,
    uint32_t adcValue, uint32_t adcMax);

#endif
