#ifndef PERIPHERAL_DISPATCH_H
#define PERIPHERAL_DISPATCH_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    void *effectsTaskHandle;
    void *potHandlerTaskHandle;
    void *btnHandlerTaskHandle;

    void *effectsAdcHandle;
    void *potAdcHandle;
    void *i2cHandle;
    void *memToMemDmaHandle;

    void *i2cTaskSupportContext;
    void *delaySamplesDmaSemaphoreHandle;

    void *adcBufA;
    void *adcBufB;
    void *dacBufA;
    void *dacBufB;
} PeripheralDispatchContext;

typedef struct
{
    bool (*semaphore_take)(void *semaphoreHandle, uint32_t timeoutTicks, void *context);
    void (*task_notify_from_isr)(void *taskHandle, uint32_t value, void *context);
    void (*semaphore_give_from_isr)(void *semaphoreHandle, void *context);

    bool (*dma_start_it)(void *dmaHandle, uint32_t srcAddress, uint32_t dstAddress,
                         uint32_t dataLength, void *context);

    bool (*adc_config_channel)(void *adcHandle, void *config, void *context);
    bool (*adc_start_it)(void *adcHandle, void *context);
    uint32_t (*adc_get_value)(void *adcHandle, void *context);

    void (*i2c_signal_completion_from_isr)(void *i2cTaskSupportContext, bool transferFailed,
                                           void *context);

    void *context;
} PeripheralDispatchOps;

void peripheral_drain_semaphore(void *semaphoreHandle, const PeripheralDispatchOps *ops);

bool peripheral_start_dma_transfer_and_wait(void *dmaHandle, uint32_t srcAddress,
                                            uint32_t dstAddress, uint32_t dataLength,
                                            void *completionSemaphoreHandle, uint32_t ticksToWait,
                                            const PeripheralDispatchOps *ops);

bool peripheral_start_adc_conversion_it(void *adcHandle, void *config,
                                        const PeripheralDispatchOps *ops);

void peripheral_on_gpio_exti(const PeripheralDispatchContext *dispatch,
                             const PeripheralDispatchOps *ops, uint16_t gpioPin);

void peripheral_on_adc_half_complete(const PeripheralDispatchContext *dispatch,
                                     const PeripheralDispatchOps *ops, void *adcHandle);

void peripheral_on_adc_complete(const PeripheralDispatchContext *dispatch,
                                const PeripheralDispatchOps *ops, void *adcHandle);

void peripheral_on_dac_half_complete(const PeripheralDispatchContext *dispatch,
                                     const PeripheralDispatchOps *ops, void *dacHandle);

void peripheral_on_dac_complete(const PeripheralDispatchContext *dispatch,
                                const PeripheralDispatchOps *ops, void *dacHandle);

void peripheral_on_i2c_tx_complete(const PeripheralDispatchContext *dispatch,
                                   const PeripheralDispatchOps *ops, void *i2cHandle);

void peripheral_on_i2c_rx_complete(const PeripheralDispatchContext *dispatch,
                                   const PeripheralDispatchOps *ops, void *i2cHandle);

void peripheral_on_i2c_error(const PeripheralDispatchContext *dispatch,
                             const PeripheralDispatchOps *ops, void *i2cHandle);

void peripheral_on_i2c_abort_complete(const PeripheralDispatchContext *dispatch,
                                      const PeripheralDispatchOps *ops, void *i2cHandle);

void peripheral_on_dma_complete(const PeripheralDispatchContext *dispatch,
                                const PeripheralDispatchOps *ops, void *dmaHandle);

#endif