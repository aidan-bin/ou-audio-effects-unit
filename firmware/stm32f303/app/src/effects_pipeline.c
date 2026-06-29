#include "effects_pipeline.h"

int effects_pipeline_init(EffectsPipeline *pipeline)
{
    if (pipeline == NULL)
    {
        return -1;
    }

    for (int i = 0; i < NUM_EFFECTS; i++)
    {
        if (effect_handle_init(&pipeline->handles[i], (EffectType)i) != 0)
        {
            return -1;
        }
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
        if (effect_handle_set_params(&pipeline->handles[i],
                                     effects_params_value_for(params, (Effect)i)) != 0)
        {
            return -1;
        }
    }

    return 0;
}

int effects_pipeline_process(const EffectsPipeline *pipeline, Effect effect, const uint16_t *in_buf,
                             uint16_t *out_buf, size_t num_samples)
{
    if (pipeline == NULL || !effect_is_valid(effect))
    {
        return -1;
    }

    return effect_handle_process(&pipeline->handles[effect], in_buf, out_buf, num_samples);
}

int effects_pipeline_get_echo_delay_samples(const EffectsPipeline *pipeline, size_t *delay_samples)
{
    if (pipeline == NULL)
    {
        return -1;
    }

    return effect_handle_get_echo_delay_samples(&pipeline->handles[ECHO], delay_samples);
}

int effects_pipeline_attach_echo_state(EffectsPipeline *pipeline, EchoState *state)
{
    if (pipeline == NULL)
    {
        return -1;
    }

    return effect_handle_attach_echo_state(&pipeline->handles[ECHO], state);
}

int effects_pipeline_apply_enabled(EffectsPipeline *pipeline, const EffectsState *state)
{
    if (pipeline == NULL || state == NULL)
    {
        return -1;
    }

    for (int i = 0; i < NUM_EFFECTS; i++)
    {
        effect_handle_set_enabled(&pipeline->handles[i], state->is_enabled[i]);
    }
    return 0;
}

int effects_pipeline_reset_state(EffectsPipeline *pipeline, Effect effect)
{
    if (pipeline == NULL || !effect_is_valid(effect))
    {
        return -1;
    }

    return effect_handle_reset_state(&pipeline->handles[effect]);
}
