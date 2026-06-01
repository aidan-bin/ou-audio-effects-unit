#include "host_harness.h"

#include <stddef.h>
#include <string.h>

void harness_init(HostHarness *harness)
{
    if (harness == NULL)
    {
        return;
    }

    memset(harness, 0, sizeof(*harness));

    harness->hal = &g_halMockState;
    harness->rtos = &g_freeRtosMockState;

    harness->effectsTaskHandle = (void *)0xE001;
    harness->potTaskHandle = (void *)0xE002;
    harness->switchTaskHandle = (void *)0xE003;
    harness->buttonTaskHandle = (void *)0xE004;
    harness->i2cTaskHandle = (void *)0xE005;
    harness->romTaskHandle = (void *)0xE006;

    harness->adcBufA = (void *)0xA001;
    harness->adcBufB = (void *)0xA002;
    harness->dacBufA = (void *)0xD001;
    harness->dacBufB = (void *)0xD002;

    harness_reset(harness);
}

void harness_reset(HostHarness *harness)
{
    if (harness == NULL)
    {
        return;
    }

    hal_mock_reset();
    freertos_mock_reset();

    harness->ticks = 0;
    memset(harness->potSamples, 0, sizeof(harness->potSamples));
    memset(harness->switches, 0, sizeof(harness->switches));

    harness->lastButtonPin = 0;
    harness->hasButtonEvent = false;

    harness->hasPendingAdcBuffer = false;
    harness->pendingAdcBufferIsA = false;

    harness->hasPendingDacBuffer = false;
    harness->pendingDacBufferIsA = false;

    harness->i2cCompletionReady = false;
    harness->i2cCompletionFailed = false;
    harness->dmaCompletionReady = false;

    harness->outstandingAllocs = 0;
}

void harness_advance_ticks(HostHarness *harness, uint32_t ticks)
{
    if (harness == NULL)
    {
        return;
    }

    harness->ticks += ticks;
}

void harness_post_adc_buffer(HostHarness *harness, bool useBufferA)
{
    if (harness == NULL)
    {
        return;
    }

    harness->hasPendingAdcBuffer = true;
    harness->pendingAdcBufferIsA = useBufferA;
}

void harness_post_dac_buffer(HostHarness *harness, bool useBufferA)
{
    if (harness == NULL)
    {
        return;
    }

    harness->hasPendingDacBuffer = true;
    harness->pendingDacBufferIsA = useBufferA;
}

void harness_set_pot_sample(HostHarness *harness, uint8_t channel, uint16_t value)
{
    if (harness == NULL || channel >= 4)
    {
        return;
    }

    harness->potSamples[channel] = value;
}

void harness_set_switch(HostHarness *harness, uint8_t index, bool state)
{
    if (harness == NULL || index >= 3)
    {
        return;
    }

    harness->switches[index] = state;
}

void harness_press_button(HostHarness *harness, uint16_t pin)
{
    if (harness == NULL)
    {
        return;
    }

    harness->lastButtonPin = pin;
    harness->hasButtonEvent = true;
}

void harness_complete_i2c(HostHarness *harness, bool success)
{
    if (harness == NULL)
    {
        return;
    }

    harness->i2cCompletionReady = true;
    harness->i2cCompletionFailed = !success;
}

void harness_complete_dma(HostHarness *harness)
{
    if (harness == NULL)
    {
        return;
    }

    harness->dmaCompletionReady = true;
}
