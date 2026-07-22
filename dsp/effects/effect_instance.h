/*
 * effect_instance — one effect as a configurable instance with its parameters and state.
 */

#ifndef EFFECT_INSTANCE_H
#define EFFECT_INSTANCE_H

#include <stddef.h>
#include <stdint.h>

#include "effects.h"

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

void effect_instance_init(EffectInstance *instance, EffectType type);
void effect_instance_reset(EffectInstance *instance);

int effect_instance_set_overdrive_params(EffectInstance *instance, const OverdriveParam *param);
int effect_instance_set_echo_params(EffectInstance *instance, const EchoParam *param);
int effect_instance_set_compression_params(EffectInstance *instance, const CompressionParam *param);
int effect_instance_set_params(EffectInstance *instance, const void *param);
int effect_instance_get_echo_delay_samples(const EffectInstance *instance, size_t *delay_samples);

int effect_instance_attach_echo_state(EffectInstance *instance, EchoState *state);
void effect_instance_set_enabled(EffectInstance *instance, int enabled);
void effect_instance_reset_state(EffectInstance *instance);

int effect_instance_process(const EffectInstance *instance, const uint16_t *in_buf,
                            uint16_t *out_buf, size_t num_samples);

#endif