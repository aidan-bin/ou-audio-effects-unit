#include "effect_runtime.h"
#include "fast_math.h"

#include <string.h>

static OverdriveParam normalize_overdrive_params(const OverdriveParam *param);
static EchoParam normalize_echo_params(const EchoParam *param);
static CompressionParam normalize_compression_params(const CompressionParam *param);

static OverdriveParam default_overdrive_params(void)
{
    return (OverdriveParam){
        .level = MAX_OVERDRIVE_LEVEL,
        .gain = MAX_OVERDRIVE_GAIN,
        .tone = MIN_OVERDRIVE_TONE,
        .mix = 0,
    };
}

static EchoParam default_echo_params(void)
{
    return (EchoParam){
        .delay_samples = 1,
        .pre_delay = MIN_ECHO_PRE_DELAY,
        .density = MAX_ECHO_DENSITY,
        .attack = MAX_ECHO_ATTACK,
        .decay = MAX_ECHO_DECAY,
    };
}

static CompressionParam default_compression_params(void)
{
    return (CompressionParam){
        .threshold = X_AXIS,
        .ratio = 0,
    };
}

void effect_instance_init(EffectInstance *instance, EffectType type)
{
    if (instance == NULL)
    {
        return;
    }

    instance->type = type;
    effect_instance_reset(instance);
}

void effect_instance_reset(EffectInstance *instance)
{
    if (instance == NULL)
    {
        return;
    }

    switch (instance->type)
    {
    case EFFECT_TYPE_OVERDRIVE:
        instance->params.overdrive = default_overdrive_params();
        break;
    case EFFECT_TYPE_ECHO:
        instance->params.echo = default_echo_params();
        break;
    case EFFECT_TYPE_COMPRESSION:
        instance->params.compression = default_compression_params();
        break;
    default:
        memset(&instance->params, 0, sizeof(instance->params));
        break;
    }
}

int effect_instance_set_overdrive_params(EffectInstance *instance, const OverdriveParam *param)
{
    if (instance == NULL || param == NULL || instance->type != EFFECT_TYPE_OVERDRIVE)
    {
        return -1;
    }

    instance->params.overdrive = normalize_overdrive_params(param);
    return 0;
}

int effect_instance_set_echo_params(EffectInstance *instance, const EchoParam *param)
{
    if (instance == NULL || param == NULL || instance->type != EFFECT_TYPE_ECHO)
    {
        return -1;
    }

    instance->params.echo = normalize_echo_params(param);
    return 0;
}

int effect_instance_set_compression_params(EffectInstance *instance,
                                           const CompressionParam *param)
{
    if (instance == NULL || param == NULL || instance->type != EFFECT_TYPE_COMPRESSION)
    {
        return -1;
    }

    instance->params.compression = normalize_compression_params(param);
    return 0;
}

int effect_instance_process(const EffectInstance *instance, const uint16_t *in_buf,
                            uint16_t *out_buf, size_t num_samples)
{
    if (instance == NULL || in_buf == NULL || out_buf == NULL)
    {
        return -1;
    }

    switch (instance->type)
    {
    case EFFECT_TYPE_OVERDRIVE:
        buf_overdrive(in_buf, out_buf, num_samples, &instance->params.overdrive);
        return 0;
    case EFFECT_TYPE_ECHO:
        buf_echo(in_buf, out_buf, num_samples, &instance->params.echo);
        return 0;
    case EFFECT_TYPE_COMPRESSION:
        buf_compression(in_buf, out_buf, num_samples, &instance->params.compression);
        return 0;
    default:
        return -1;
    }
}

int effect_handle_init(EffectHandle *handle, EffectType type)
{
    if (handle == NULL)
    {
        return -1;
    }

    effect_instance_init(&handle->instance, type);
    handle->initialized = 1;
    return 0;
}

int effect_handle_reset(EffectHandle *handle)
{
    if (handle == NULL || handle->initialized == 0)
    {
        return -1;
    }

    effect_instance_reset(&handle->instance);
    return 0;
}

int effect_handle_set_overdrive_params(EffectHandle *handle, const OverdriveParam *param)
{
    if (handle == NULL || handle->initialized == 0)
    {
        return -1;
    }

    return effect_instance_set_overdrive_params(&handle->instance, param);
}

int effect_handle_set_echo_params(EffectHandle *handle, const EchoParam *param)
{
    if (handle == NULL || handle->initialized == 0)
    {
        return -1;
    }

    return effect_instance_set_echo_params(&handle->instance, param);
}

int effect_handle_set_compression_params(EffectHandle *handle, const CompressionParam *param)
{
    if (handle == NULL || handle->initialized == 0)
    {
        return -1;
    }

    return effect_instance_set_compression_params(&handle->instance, param);
}

int effect_handle_get_echo_delay_samples(const EffectHandle *handle, size_t *delay_samples)
{
    if (handle == NULL || delay_samples == NULL || handle->initialized == 0 ||
        handle->instance.type != EFFECT_TYPE_ECHO)
    {
        return -1;
    }

    *delay_samples = handle->instance.params.echo.delay_samples;
    return 0;
}

int effect_handle_process(const EffectHandle *handle, const uint16_t *in_buf, uint16_t *out_buf,
                          size_t num_samples)
{
    if (handle == NULL || handle->initialized == 0)
    {
        return -1;
    }

    return effect_instance_process(&handle->instance, in_buf, out_buf, num_samples);
}

static OverdriveParam normalize_overdrive_params(const OverdriveParam *param)
{
    OverdriveParam out = *param;

    out.level = clamp_range(out.level, 0, MAX_OVERDRIVE_LEVEL);
    out.gain = clamp_qn(out.gain);
    out.tone = clamp_range(out.tone, MIN_OVERDRIVE_TONE, MAX_OVERDRIVE_TONE);
    out.mix = clamp_qn(out.mix);

    return out;
}

static EchoParam normalize_echo_params(const EchoParam *param)
{
    EchoParam out = *param;

    out.delay_samples = clamp_range(out.delay_samples, 0, MAX_ECHO_DELAY_SAMPLES);
    if (out.delay_samples == 0)
    {
        out.pre_delay = 0;
    }
    else
    {
        out.pre_delay = clamp_range(out.pre_delay, MIN_ECHO_PRE_DELAY, out.delay_samples);
    }

    out.density = clamp_qn(out.density);
    out.attack = clamp_qn(out.attack);
    out.decay = clamp_qn(out.decay);

    return out;
}

static CompressionParam normalize_compression_params(const CompressionParam *param)
{
    CompressionParam out = *param;

    out.threshold = clamp_range(out.threshold, 0, X_AXIS);
    out.ratio = clamp_qn(out.ratio);

    return out;
}