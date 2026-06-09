#include "hal_mock.h"

#include <string.h>

HalMockState g_hal_mock_state;

static size_t cap_copy_size(size_t size)
{
    return size < HAL_MOCK_CAPTURE_BYTES ? size : HAL_MOCK_CAPTURE_BYTES;
}

void hal_mock_reset(void)
{
    memset(&g_hal_mock_state, 0, sizeof(g_hal_mock_state));

    g_hal_mock_state.gpio.next_state = HAL_MOCK_GPIO_RESET;

    g_hal_mock_state.i2c.next_ready_status = HAL_MOCK_STATUS_OK;
    g_hal_mock_state.i2c.next_tx_status = HAL_MOCK_STATUS_OK;
    g_hal_mock_state.i2c.next_rx_status = HAL_MOCK_STATUS_OK;

    g_hal_mock_state.adc.next_start_status = HAL_MOCK_STATUS_OK;
    g_hal_mock_state.adc.next_stop_status = HAL_MOCK_STATUS_OK;

    g_hal_mock_state.dac.next_start_status = HAL_MOCK_STATUS_OK;
    g_hal_mock_state.dac.next_stop_status = HAL_MOCK_STATUS_OK;
    g_hal_mock_state.dac.next_set_value_status = HAL_MOCK_STATUS_OK;
}

void hal_mock_set_i2c_callbacks(HalMockI2CCallbacks callbacks)
{
    g_hal_mock_state.i2c.callbacks = callbacks;
}

HalMockGpioState hal_mock_gpio_read_pin(uint16_t pin)
{
    g_hal_mock_state.gpio.read_count++;
    g_hal_mock_state.gpio.last_pin = pin;
    return g_hal_mock_state.gpio.next_state;
}

HalMockStatus hal_mock_i2c_is_device_ready(uint16_t address, uint32_t trials, uint32_t timeout_ms)
{
    g_hal_mock_state.i2c.is_ready_count++;
    g_hal_mock_state.i2c.last_ready_address = address;
    g_hal_mock_state.i2c.last_ready_trials = trials;
    g_hal_mock_state.i2c.last_ready_timeout_ms = timeout_ms;
    return g_hal_mock_state.i2c.next_ready_status;
}

HalMockStatus hal_mock_i2c_master_transmit_dma(uint16_t address, const uint8_t *data, size_t size)
{
    g_hal_mock_state.i2c.tx_count++;
    g_hal_mock_state.i2c.last_tx_address = address;
    g_hal_mock_state.i2c.last_tx_size = size;

    size_t copy_size = cap_copy_size(size);
    if (copy_size > 0 && data != NULL)
    {
        memcpy(g_hal_mock_state.i2c.last_tx_bytes, data, copy_size);
    }

    HalMockStatus status = g_hal_mock_state.i2c.next_tx_status;

    if (status == HAL_MOCK_STATUS_OK && g_hal_mock_state.i2c.auto_invoke_tx_complete &&
        g_hal_mock_state.i2c.callbacks.tx_complete != NULL)
    {
        g_hal_mock_state.i2c.callbacks.tx_complete(g_hal_mock_state.i2c.callbacks.context);
    }

    if (status != HAL_MOCK_STATUS_OK && g_hal_mock_state.i2c.auto_invoke_error &&
        g_hal_mock_state.i2c.callbacks.error != NULL)
    {
        g_hal_mock_state.i2c.callbacks.error(g_hal_mock_state.i2c.callbacks.context);
    }

    return status;
}

HalMockStatus hal_mock_i2c_master_receive_dma(uint16_t address, uint8_t *data, size_t size)
{
    g_hal_mock_state.i2c.rx_count++;
    g_hal_mock_state.i2c.last_rx_address = address;
    g_hal_mock_state.i2c.last_rx_size = size;

    size_t copy_size = cap_copy_size(size);
    if (copy_size > 0 && data != NULL)
    {
        memcpy(data, g_hal_mock_state.i2c.next_rx_bytes, copy_size);
    }

    HalMockStatus status = g_hal_mock_state.i2c.next_rx_status;

    if (status == HAL_MOCK_STATUS_OK && g_hal_mock_state.i2c.auto_invoke_rx_complete &&
        g_hal_mock_state.i2c.callbacks.rx_complete != NULL)
    {
        g_hal_mock_state.i2c.callbacks.rx_complete(g_hal_mock_state.i2c.callbacks.context);
    }

    if (status != HAL_MOCK_STATUS_OK && g_hal_mock_state.i2c.auto_invoke_error &&
        g_hal_mock_state.i2c.callbacks.error != NULL)
    {
        g_hal_mock_state.i2c.callbacks.error(g_hal_mock_state.i2c.callbacks.context);
    }

    return status;
}

HalMockStatus hal_mock_adc_start_dma(uint16_t *buffer, size_t length)
{
    g_hal_mock_state.adc.start_dma_count++;
    g_hal_mock_state.adc.last_start_buffer = buffer;
    g_hal_mock_state.adc.last_start_length = length;
    return g_hal_mock_state.adc.next_start_status;
}

HalMockStatus hal_mock_adc_stop_dma(void)
{
    g_hal_mock_state.adc.stop_dma_count++;
    return g_hal_mock_state.adc.next_stop_status;
}

HalMockStatus hal_mock_dac_start_dma(const uint16_t *buffer, size_t length)
{
    g_hal_mock_state.dac.start_dma_count++;
    g_hal_mock_state.dac.last_start_buffer = buffer;
    g_hal_mock_state.dac.last_start_length = length;
    return g_hal_mock_state.dac.next_start_status;
}

HalMockStatus hal_mock_dac_stop_dma(void)
{
    g_hal_mock_state.dac.stop_dma_count++;
    return g_hal_mock_state.dac.next_stop_status;
}

HalMockStatus hal_mock_dac_set_value(uint16_t value)
{
    g_hal_mock_state.dac.set_value_count++;
    g_hal_mock_state.dac.last_value = value;
    return g_hal_mock_state.dac.next_set_value_status;
}
