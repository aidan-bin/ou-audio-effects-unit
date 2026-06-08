#ifndef EFFECTS_TASK_H
#define EFFECTS_TASK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "effects_pipeline.h"

typedef struct
{
    EffectsPipeline *pipeline;
    volatile EffectsState *effectsState;
    volatile EffectsParams *effectsParams;

    uint16_t *adcBufA;
    uint16_t *adcBufB;
    uint16_t *dacBufA;
    uint16_t *dacBufB;
    uint16_t *delaySamplesBuf;

    size_t sampleBufLen;
    size_t delaySamplesLen;

    uint32_t samplingPeriodUs;
    uint32_t processingSlackMs;
} EffectsTaskContext;

typedef struct
{
    bool (*wait_for_adc_buffer)(uint32_t timeoutTicks, uint16_t **bufPtr, void *context);
    bool (*wait_for_dac_buffer)(uint32_t timeoutTicks, uint16_t **bufPtr, void *context);

    bool (*dma_copy)(const uint16_t *src, uint16_t *dst, size_t count, uint32_t timeoutTicks,
                     void *context);

    void *(*alloc)(size_t size, void *context);
    void (*free)(void *ptr, void *context);

    bool (*read_latched_state)(EffectsState *state, EffectsParams *params, void *context);
    bool (*replace_input_for_testing)(uint16_t *buf, size_t count, void *context);
    void (*report_failure)(void *context);
    void (*report_frame_complete)(void *context);

    uint32_t (*ms_to_ticks)(uint32_t ms, void *context);

    void (*on_frame_begin)(void *context);
    void (*on_frame_end)(uint32_t frame_time_us, bool overrun, void *context);
    uint32_t (*get_timestamp_us)(void *context);

    void (*panic_write)(const char *text, void *context);
    void *context;
} EffectsTaskOps;

bool effects_task_step(const EffectsTaskContext *taskContext, const EffectsTaskOps *ops);

#endif