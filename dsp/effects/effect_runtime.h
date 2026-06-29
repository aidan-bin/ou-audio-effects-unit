#ifndef EFFECT_RUNTIME_H
#define EFFECT_RUNTIME_H

#include <stddef.h>
#include <stdint.h>

#include "effects.h"

typedef enum
{
    EFFECT_TYPE_OVERDRIVE = 0,
    EFFECT_TYPE_ECHO = 1,
    EFFECT_TYPE_COMPRESSION = 2,
} EffectType;

typedef union
{
    OverdriveParam overdrive;
    EchoParam echo;
    CompressionParam compression;
} EffectParams;

typedef struct
{
    EffectType type;
    EffectParams params;
    EchoState *echo_state;
} EffectInstance;

typedef struct
{
    EffectInstance instance;
    int initialized;
} EffectHandle;

void effect_instance_init(EffectInstance *instance, EffectType type);
void effect_instance_reset(EffectInstance *instance);

int effect_instance_set_overdrive_params(EffectInstance *instance, const OverdriveParam *param);
int effect_instance_set_echo_params(EffectInstance *instance, const EchoParam *param);
int effect_instance_set_compression_params(EffectInstance *instance, const CompressionParam *param);

int effect_instance_attach_echo_state(EffectInstance *instance, EchoState *state);
void effect_instance_set_enabled(EffectInstance *instance, int enabled);
void effect_instance_reset_state(EffectInstance *instance);

int effect_instance_process(const EffectInstance *instance, const uint16_t *in_buf,
                            uint16_t *out_buf, size_t num_samples);

int effect_handle_init(EffectHandle *handle, EffectType type);
int effect_handle_reset(EffectHandle *handle);
int effect_handle_set_overdrive_params(EffectHandle *handle, const OverdriveParam *param);
int effect_handle_set_echo_params(EffectHandle *handle, const EchoParam *param);
int effect_handle_set_compression_params(EffectHandle *handle, const CompressionParam *param);
int effect_handle_get_echo_delay_samples(const EffectHandle *handle, size_t *delay_samples);
int effect_handle_attach_echo_state(EffectHandle *handle, EchoState *state);
int effect_handle_set_enabled(EffectHandle *handle, int enabled);
int effect_handle_reset_state(EffectHandle *handle);
int effect_handle_process(const EffectHandle *handle, const uint16_t *in_buf, uint16_t *out_buf,
                          size_t num_samples);

#endif