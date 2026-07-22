#include "effects_pipeline.h"

int effects_pipeline_init(EffectsPipeline *pipeline)
{
    if (pipeline == NULL)
    {
        return -1;
    }

    for (int i = 0; i < NUM_EFFECTS; i++)
    {
        effect_instance_init(&pipeline->instances[i], (EffectType)i);
    }

    return 0;
}

int effects_pipeline_sync_params(EffectsPipeline *pipeline, const EffectsParams *params)
{
    if (pipeline == NULL || params == NULL)
    {
        return -1;
    }

    for (int i = 0; i < NUM_EFFECTS; i++)
    {
        if (effect_instance_set_params(&pipeline->instances[i],
                                       effects_params_value_for(params, (EffectType)i)) != 0)
        {
            return -1;
        }
    }

    return 0;
}

int effects_pipeline_process(const EffectsPipeline *pipeline, EffectType effect, const uint16_t *in_buf,
                             uint16_t *out_buf, size_t num_samples)
{
    if (pipeline == NULL || !effect_is_valid(effect))
    {
        return -1;
    }

    return effect_instance_process(&pipeline->instances[effect], in_buf, out_buf, num_samples);
}

int effects_pipeline_get_echo_delay_samples(const EffectsPipeline *pipeline, size_t *delay_samples)
{
    if (pipeline == NULL)
    {
        return -1;
    }

    return effect_instance_get_echo_delay_samples(&pipeline->instances[EFFECT_TYPE_ECHO], delay_samples);
}

int effects_pipeline_attach_echo_state(EffectsPipeline *pipeline, EchoState *state)
{
    if (pipeline == NULL)
    {
        return -1;
    }

    return effect_instance_attach_echo_state(&pipeline->instances[EFFECT_TYPE_ECHO], state);
}

int effects_pipeline_apply_enabled(EffectsPipeline *pipeline, const EffectsState *state)
{
    if (pipeline == NULL || state == NULL)
    {
        return -1;
    }

    for (int i = 0; i < NUM_EFFECTS; i++)
    {
        effect_instance_set_enabled(&pipeline->instances[i],
                                    effects_state_effect_enabled(state, (EffectType)i));
    }
    return 0;
}

int effects_pipeline_reset_state(EffectsPipeline *pipeline, EffectType effect)
{
    if (pipeline == NULL || !effect_is_valid(effect))
    {
        return -1;
    }

    effect_instance_reset_state(&pipeline->instances[effect]);
    return 0;
}
