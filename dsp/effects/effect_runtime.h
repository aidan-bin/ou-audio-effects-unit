#ifndef EFFECT_RUNTIME_H
#define EFFECT_RUNTIME_H

#include <stddef.h>
#include <stdint.h>

#include "effects.h"

typedef enum {
    EFFECT_TYPE_OVERDRIVE = 0,
    EFFECT_TYPE_ECHO = 1,
    EFFECT_TYPE_COMPRESSION = 2,
} EffectType;

typedef union {
    OverdriveParam overdrive;
    EchoParam echo;
    CompressionParam compression;
} EffectParams;

typedef struct {
    EffectType type;
    EffectParams params;
} EffectInstance;

void effect_instance_init(EffectInstance* instance, EffectType type);
void effect_instance_reset(EffectInstance* instance);

int effect_instance_set_overdrive_params(EffectInstance* instance, const OverdriveParam* param);
int effect_instance_set_echo_params(EffectInstance* instance, const EchoParam* param);
int effect_instance_set_compression_params(EffectInstance* instance, const CompressionParam* param);

int effect_instance_process(const EffectInstance* instance, const uint16_t* in_buf, uint16_t* out_buf,
    size_t num_samples);

#endif