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
    instance->echo_state = NULL;
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

int effect_instance_set_params(EffectInstance *instance, const void *param)
{
    if (instance == NULL || param == NULL)
    {
        return -1;
    }

    switch (instance->type)
    {
    case EFFECT_TYPE_OVERDRIVE:
        return effect_instance_set_overdrive_params(instance, (const OverdriveParam *)param);
    case EFFECT_TYPE_ECHO:
        return effect_instance_set_echo_params(instance, (const EchoParam *)param);
    case EFFECT_TYPE_COMPRESSION:
        return effect_instance_set_compression_params(instance, (const CompressionParam *)param);
    default:
        return -1;
    }
}

int effect_instance_attach_echo_state(EffectInstance *instance, EchoState *state)
{
    if (instance == NULL || instance->type != EFFECT_TYPE_ECHO)
    {
        return -1;
    }

    instance->echo_state = state;
    return 0;
}

void effect_instance_reset_state(EffectInstance *instance)
{
    if (instance == NULL)
    {
        return;
    }

    if (instance->type == EFFECT_TYPE_ECHO && instance->echo_state != NULL)
    {
        echo_state_reset(instance->echo_state);
    }
}

void effect_instance_set_enabled(EffectInstance *instance, int enabled)
{
    if (instance == NULL)
    {
        return;
    }

    if (instance->type == EFFECT_TYPE_ECHO && instance->echo_state != NULL)
    {
        if (enabled && !instance->echo_state->prev_enabled)
        {
            echo_state_reset(instance->echo_state);
        }
        instance->echo_state->prev_enabled = enabled ? 1 : 0;
    }
}

static void echo_process_stateful(EchoState *st, const EchoParam *param, const uint16_t *in_buf,
                                  uint16_t *out_buf, size_t num_samples)
{
    if (st->history != NULL && st->history_len > 0)
    {
        size_t len = st->history_len;
        for (size_t k = 0; k < num_samples; k++)
        {
            st->history[(st->history_w + k) % len] = in_buf[k];
        }

        size_t delay_mod = param->delay_samples % len;
        size_t start_idx = (st->history_w + len - delay_mod) % len;
        buf_echo_ring(st->history, len, start_idx, out_buf, num_samples, param);
        st->history_w = (st->history_w + num_samples) % len;
    }
    else
    {
        for (size_t k = 0; k < num_samples; k++)
        {
            out_buf[k] = in_buf[k];
        }
    }

    buf_echo_feedback(&st->fb, in_buf, out_buf, num_samples, param);
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
        if (instance->echo_state != NULL)
        {
            echo_process_stateful(instance->echo_state, &instance->params.echo, in_buf, out_buf,
                                  num_samples);
        }
        else
        {
            buf_echo(in_buf, out_buf, num_samples, &instance->params.echo);
        }
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

int effect_handle_set_params(EffectHandle *handle, const void *param)
{
    if (handle == NULL || handle->initialized == 0)
    {
        return -1;
    }

    return effect_instance_set_params(&handle->instance, param);
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

int effect_handle_attach_echo_state(EffectHandle *handle, EchoState *state)
{
    if (handle == NULL || handle->initialized == 0)
    {
        return -1;
    }

    return effect_instance_attach_echo_state(&handle->instance, state);
}

int effect_handle_reset_state(EffectHandle *handle)
{
    if (handle == NULL || handle->initialized == 0)
    {
        return -1;
    }

    effect_instance_reset_state(&handle->instance);
    return 0;
}

int effect_handle_set_enabled(EffectHandle *handle, int enabled)
{
    if (handle == NULL || handle->initialized == 0)
    {
        return -1;
    }

    effect_instance_set_enabled(&handle->instance, enabled);
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

    out.feedback = clamp_range(out.feedback, 0, MAX_ECHO_FEEDBACK);
    out.feedback_delay = clamp_range(out.feedback_delay, 0, MAX_ECHO_FEEDBACK_DELAY);
    out.damping = clamp_range(out.damping, 0, MAX_ECHO_DAMPING);

    return out;
}

static CompressionParam normalize_compression_params(const CompressionParam *param)
{
    CompressionParam out = *param;

    out.threshold = clamp_range(out.threshold, 0, X_AXIS);
    out.ratio = clamp_qn(out.ratio);

    return out;
}