/*
 * effects_pipeline — orchestrates the effect instances based on the effects model.
 */

#ifndef EFFECTS_PIPELINE_H
#define EFFECTS_PIPELINE_H

#include <stddef.h>
#include <stdint.h>

#include "effect_instance.h"
#include "effects_model.h"

typedef struct
{
    EffectInstance instances[NUM_EFFECTS];
} EffectsPipeline;

int effects_pipeline_init(EffectsPipeline *pipeline);
int effects_pipeline_sync_params(EffectsPipeline *pipeline, const EffectsParams *params);
int effects_pipeline_process(const EffectsPipeline *pipeline, EffectType effect, const uint16_t *in_buf,
                             uint16_t *out_buf, size_t num_samples);
int effects_pipeline_get_echo_delay_samples(const EffectsPipeline *pipeline, size_t *delay_samples);
int effects_pipeline_attach_echo_state(EffectsPipeline *pipeline, EchoState *state);
int effects_pipeline_apply_enabled(EffectsPipeline *pipeline, const EffectsState *state);
int effects_pipeline_reset_state(EffectsPipeline *pipeline, EffectType effect);

#endif