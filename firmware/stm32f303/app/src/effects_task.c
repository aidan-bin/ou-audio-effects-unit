#include "effects_task.h"

#include "log.h"

#define US_PER_MS 1000U
#define INFINITE_TIMEOUT_TICKS 0xFFFFFFFFU

static uint32_t compute_ticks_to_wait(const EffectsTaskContext *task_context,
                                      const EffectsTaskOps *ops)
{
    if (task_context == NULL || ops == NULL || ops->ms_to_ticks == NULL ||
        task_context->sample_buf_len == 0)
    {
        return 0;
    }

    uint32_t frame_duration_us = task_context->sampling_period_us * (uint32_t)task_context->sample_buf_len;
    uint32_t frame_duration_ms = frame_duration_us / US_PER_MS;
    uint32_t frame_ticks = ops->ms_to_ticks(frame_duration_ms, ops->context);
    uint32_t slack_ticks = ops->ms_to_ticks(task_context->processing_slack_ms, ops->context);

    return frame_ticks > slack_ticks ? frame_ticks - slack_ticks : 0;
}

bool effects_task_step(const EffectsTaskContext *task_context, const EffectsTaskOps *ops)
{
    if (task_context == NULL || ops == NULL || task_context->pipeline == NULL ||
        task_context->delay_samples_buf == NULL || ops->wait_for_adc_buffer == NULL ||
        ops->wait_for_dac_buffer == NULL || ops->dma_copy == NULL || ops->alloc == NULL ||
        ops->free == NULL || ops->read_latched_state == NULL || ops->ms_to_ticks == NULL)
    {
        (void)log_write(LOG_LEVEL_ERROR, "effects task invalid context or ops");
        if (ops != NULL && ops->panic_write != NULL)
        {
            ops->panic_write("panic: effects task invalid context or ops\n", ops->context);
        }
        return false;
    }

    uint16_t *curr_adc_buf = NULL;
    uint16_t *curr_dac_buf = NULL;
    uint16_t *echo_input_buf = NULL;
    uint16_t *input_buf = NULL;
    uint16_t *output_buf = NULL;

    bool process_failed = false;
    EffectsState latched_effects_state;
    EffectsParams latched_effects_params;

    bool profiling = ops->get_timestamp_us != NULL;
    uint32_t frame_start_us = profiling ? ops->get_timestamp_us(ops->context) : 0;

    if (ops->on_frame_begin != NULL)
    {
        ops->on_frame_begin(ops->context);
    }

    uint32_t ticks_to_wait = compute_ticks_to_wait(task_context, ops);

    if (!ops->wait_for_adc_buffer(INFINITE_TIMEOUT_TICKS, &curr_adc_buf, ops->context))
    {
        (void)log_write(LOG_LEVEL_WARN, "effects task adc wait failed");
        if (ops->panic_write != NULL)
        {
            ops->panic_write("panic: adc wait failed\n", ops->context);
        }
        return false;
    }

    if (curr_adc_buf != task_context->adc_buf_a && curr_adc_buf != task_context->adc_buf_b)
    {
        (void)log_write(LOG_LEVEL_ERROR, "effects task received invalid adc buffer");
        if (ops->panic_write != NULL)
        {
            ops->panic_write("panic: invalid adc buffer\n", ops->context);
        }
        return false;
    }

    if (ops->replace_input_for_testing != NULL &&
        !ops->replace_input_for_testing(curr_adc_buf, task_context->sample_buf_len, ops->context))
    {
        (void)log_write(LOG_LEVEL_WARN, "effects task test input replacement failed");
        if (ops->panic_write != NULL)
        {
            ops->panic_write("panic: test input replacement failed\n", ops->context);
        }
        return false;
    }

    if (task_context->delay_samples_len > task_context->sample_buf_len)
    {
        size_t shift_count = task_context->delay_samples_len - task_context->sample_buf_len;
        if (!ops->dma_copy(&task_context->delay_samples_buf[task_context->sample_buf_len],
                           task_context->delay_samples_buf, shift_count, ticks_to_wait, ops->context))
        {
            (void)log_write(LOG_LEVEL_ERROR, "effects task delay sample shift failed");
            if (ops->panic_write != NULL)
            {
                ops->panic_write("panic: delay sample shift failed\n", ops->context);
            }
            return false;
        }
    }

    if (!ops->dma_copy(
            curr_adc_buf,
            &task_context->delay_samples_buf[task_context->delay_samples_len - task_context->sample_buf_len],
            task_context->sample_buf_len, ticks_to_wait, ops->context))
    {
        (void)log_write(LOG_LEVEL_ERROR, "effects task delay sample capture failed");
        if (ops->panic_write != NULL)
        {
            ops->panic_write("panic: delay sample capture failed\n", ops->context);
        }
        return false;
    }

    if (!ops->wait_for_dac_buffer(ticks_to_wait, &curr_dac_buf, ops->context))
    {
        if (curr_adc_buf == task_context->adc_buf_a)
        {
            curr_dac_buf = task_context->dac_buf_a;
        }
        else if (curr_adc_buf == task_context->adc_buf_b)
        {
            curr_dac_buf = task_context->dac_buf_b;
        }
        else
        {
            if (ops->panic_write != NULL)
            {
                ops->panic_write("panic: dac wait failed\n", ops->context);
            }
            return false;
        }
    }

    if (curr_dac_buf != task_context->dac_buf_a && curr_dac_buf != task_context->dac_buf_b)
    {
        (void)log_write(LOG_LEVEL_ERROR, "effects task received invalid dac buffer");
        if (ops->panic_write != NULL)
        {
            ops->panic_write("panic: invalid dac buffer\n", ops->context);
        }
        return false;
    }

    if (!ops->read_latched_state(&latched_effects_state, &latched_effects_params, ops->context))
    {
        (void)log_write(LOG_LEVEL_ERROR, "process_failed: read_latched_state");
        process_failed = true;
        goto cleanup;
    }

    effects_state_normalize(&latched_effects_state);

    bool any_enabled = false;
    for (int i = 0; i < NUM_EFFECTS; i++)
    {
        if (latched_effects_state.is_enabled[latched_effects_state.ordered[i]])
        {
            any_enabled = true;
            break;
        }
    }

    if (!any_enabled)
    {
        if (!ops->dma_copy(curr_adc_buf, curr_dac_buf, task_context->sample_buf_len,
                           ticks_to_wait, ops->context))
        {
            (void)log_write(LOG_LEVEL_ERROR, "process_failed: pass-through copy");
            if (ops->panic_write != NULL)
            {
                ops->panic_write("panic: pass-through copy failed\n", ops->context);
            }
            process_failed = true;
        }
        goto cleanup;
    }

    input_buf = curr_adc_buf;
    output_buf = curr_dac_buf;

    if (effects_pipeline_sync_params(task_context->pipeline, &latched_effects_params) != 0)
    {
        (void)log_write(LOG_LEVEL_ERROR, "process_failed: pipeline_sync_params");
        process_failed = true;
        goto cleanup;
    }

    for (int i = 0; i < NUM_EFFECTS; i++)
    {
        Effect effect = latched_effects_state.ordered[i];

        if (!latched_effects_state.is_enabled[effect])
        {
            continue;
        }

        if (effect == ECHO)
        {
            size_t num_delay_samples = 0;
            if (effects_pipeline_get_echo_delay_samples(task_context->pipeline, &num_delay_samples) !=
                0)
            {
                (void)log_write(LOG_LEVEL_ERROR, "process_failed: get_echo_delay_samples");
                process_failed = true;
                break;
            }

            if (num_delay_samples > task_context->delay_samples_len)
            {
                num_delay_samples = task_context->delay_samples_len;
            }

            echo_input_buf = (uint16_t *)ops->alloc(
                (num_delay_samples + task_context->sample_buf_len) * sizeof(uint16_t), ops->context);
            if (echo_input_buf == NULL)
            {
                (void)log_write(LOG_LEVEL_ERROR, "process_failed: echo_input_buf alloc");
                process_failed = true;
                break;
            }

            if (num_delay_samples > 0)
            {
                if (!ops->dma_copy(
                        &task_context->delay_samples_buf[task_context->delay_samples_len - num_delay_samples],
                        echo_input_buf, num_delay_samples, ticks_to_wait, ops->context))
                {
                    (void)log_write(LOG_LEVEL_ERROR, "process_failed: echo delay history dma copy");
                    process_failed = true;
                    break;
                }
            }

            if (!ops->dma_copy(input_buf, &echo_input_buf[num_delay_samples], task_context->sample_buf_len,
                               ticks_to_wait, ops->context))
            {
                (void)log_write(LOG_LEVEL_ERROR, "process_failed: echo input dma copy");
                process_failed = true;
                break;
            }
        }

        if (effect == ECHO)
        {
            if (effects_pipeline_process(task_context->pipeline, effect, echo_input_buf, output_buf,
                                         task_context->sample_buf_len) != 0)
            {
                (void)log_write(LOG_LEVEL_ERROR, "process_failed: echo pipeline process");
                process_failed = true;
                break;
            }
        }
        else if (effects_pipeline_process(task_context->pipeline, effect, input_buf, output_buf,
                                          task_context->sample_buf_len) != 0)
        {
            (void)log_write(LOG_LEVEL_ERROR, "process_failed: effect pipeline process");
            process_failed = true;
            break;
        }

        if (echo_input_buf != NULL)
        {
            ops->free(echo_input_buf, ops->context);
            echo_input_buf = NULL;
        }

        if (i < NUM_EFFECTS - 1)
        {
            uint16_t *temp = input_buf;
            input_buf = output_buf;
            output_buf = temp;
        }
    }

cleanup:
    if (echo_input_buf != NULL)
    {
        ops->free(echo_input_buf, ops->context);
    }

    if (!process_failed && ops->replace_output_for_testing != NULL &&
        !ops->replace_output_for_testing(curr_dac_buf, task_context->sample_buf_len, ops->context))
    {
        (void)log_write(LOG_LEVEL_ERROR, "process_failed: test output replacement");
        if (ops->panic_write != NULL)
        {
            ops->panic_write("panic: test output replacement failed\n", ops->context);
        }
        process_failed = true;
    }

    if (process_failed && ops->report_failure != NULL)
    {
        (void)log_write(LOG_LEVEL_ERROR, "effects task frame processing failed");
        ops->report_failure(ops->context);
    }

    if (!process_failed && ops->report_frame_complete != NULL)
    {
        ops->report_frame_complete(ops->context);
    }

    if (profiling)
    {
        uint32_t frame_time_us = ops->get_timestamp_us(ops->context) - frame_start_us;
        uint32_t frame_budget_us = task_context->sampling_period_us * (uint32_t)task_context->sample_buf_len;
        uint32_t slack_us = (uint32_t)task_context->processing_slack_ms * US_PER_MS;
        uint32_t total_budget_us = frame_budget_us + slack_us;
        bool overrun = frame_time_us > total_budget_us;
        if (ops->on_frame_end != NULL)
        {
            ops->on_frame_end(frame_time_us, overrun, ops->context);
        }
    }

    return !process_failed;
}
