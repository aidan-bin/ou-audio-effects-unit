#include "effects_pipeline.h"

int effects_pipeline_init(EffectsPipeline *pipeline)
{
    if (pipeline == NULL)
    {
        return -1;
    }

    if (effect_handle_init(&pipeline->overdrive, EFFECT_TYPE_OVERDRIVE) != 0)
    {
        return -1;
    }

    if (effect_handle_init(&pipeline->echo, EFFECT_TYPE_ECHO) != 0)
    {
        return -1;
    }

    if (effect_handle_init(&pipeline->compression, EFFECT_TYPE_COMPRESSION) != 0)
    {
        return -1;
    }

    return 0;
}

int effects_pipeline_sync_params(EffectsPipeline *pipeline, const EffectsParams *params)
{
    if (pipeline == NULL || params == NULL)
    {
        return -1;
    }

    if (effect_handle_set_overdrive_params(&pipeline->overdrive, &params->overdrive) != 0)
    {
        return -1;
    }

    if (effect_handle_set_echo_params(&pipeline->echo, &params->echo) != 0)
    {
        return -1;
    }

    if (effect_handle_set_compression_params(&pipeline->compression, &params->compression) != 0)
    {
        return -1;
    }

    return 0;
}

int effects_pipeline_process(const EffectsPipeline *pipeline, Effect effect, const uint16_t *in_buf,
                             uint16_t *out_buf, size_t num_samples)
{
    if (pipeline == NULL)
    {
        return -1;
    }

    switch (effect)
    {
    case OVERDRIVE:
        return effect_handle_process(&pipeline->overdrive, in_buf, out_buf, num_samples);
    case ECHO:
        return effect_handle_process(&pipeline->echo, in_buf, out_buf, num_samples);
    case COMPRESSION:
        return effect_handle_process(&pipeline->compression, in_buf, out_buf, num_samples);
    default:
        return -1;
    }
}

int effects_pipeline_get_echo_delay_samples(const EffectsPipeline *pipeline, size_t *delay_samples)
{
    if (pipeline == NULL)
    {
        return -1;
    }

    return effect_handle_get_echo_delay_samples(&pipeline->echo, delay_samples);
}