#include "peripheral_dispatch.h"

#include <stddef.h>

#include "log.h"

void peripheral_drain_semaphore(void *semaphore_handle, const PeripheralDispatchOps *ops)
{
    if (semaphore_handle == NULL || ops == NULL || ops->semaphore_take == NULL)
    {
        return;
    }

    while (ops->semaphore_take(semaphore_handle, 0, ops->context))
    {
    }
}

bool peripheral_start_dma_transfer_and_wait(void *dma_handle, uint32_t src_address,
                                            uint32_t dst_address, uint32_t data_length,
                                            void *completion_semaphore_handle, uint32_t ticks_to_wait,
                                            const PeripheralDispatchOps *ops)
{
    if (dma_handle == NULL || completion_semaphore_handle == NULL || ops == NULL ||
        ops->dma_start_it == NULL || ops->semaphore_take == NULL)
    {
        return false;
    }

    peripheral_drain_semaphore(completion_semaphore_handle, ops);

    if (!ops->dma_start_it(dma_handle, src_address, dst_address, data_length, ops->context))
    {
        return false;
    }

    return ops->semaphore_take(completion_semaphore_handle, ticks_to_wait, ops->context);
}

bool peripheral_start_adc_conversion_it(void *adc_handle, void *config,
                                        const PeripheralDispatchOps *ops)
{
    if (adc_handle == NULL || config == NULL || ops == NULL || ops->adc_config_channel == NULL ||
        ops->adc_start_it == NULL)
    {
        return false;
    }

    if (!ops->adc_config_channel(adc_handle, config, ops->context))
    {
        return false;
    }

    return ops->adc_start_it(adc_handle, ops->context);
}

void peripheral_on_gpio_exti(const PeripheralDispatchContext *dispatch,
                             const PeripheralDispatchOps *ops, uint16_t gpio_pin)
{
    if (dispatch == NULL || ops == NULL || ops->task_notify_from_isr == NULL)
    {
        return;
    }

    ops->task_notify_from_isr(dispatch->btn_handler_task_handle, (uint32_t)gpio_pin, ops->context);
}

void peripheral_on_adc_half_complete(const PeripheralDispatchContext *dispatch,
                                     const PeripheralDispatchOps *ops, void *adc_handle)
{
    if (dispatch == NULL || ops == NULL || ops->task_notify_from_isr == NULL)
    {
        return;
    }

    if (adc_handle == dispatch->effects_adc_handle)
    {
        ops->task_notify_from_isr(dispatch->effects_task_handle,
                                  (uint32_t)(uintptr_t)dispatch->adc_buf_a, ops->context);
    }
}

void peripheral_on_adc_complete(const PeripheralDispatchContext *dispatch,
                                const PeripheralDispatchOps *ops, void *adc_handle)
{
    if (dispatch == NULL || ops == NULL || ops->task_notify_from_isr == NULL)
    {
        return;
    }

    if (adc_handle == dispatch->pot_adc_handle)
    {
        if (ops->adc_get_value == NULL)
        {
            return;
        }

        uint32_t value = ops->adc_get_value(adc_handle, ops->context);
        ops->task_notify_from_isr(dispatch->pot_handler_task_handle, value, ops->context);
        return;
    }

    if (adc_handle == dispatch->effects_adc_handle)
    {
        ops->task_notify_from_isr(dispatch->effects_task_handle,
                                  (uint32_t)(uintptr_t)dispatch->adc_buf_b, ops->context);
    }
}

void peripheral_on_dac_half_complete(const PeripheralDispatchContext *dispatch,
                                     const PeripheralDispatchOps *ops, void *dac_handle)
{
    (void)dac_handle;

    if (dispatch == NULL || ops == NULL || ops->task_notify_from_isr == NULL)
    {
        return;
    }

    ops->task_notify_from_isr(dispatch->effects_task_handle, (uint32_t)(uintptr_t)dispatch->dac_buf_a,
                              ops->context);
}

void peripheral_on_dac_complete(const PeripheralDispatchContext *dispatch,
                                const PeripheralDispatchOps *ops, void *dac_handle)
{
    (void)dac_handle;

    if (dispatch == NULL || ops == NULL || ops->task_notify_from_isr == NULL)
    {
        return;
    }

    ops->task_notify_from_isr(dispatch->effects_task_handle, (uint32_t)(uintptr_t)dispatch->dac_buf_b,
                              ops->context);
}

void peripheral_on_i2c_tx_complete(const PeripheralDispatchContext *dispatch,
                                   const PeripheralDispatchOps *ops, void *i2c_handle)
{
    if (dispatch == NULL || ops == NULL || ops->i2c_signal_completion_from_isr == NULL)
    {
        return;
    }

    if (i2c_handle == dispatch->i2c_handle)
    {
        ops->i2c_signal_completion_from_isr(dispatch->i2c_task_support_context, false, ops->context);
    }
}

void peripheral_on_i2c_rx_complete(const PeripheralDispatchContext *dispatch,
                                   const PeripheralDispatchOps *ops, void *i2c_handle)
{
    if (dispatch == NULL || ops == NULL || ops->i2c_signal_completion_from_isr == NULL)
    {
        return;
    }

    if (i2c_handle == dispatch->i2c_handle)
    {
        ops->i2c_signal_completion_from_isr(dispatch->i2c_task_support_context, false, ops->context);
    }
}

void peripheral_on_i2c_error(const PeripheralDispatchContext *dispatch,
                             const PeripheralDispatchOps *ops, void *i2c_handle)
{
    if (dispatch == NULL || ops == NULL || ops->i2c_signal_completion_from_isr == NULL)
    {
        return;
    }

    if (i2c_handle == dispatch->i2c_handle)
    {
        (void)log_write(LOG_LEVEL_WARN, "i2c error callback");
        ops->i2c_signal_completion_from_isr(dispatch->i2c_task_support_context, true, ops->context);
    }
}

void peripheral_on_i2c_abort_complete(const PeripheralDispatchContext *dispatch,
                                      const PeripheralDispatchOps *ops, void *i2c_handle)
{
    if (dispatch == NULL || ops == NULL || ops->i2c_signal_completion_from_isr == NULL)
    {
        return;
    }

    if (i2c_handle == dispatch->i2c_handle)
    {
        (void)log_write(LOG_LEVEL_WARN, "i2c abort callback");
        ops->i2c_signal_completion_from_isr(dispatch->i2c_task_support_context, true, ops->context);
    }
}

void peripheral_on_dma_complete(const PeripheralDispatchContext *dispatch,
                                const PeripheralDispatchOps *ops, void *dma_handle)
{
    if (dispatch == NULL || ops == NULL || ops->semaphore_give_from_isr == NULL)
    {
        return;
    }

    if (dma_handle == dispatch->mem_to_mem_dma_handle)
    {
        ops->semaphore_give_from_isr(dispatch->delay_samples_dma_semaphore_handle, ops->context);
    }
}
