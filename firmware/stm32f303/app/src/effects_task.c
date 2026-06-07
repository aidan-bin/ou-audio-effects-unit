#include "effects_task.h"

#include "log.h"

static uint32_t compute_ticks_to_wait(const EffectsTaskContext *task_context,
                                      const EffectsTaskOps *ops)
{
    if (task_context == NULL || ops == NULL || ops->ms_to_ticks == NULL ||
        task_context->sampleBufLen == 0)
    {
        return 0;
    }

    uint32_t frame_duration_us = task_context->samplingPeriodUs * (uint32_t)task_context->sampleBufLen;
    uint32_t frame_duration_ms = frame_duration_us / 1000U;
    uint32_t frame_ticks = ops->ms_to_ticks(frame_duration_ms, ops->context);
    uint32_t slack_ticks = ops->ms_to_ticks(task_context->processingSlackMs, ops->context);

    return frame_ticks > slack_ticks ? frame_ticks - slack_ticks : 0;
}

bool effects_task_step(const EffectsTaskContext *task_context, const EffectsTaskOps *ops)
{
    if (task_context == NULL || ops == NULL || task_context->pipeline == NULL ||
        task_context->delaySamplesBuf == NULL || ops->wait_for_adc_buffer == NULL ||
        ops->wait_for_dac_buffer == NULL || ops->dma_copy == NULL || ops->alloc == NULL ||
        ops->free == NULL || ops->read_latched_state == NULL || ops->ms_to_ticks == NULL)
    {
        (void)log_write(LOG_LEVEL_ERROR, "effects task invalid context or ops");
        return false;
    }

    uint16_t *curr_adc_buf = NULL;
    uint16_t *curr_dac_buf = NULL;
    uint16_t *echo_input_buf = NULL;
    uint16_t *echo_delay_samples_buf = NULL;
    uint16_t *input_buf = NULL;
    uint16_t *output_buf = NULL;

    bool process_failed = false;
    EffectsState latched_effects_state;
    EffectsParams latched_effects_params;

    uint32_t ticks_to_wait = compute_ticks_to_wait(task_context, ops);

    if (!ops->wait_for_adc_buffer(0xFFFFFFFFU, &curr_adc_buf, ops->context))
    {
        (void)log_write(LOG_LEVEL_WARN, "effects task adc wait failed");
        return false;
    }

    if (curr_adc_buf != task_context->adcBufA && curr_adc_buf != task_context->adcBufB)
    {
        (void)log_write(LOG_LEVEL_ERROR, "effects task received invalid adc buffer");
        return false;
    }

    if (ops->replace_input_for_testing != NULL &&
        !ops->replace_input_for_testing(curr_adc_buf, task_context->sampleBufLen, ops->context))
    {
        (void)log_write(LOG_LEVEL_WARN, "effects task test input replacement failed");
        return false;
    }

    if (task_context->delaySamplesLen > task_context->sampleBufLen)
    {
        size_t shift_count = task_context->delaySamplesLen - task_context->sampleBufLen;
        if (!ops->dma_copy(&task_context->delaySamplesBuf[task_context->sampleBufLen],
                           task_context->delaySamplesBuf, shift_count, ticks_to_wait, ops->context))
        {
            (void)log_write(LOG_LEVEL_ERROR, "effects task delay sample shift failed");
            return false;
        }
    }

    if (!ops->dma_copy(
            curr_adc_buf,
            &task_context->delaySamplesBuf[task_context->delaySamplesLen - task_context->sampleBufLen],
            task_context->sampleBufLen, ticks_to_wait, ops->context))
    {
        (void)log_write(LOG_LEVEL_ERROR, "effects task delay sample capture failed");
        return false;
    }

    if (!ops->wait_for_dac_buffer(ticks_to_wait, &curr_dac_buf, ops->context))
    {
        (void)log_write(LOG_LEVEL_WARN, "effects task dac wait failed");
        return false;
    }

    if (curr_dac_buf != task_context->dacBufA && curr_dac_buf != task_context->dacBufB)
    {
        (void)log_write(LOG_LEVEL_ERROR, "effects task received invalid dac buffer");
        return false;
    }

    echo_delay_samples_buf =
        (uint16_t *)ops->alloc(task_context->delaySamplesLen * sizeof(uint16_t), ops->context);
    if (echo_delay_samples_buf == NULL)
    {
        process_failed = true;
        goto cleanup;
    }

    if (!ops->dma_copy(task_context->delaySamplesBuf, echo_delay_samples_buf,
                       task_context->delaySamplesLen, ticks_to_wait, ops->context))
    {
        process_failed = true;
        goto cleanup;
    }

    if (!ops->read_latched_state(&latched_effects_state, &latched_effects_params, ops->context))
    {
        process_failed = true;
        goto cleanup;
    }

    effects_state_normalize(&latched_effects_state);
    if (effects_pipeline_sync_params(task_context->pipeline, &latched_effects_params) != 0)
    {
        process_failed = true;
        goto cleanup;
    }

    input_buf = curr_adc_buf;
    output_buf = curr_dac_buf;

    for (int i = 0; i < NUM_EFFECTS; i++)
    {
        Effect effect = latched_effects_state.ordered[i];

        if (!latched_effects_state.isEnabled[effect])
        {
            continue;
        }

        if (effect == ECHO)
        {
            size_t num_delay_samples = 0;
            if (effects_pipeline_get_echo_delay_samples(task_context->pipeline, &num_delay_samples) !=
                0)
            {
                process_failed = true;
                break;
            }

            if (num_delay_samples > task_context->delaySamplesLen)
            {
                num_delay_samples = task_context->delaySamplesLen;
            }

            echo_input_buf = (uint16_t *)ops->alloc(
                (num_delay_samples + task_context->sampleBufLen) * sizeof(uint16_t), ops->context);
            if (echo_input_buf == NULL)
            {
                process_failed = true;
                break;
            }

            if (num_delay_samples > 0)
            {
                if (!ops->dma_copy(
                        &echo_delay_samples_buf[task_context->delaySamplesLen - num_delay_samples],
                        echo_input_buf, num_delay_samples, ticks_to_wait, ops->context))
                {
                    process_failed = true;
                    break;
                }
            }

            if (!ops->dma_copy(input_buf, &echo_input_buf[num_delay_samples], task_context->sampleBufLen,
                               ticks_to_wait, ops->context))
            {
                process_failed = true;
                break;
            }
        }

        if (effect == ECHO)
        {
            if (effects_pipeline_process(task_context->pipeline, effect, echo_input_buf, output_buf,
                                         task_context->sampleBufLen) != 0)
            {
                process_failed = true;
                break;
            }
        }
        else if (effects_pipeline_process(task_context->pipeline, effect, input_buf, output_buf,
                                          task_context->sampleBufLen) != 0)
        {
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
    if (echo_delay_samples_buf != NULL)
    {
        ops->free(echo_delay_samples_buf, ops->context);
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

    return !process_failed;
}
