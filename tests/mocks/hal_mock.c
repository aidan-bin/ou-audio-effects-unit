#include "hal_mock.h"

#include <string.h>

HalMockState g_halMockState;

static size_t cap_copy_size(size_t size) {
    return size < HAL_MOCK_CAPTURE_BYTES ? size : HAL_MOCK_CAPTURE_BYTES;
}

void hal_mock_reset(void) {
    memset(&g_halMockState, 0, sizeof(g_halMockState));

    g_halMockState.gpio.nextState = HAL_MOCK_GPIO_RESET;

    g_halMockState.i2c.nextReadyStatus = HAL_MOCK_STATUS_OK;
    g_halMockState.i2c.nextTxStatus = HAL_MOCK_STATUS_OK;
    g_halMockState.i2c.nextRxStatus = HAL_MOCK_STATUS_OK;

    g_halMockState.adc.nextStartStatus = HAL_MOCK_STATUS_OK;
    g_halMockState.adc.nextStopStatus = HAL_MOCK_STATUS_OK;

    g_halMockState.dac.nextStartStatus = HAL_MOCK_STATUS_OK;
    g_halMockState.dac.nextStopStatus = HAL_MOCK_STATUS_OK;
    g_halMockState.dac.nextSetValueStatus = HAL_MOCK_STATUS_OK;
}

void hal_mock_set_i2c_callbacks(HalMockI2CCallbacks callbacks) {
    g_halMockState.i2c.callbacks = callbacks;
}

HalMockGpioState hal_mock_gpio_read_pin(uint16_t pin) {
    g_halMockState.gpio.readCount++;
    g_halMockState.gpio.lastPin = pin;
    return g_halMockState.gpio.nextState;
}

HalMockStatus hal_mock_i2c_is_device_ready(uint16_t address, uint32_t trials, uint32_t timeoutMs) {
    g_halMockState.i2c.isReadyCount++;
    g_halMockState.i2c.lastReadyAddress = address;
    g_halMockState.i2c.lastReadyTrials = trials;
    g_halMockState.i2c.lastReadyTimeoutMs = timeoutMs;
    return g_halMockState.i2c.nextReadyStatus;
}

HalMockStatus hal_mock_i2c_master_transmit_dma(uint16_t address, const uint8_t *data, size_t size) {
    g_halMockState.i2c.txCount++;
    g_halMockState.i2c.lastTxAddress = address;
    g_halMockState.i2c.lastTxSize = size;

    size_t copySize = cap_copy_size(size);
    if (copySize > 0 && data != NULL) {
        memcpy(g_halMockState.i2c.lastTxBytes, data, copySize);
    }

    HalMockStatus status = g_halMockState.i2c.nextTxStatus;

    if (status == HAL_MOCK_STATUS_OK && g_halMockState.i2c.autoInvokeTxComplete &&
        g_halMockState.i2c.callbacks.txComplete != NULL) {
        g_halMockState.i2c.callbacks.txComplete(g_halMockState.i2c.callbacks.context);
    }

    if (status != HAL_MOCK_STATUS_OK && g_halMockState.i2c.autoInvokeError &&
        g_halMockState.i2c.callbacks.error != NULL) {
        g_halMockState.i2c.callbacks.error(g_halMockState.i2c.callbacks.context);
    }

    return status;
}

HalMockStatus hal_mock_i2c_master_receive_dma(uint16_t address, uint8_t *data, size_t size) {
    g_halMockState.i2c.rxCount++;
    g_halMockState.i2c.lastRxAddress = address;
    g_halMockState.i2c.lastRxSize = size;

    size_t copySize = cap_copy_size(size);
    if (copySize > 0 && data != NULL) {
        memcpy(data, g_halMockState.i2c.nextRxBytes, copySize);
    }

    HalMockStatus status = g_halMockState.i2c.nextRxStatus;

    if (status == HAL_MOCK_STATUS_OK && g_halMockState.i2c.autoInvokeRxComplete &&
        g_halMockState.i2c.callbacks.rxComplete != NULL) {
        g_halMockState.i2c.callbacks.rxComplete(g_halMockState.i2c.callbacks.context);
    }

    if (status != HAL_MOCK_STATUS_OK && g_halMockState.i2c.autoInvokeError &&
        g_halMockState.i2c.callbacks.error != NULL) {
        g_halMockState.i2c.callbacks.error(g_halMockState.i2c.callbacks.context);
    }

    return status;
}

HalMockStatus hal_mock_adc_start_dma(uint16_t *buffer, size_t length) {
    g_halMockState.adc.startDmaCount++;
    g_halMockState.adc.lastStartBuffer = buffer;
    g_halMockState.adc.lastStartLength = length;
    return g_halMockState.adc.nextStartStatus;
}

HalMockStatus hal_mock_adc_stop_dma(void) {
    g_halMockState.adc.stopDmaCount++;
    return g_halMockState.adc.nextStopStatus;
}

HalMockStatus hal_mock_dac_start_dma(const uint16_t *buffer, size_t length) {
    g_halMockState.dac.startDmaCount++;
    g_halMockState.dac.lastStartBuffer = buffer;
    g_halMockState.dac.lastStartLength = length;
    return g_halMockState.dac.nextStartStatus;
}

HalMockStatus hal_mock_dac_stop_dma(void) {
    g_halMockState.dac.stopDmaCount++;
    return g_halMockState.dac.nextStopStatus;
}

HalMockStatus hal_mock_dac_set_value(uint16_t value) {
    g_halMockState.dac.setValueCount++;
    g_halMockState.dac.lastValue = value;
    return g_halMockState.dac.nextSetValueStatus;
}
