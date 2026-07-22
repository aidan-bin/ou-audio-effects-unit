/*
 * peripheral_dispatch — dependency-injection layer for ISR/DMA/ADC; converts hardware events into app-level notifications.
 */

#ifndef PERIPHERAL_DISPATCH_H
#define PERIPHERAL_DISPATCH_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    void *effects_task_handle;
    void *pot_handler_task_handle;
    void *btn_handler_task_handle;

    void *effects_adc_handle;
    void *pot_adc_handle;
    void *i2c_handle;
    void *mem_to_mem_dma_handle;

    void *i2c_task_support_context;
    void *delay_samples_dma_semaphore_handle;

    void *adc_buf_a;
    void *adc_buf_b;
    void *dac_buf_a;
    void *dac_buf_b;
} PeripheralDispatchContext;

typedef struct
{
    bool (*semaphore_take)(void *semaphore_handle, uint32_t timeout_ticks, void *context);
    void (*task_notify_from_isr)(void *task_handle, uint32_t value, void *context);
    void (*semaphore_give_from_isr)(void *semaphore_handle, void *context);

    bool (*dma_start_it)(void *dma_handle, uint32_t src_address, uint32_t dst_address,
                         uint32_t data_length, void *context);

    bool (*adc_config_channel)(void *adc_handle, void *config, void *context);
    bool (*adc_start_it)(void *adc_handle, void *context);
    uint32_t (*adc_get_value)(void *adc_handle, void *context);

    void (*i2c_signal_completion_from_isr)(void *i2c_task_support_context, bool transfer_failed,
                                           void *context);

    void *context;
} PeripheralDispatchOps;

void peripheral_drain_semaphore(void *semaphore_handle, const PeripheralDispatchOps *ops);

bool peripheral_start_dma_transfer_and_wait(void *dma_handle, uint32_t src_address,
                                            uint32_t dst_address, uint32_t data_length,
                                            void *completion_semaphore_handle, uint32_t ticks_to_wait,
                                            const PeripheralDispatchOps *ops);

bool peripheral_start_adc_conversion_it(void *adc_handle, void *config,
                                        const PeripheralDispatchOps *ops);

void peripheral_on_gpio_exti(const PeripheralDispatchContext *dispatch,
                             const PeripheralDispatchOps *ops, uint16_t gpio_pin);

void peripheral_on_adc_half_complete(const PeripheralDispatchContext *dispatch,
                                     const PeripheralDispatchOps *ops, void *adc_handle);

void peripheral_on_adc_complete(const PeripheralDispatchContext *dispatch,
                                const PeripheralDispatchOps *ops, void *adc_handle);

void peripheral_on_dac_half_complete(const PeripheralDispatchContext *dispatch,
                                     const PeripheralDispatchOps *ops, void *dac_handle);

void peripheral_on_dac_complete(const PeripheralDispatchContext *dispatch,
                                const PeripheralDispatchOps *ops, void *dac_handle);

void peripheral_on_i2c_tx_complete(const PeripheralDispatchContext *dispatch,
                                   const PeripheralDispatchOps *ops, void *i2c_handle);

void peripheral_on_i2c_rx_complete(const PeripheralDispatchContext *dispatch,
                                   const PeripheralDispatchOps *ops, void *i2c_handle);

void peripheral_on_i2c_error(const PeripheralDispatchContext *dispatch,
                             const PeripheralDispatchOps *ops, void *i2c_handle);

void peripheral_on_i2c_abort_complete(const PeripheralDispatchContext *dispatch,
                                      const PeripheralDispatchOps *ops, void *i2c_handle);

void peripheral_on_dma_complete(const PeripheralDispatchContext *dispatch,
                                const PeripheralDispatchOps *ops, void *dma_handle);

#endif