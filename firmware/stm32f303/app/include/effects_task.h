#ifndef EFFECTS_TASK_H
#define EFFECTS_TASK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "effects_pipeline.h"

typedef struct
{
    EffectsPipeline *pipeline;
    volatile EffectsState *effects_state;
    volatile EffectsParams *effects_params;

    uint16_t *adc_buf_a;
    uint16_t *adc_buf_b;
    uint16_t *dac_buf_a;
    uint16_t *dac_buf_b;
    uint16_t *delay_samples_buf;

    uint16_t *echo_scratch_buf;
    size_t echo_scratch_len;

    size_t sample_buf_len;
    size_t delay_samples_len;

    uint32_t sampling_period_us;
    uint32_t processing_slack_ms;
} EffectsTaskContext;

typedef struct
{
    bool (*wait_for_adc_buffer)(uint32_t timeout_ticks, uint16_t **buf_ptr, void *context);
    bool (*wait_for_dac_buffer)(uint32_t timeout_ticks, uint16_t **buf_ptr, void *context);

    bool (*dma_copy)(const uint16_t *src, uint16_t *dst, size_t count, uint32_t timeout_ticks,
                     void *context);

    bool (*read_latched_state)(EffectsState *state, EffectsParams *params, void *context);
    bool (*replace_input_for_testing)(uint16_t *buf, size_t count, void *context);
    bool (*replace_output_for_testing)(uint16_t *buf, size_t count, void *context);
    void (*report_failure)(void *context);
    void (*report_frame_complete)(void *context);

    uint32_t (*ms_to_ticks)(uint32_t ms, void *context);

    void (*on_frame_begin)(void *context);
    void (*on_frame_end)(uint32_t frame_time_us, bool overrun, void *context);
    uint32_t (*get_timestamp_us)(void *context);

    void (*panic_write)(const char *text, void *context);
    void (*on_output_frame)(const uint16_t *buf, size_t count, void *context);
    void *context;
} EffectsTaskOps;

bool effects_task_step(const EffectsTaskContext *task_context, const EffectsTaskOps *ops);

#endif