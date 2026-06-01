#include "peripheral_dispatch.h"

#include <stddef.h>

void peripheral_drain_semaphore(void *semaphoreHandle, const PeripheralDispatchOps *ops)
{
    if (semaphoreHandle == NULL || ops == NULL || ops->semaphore_take == NULL)
    {
        return;
    }

    while (ops->semaphore_take(semaphoreHandle, 0, ops->context))
    {
    }
}

bool peripheral_start_dma_transfer_and_wait(void *dmaHandle, uint32_t srcAddress, uint32_t dstAddress,
    uint32_t dataLength, void *completionSemaphoreHandle, uint32_t ticksToWait,
    const PeripheralDispatchOps *ops)
{
    if (dmaHandle == NULL || completionSemaphoreHandle == NULL || ops == NULL ||
        ops->dma_start_it == NULL || ops->semaphore_take == NULL)
    {
        return false;
    }

    peripheral_drain_semaphore(completionSemaphoreHandle, ops);

    if (!ops->dma_start_it(dmaHandle, srcAddress, dstAddress, dataLength, ops->context))
    {
        return false;
    }

    return ops->semaphore_take(completionSemaphoreHandle, ticksToWait, ops->context);
}

bool peripheral_start_adc_conversion_it(void *adcHandle, void *config, const PeripheralDispatchOps *ops)
{
    if (adcHandle == NULL || config == NULL || ops == NULL || ops->adc_config_channel == NULL ||
        ops->adc_start_it == NULL)
    {
        return false;
    }

    if (!ops->adc_config_channel(adcHandle, config, ops->context))
    {
        return false;
    }

    return ops->adc_start_it(adcHandle, ops->context);
}

void peripheral_on_gpio_exti(const PeripheralDispatchContext *dispatch, const PeripheralDispatchOps *ops,
    uint16_t gpioPin)
{
    if (dispatch == NULL || ops == NULL || ops->task_notify_from_isr == NULL)
    {
        return;
    }

    ops->task_notify_from_isr(dispatch->btnHandlerTaskHandle, (uint32_t)gpioPin, ops->context);
}

void peripheral_on_adc_half_complete(const PeripheralDispatchContext *dispatch, const PeripheralDispatchOps *ops,
    void *adcHandle)
{
    if (dispatch == NULL || ops == NULL || ops->task_notify_from_isr == NULL)
    {
        return;
    }

    if (adcHandle == dispatch->effectsAdcHandle)
    {
        ops->task_notify_from_isr(dispatch->effectsTaskHandle, (uint32_t)(uintptr_t)dispatch->adcBufA,
            ops->context);
    }
}

void peripheral_on_adc_complete(const PeripheralDispatchContext *dispatch, const PeripheralDispatchOps *ops,
    void *adcHandle)
{
    if (dispatch == NULL || ops == NULL || ops->task_notify_from_isr == NULL)
    {
        return;
    }

    if (adcHandle == dispatch->potAdcHandle)
    {
        if (ops->adc_get_value == NULL)
        {
            return;
        }

        uint32_t value = ops->adc_get_value(adcHandle, ops->context);
        ops->task_notify_from_isr(dispatch->potHandlerTaskHandle, value, ops->context);
        return;
    }

    if (adcHandle == dispatch->effectsAdcHandle)
    {
        ops->task_notify_from_isr(dispatch->effectsTaskHandle, (uint32_t)(uintptr_t)dispatch->adcBufB,
            ops->context);
    }
}

void peripheral_on_dac_half_complete(const PeripheralDispatchContext *dispatch, const PeripheralDispatchOps *ops,
    void *dacHandle)
{
    (void)dacHandle;

    if (dispatch == NULL || ops == NULL || ops->task_notify_from_isr == NULL)
    {
        return;
    }

    ops->task_notify_from_isr(dispatch->effectsTaskHandle, (uint32_t)(uintptr_t)dispatch->dacBufA,
        ops->context);
}

void peripheral_on_dac_complete(const PeripheralDispatchContext *dispatch, const PeripheralDispatchOps *ops,
    void *dacHandle)
{
    (void)dacHandle;

    if (dispatch == NULL || ops == NULL || ops->task_notify_from_isr == NULL)
    {
        return;
    }

    ops->task_notify_from_isr(dispatch->effectsTaskHandle, (uint32_t)(uintptr_t)dispatch->dacBufB,
        ops->context);
}

void peripheral_on_i2c_tx_complete(const PeripheralDispatchContext *dispatch, const PeripheralDispatchOps *ops,
    void *i2cHandle)
{
    if (dispatch == NULL || ops == NULL || ops->i2c_signal_completion_from_isr == NULL)
    {
        return;
    }

    if (i2cHandle == dispatch->i2cHandle)
    {
        ops->i2c_signal_completion_from_isr(dispatch->i2cTaskSupportContext, false, ops->context);
    }
}

void peripheral_on_i2c_rx_complete(const PeripheralDispatchContext *dispatch, const PeripheralDispatchOps *ops,
    void *i2cHandle)
{
    if (dispatch == NULL || ops == NULL || ops->i2c_signal_completion_from_isr == NULL)
    {
        return;
    }

    if (i2cHandle == dispatch->i2cHandle)
    {
        ops->i2c_signal_completion_from_isr(dispatch->i2cTaskSupportContext, false, ops->context);
    }
}

void peripheral_on_i2c_error(const PeripheralDispatchContext *dispatch, const PeripheralDispatchOps *ops,
    void *i2cHandle)
{
    if (dispatch == NULL || ops == NULL || ops->i2c_signal_completion_from_isr == NULL)
    {
        return;
    }

    if (i2cHandle == dispatch->i2cHandle)
    {
        ops->i2c_signal_completion_from_isr(dispatch->i2cTaskSupportContext, true, ops->context);
    }
}

void peripheral_on_i2c_abort_complete(const PeripheralDispatchContext *dispatch, const PeripheralDispatchOps *ops,
    void *i2cHandle)
{
    if (dispatch == NULL || ops == NULL || ops->i2c_signal_completion_from_isr == NULL)
    {
        return;
    }

    if (i2cHandle == dispatch->i2cHandle)
    {
        ops->i2c_signal_completion_from_isr(dispatch->i2cTaskSupportContext, true, ops->context);
    }
}

void peripheral_on_dma_complete(const PeripheralDispatchContext *dispatch, const PeripheralDispatchOps *ops,
    void *dmaHandle)
{
    if (dispatch == NULL || ops == NULL || ops->semaphore_give_from_isr == NULL)
    {
        return;
    }

    if (dmaHandle == dispatch->memToMemDmaHandle)
    {
        ops->semaphore_give_from_isr(dispatch->delaySamplesDmaSemaphoreHandle, ops->context);
    }
}
