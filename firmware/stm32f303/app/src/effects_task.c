#include "effects_task.h"

static uint32_t compute_ticks_to_wait(const EffectsTaskContext *taskContext,
                                      const EffectsTaskOps *ops)
{
    if (taskContext == NULL || ops == NULL || ops->ms_to_ticks == NULL ||
        taskContext->sampleBufLen == 0)
    {
        return 0;
    }

    uint32_t frameDurationUs = taskContext->samplingPeriodUs * (uint32_t)taskContext->sampleBufLen;
    uint32_t frameDurationMs = frameDurationUs / 1000U;
    uint32_t frameTicks = ops->ms_to_ticks(frameDurationMs, ops->context);
    uint32_t slackTicks = ops->ms_to_ticks(taskContext->processingSlackMs, ops->context);

    return frameTicks > slackTicks ? frameTicks - slackTicks : 0;
}

bool effects_task_step(const EffectsTaskContext *taskContext, const EffectsTaskOps *ops)
{
    if (taskContext == NULL || ops == NULL || taskContext->pipeline == NULL ||
        taskContext->delaySamplesBuf == NULL || ops->wait_for_adc_buffer == NULL ||
        ops->wait_for_dac_buffer == NULL || ops->dma_copy == NULL || ops->alloc == NULL ||
        ops->free == NULL || ops->read_latched_state == NULL || ops->ms_to_ticks == NULL)
    {
        return false;
    }

    uint16_t *currAdcBuf = NULL;
    uint16_t *currDacBuf = NULL;
    uint16_t *echoInputBuf = NULL;
    uint16_t *echoDelaySamplesBuf = NULL;
    uint16_t *inputBuf = NULL;
    uint16_t *outputBuf = NULL;

    bool processFailed = false;
    EffectsState latchedEffectsState;
    EffectsParams latchedEffectsParams;

    uint32_t ticksToWait = compute_ticks_to_wait(taskContext, ops);

    if (!ops->wait_for_adc_buffer(0xFFFFFFFFU, &currAdcBuf, ops->context))
    {
        return false;
    }

    if (currAdcBuf != taskContext->adcBufA && currAdcBuf != taskContext->adcBufB)
    {
        return false;
    }

    if (taskContext->delaySamplesLen > taskContext->sampleBufLen)
    {
        size_t shiftCount = taskContext->delaySamplesLen - taskContext->sampleBufLen;
        if (!ops->dma_copy(&taskContext->delaySamplesBuf[taskContext->sampleBufLen],
                           taskContext->delaySamplesBuf, shiftCount, ticksToWait, ops->context))
        {
            return false;
        }
    }

    if (!ops->dma_copy(
            currAdcBuf,
            &taskContext->delaySamplesBuf[taskContext->delaySamplesLen - taskContext->sampleBufLen],
            taskContext->sampleBufLen, ticksToWait, ops->context))
    {
        return false;
    }

    if (!ops->wait_for_dac_buffer(ticksToWait, &currDacBuf, ops->context))
    {
        return false;
    }

    if (currDacBuf != taskContext->dacBufA && currDacBuf != taskContext->dacBufB)
    {
        return false;
    }

    echoDelaySamplesBuf =
        (uint16_t *)ops->alloc(taskContext->delaySamplesLen * sizeof(uint16_t), ops->context);
    if (echoDelaySamplesBuf == NULL)
    {
        processFailed = true;
        goto cleanup;
    }

    if (!ops->dma_copy(taskContext->delaySamplesBuf, echoDelaySamplesBuf,
                       taskContext->delaySamplesLen, ticksToWait, ops->context))
    {
        processFailed = true;
        goto cleanup;
    }

    if (!ops->read_latched_state(&latchedEffectsState, &latchedEffectsParams, ops->context))
    {
        processFailed = true;
        goto cleanup;
    }

    effects_state_normalize(&latchedEffectsState);
    if (effects_pipeline_sync_params(taskContext->pipeline, &latchedEffectsParams) != 0)
    {
        processFailed = true;
        goto cleanup;
    }

    inputBuf = currAdcBuf;
    outputBuf = currDacBuf;

    for (int i = 0; i < NUM_EFFECTS; i++)
    {
        Effect effect = latchedEffectsState.ordered[i];

        if (!latchedEffectsState.isEnabled[effect])
        {
            continue;
        }

        if (effect == ECHO)
        {
            size_t numDelaySamples = 0;
            if (effects_pipeline_get_echo_delay_samples(taskContext->pipeline, &numDelaySamples) !=
                0)
            {
                processFailed = true;
                break;
            }

            if (numDelaySamples > taskContext->delaySamplesLen)
            {
                numDelaySamples = taskContext->delaySamplesLen;
            }

            echoInputBuf = (uint16_t *)ops->alloc(
                (numDelaySamples + taskContext->sampleBufLen) * sizeof(uint16_t), ops->context);
            if (echoInputBuf == NULL)
            {
                processFailed = true;
                break;
            }

            if (numDelaySamples > 0)
            {
                if (!ops->dma_copy(
                        &echoDelaySamplesBuf[taskContext->delaySamplesLen - numDelaySamples],
                        echoInputBuf, numDelaySamples, ticksToWait, ops->context))
                {
                    processFailed = true;
                    break;
                }
            }

            if (!ops->dma_copy(inputBuf, &echoInputBuf[numDelaySamples], taskContext->sampleBufLen,
                               ticksToWait, ops->context))
            {
                processFailed = true;
                break;
            }
        }

        if (effect == ECHO)
        {
            if (effects_pipeline_process(taskContext->pipeline, effect, echoInputBuf, outputBuf,
                                         taskContext->sampleBufLen) != 0)
            {
                processFailed = true;
                break;
            }
        }
        else if (effects_pipeline_process(taskContext->pipeline, effect, inputBuf, outputBuf,
                                          taskContext->sampleBufLen) != 0)
        {
            processFailed = true;
            break;
        }

        if (echoInputBuf != NULL)
        {
            ops->free(echoInputBuf, ops->context);
            echoInputBuf = NULL;
        }

        if (i < NUM_EFFECTS - 1)
        {
            uint16_t *temp = inputBuf;
            inputBuf = outputBuf;
            outputBuf = temp;
        }
    }

cleanup:
    if (echoInputBuf != NULL)
    {
        ops->free(echoInputBuf, ops->context);
    }
    if (echoDelaySamplesBuf != NULL)
    {
        ops->free(echoDelaySamplesBuf, ops->context);
    }

    if (processFailed && ops->report_failure != NULL)
    {
        ops->report_failure(ops->context);
    }

    return !processFailed;
}
