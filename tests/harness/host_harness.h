#ifndef HOST_HARNESS_H
#define HOST_HARNESS_H

#include <stdbool.h>
#include <stdint.h>

#include "mocks/freertos_mock.h"
#include "mocks/hal_mock.h"

typedef struct
{
    HalMockState *hal;
    FreeRtosMockState *rtos;

    void *effectsTaskHandle;
    void *potTaskHandle;
    void *switchTaskHandle;
    void *buttonTaskHandle;
    void *i2cTaskHandle;
    void *romTaskHandle;

    void *adcBufA;
    void *adcBufB;
    void *dacBufA;
    void *dacBufB;

    uint32_t ticks;
    uint16_t potSamples[4];
    bool switches[3];

    uint16_t lastButtonPin;
    bool hasButtonEvent;

    bool hasPendingAdcBuffer;
    bool pendingAdcBufferIsA;

    bool hasPendingDacBuffer;
    bool pendingDacBufferIsA;

    bool i2cCompletionReady;
    bool i2cCompletionFailed;
    bool dmaCompletionReady;

    uint32_t outstandingAllocs;
} HostHarness;

void harness_init(HostHarness *harness);
void harness_reset(HostHarness *harness);

void harness_advance_ticks(HostHarness *harness, uint32_t ticks);

void harness_post_adc_buffer(HostHarness *harness, bool useBufferA);
void harness_post_dac_buffer(HostHarness *harness, bool useBufferA);

void harness_set_pot_sample(HostHarness *harness, uint8_t channel, uint16_t value);
void harness_set_switch(HostHarness *harness, uint8_t index, bool state);
void harness_press_button(HostHarness *harness, uint16_t pin);

void harness_complete_i2c(HostHarness *harness, bool success);
void harness_complete_dma(HostHarness *harness);

#endif