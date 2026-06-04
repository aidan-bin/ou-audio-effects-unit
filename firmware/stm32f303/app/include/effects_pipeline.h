#ifndef EFFECTS_PIPELINE_H
#define EFFECTS_PIPELINE_H

#include <stddef.h>
#include <stdint.h>

#include "effect_runtime.h"
#include "effects_model.h"

typedef struct
{
    EffectHandle overdrive;
    EffectHandle echo;
    EffectHandle compression;
} EffectsPipeline;

int effects_pipeline_init(EffectsPipeline *pipeline);
int effects_pipeline_sync_params(EffectsPipeline *pipeline, const EffectsParams *params);
int effects_pipeline_process(const EffectsPipeline *pipeline, Effect effect, const uint16_t *inBuf,
                             uint16_t *outBuf, size_t numSamples);
int effects_pipeline_get_echo_delay_samples(const EffectsPipeline *pipeline, size_t *delaySamples);

#endif