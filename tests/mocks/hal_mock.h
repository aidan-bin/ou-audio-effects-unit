#ifndef HAL_MOCK_H
#define HAL_MOCK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define HAL_MOCK_CAPTURE_BYTES 64

typedef enum
{
    HAL_MOCK_STATUS_OK = 0,
    HAL_MOCK_STATUS_ERROR = 1,
    HAL_MOCK_STATUS_BUSY = 2,
    HAL_MOCK_STATUS_TIMEOUT = 3,
} HalMockStatus;

typedef enum
{
    HAL_MOCK_GPIO_RESET = 0,
    HAL_MOCK_GPIO_SET = 1,
} HalMockGpioState;

typedef void (*HalMockI2CTxCompleteCallback)(void *context);
typedef void (*HalMockI2CRxCompleteCallback)(void *context);
typedef void (*HalMockI2CErrorCallback)(void *context);

typedef struct
{
    HalMockI2CTxCompleteCallback tx_complete;
    HalMockI2CRxCompleteCallback rx_complete;
    HalMockI2CErrorCallback error;
    void *context;
} HalMockI2CCallbacks;

typedef struct
{
    uint32_t read_count;
    uint16_t last_pin;
    HalMockGpioState next_state;
} HalMockGpioReadState;

typedef struct
{
    uint32_t is_ready_count;
    uint32_t tx_count;
    uint32_t rx_count;

    uint16_t last_ready_address;
    uint32_t last_ready_trials;
    uint32_t last_ready_timeout_ms;

    uint16_t last_tx_address;
    size_t last_tx_size;
    uint8_t last_tx_bytes[HAL_MOCK_CAPTURE_BYTES];

    uint16_t last_rx_address;
    size_t last_rx_size;
    uint8_t next_rx_bytes[HAL_MOCK_CAPTURE_BYTES];

    HalMockStatus next_ready_status;
    HalMockStatus next_tx_status;
    HalMockStatus next_rx_status;

    bool auto_invoke_tx_complete;
    bool auto_invoke_rx_complete;
    bool auto_invoke_error;

    HalMockI2CCallbacks callbacks;
} HalMockI2CState;

typedef struct
{
    uint32_t start_dma_count;
    uint32_t stop_dma_count;
    size_t last_start_length;
    uint16_t *last_start_buffer;
    HalMockStatus next_start_status;
    HalMockStatus next_stop_status;
} HalMockAdcState;

typedef struct
{
    uint32_t start_dma_count;
    uint32_t stop_dma_count;
    uint32_t set_value_count;

    size_t last_start_length;
    const uint16_t *last_start_buffer;
    uint16_t last_value;

    HalMockStatus next_start_status;
    HalMockStatus next_stop_status;
    HalMockStatus next_set_value_status;
} HalMockDacState;

typedef struct
{
    HalMockGpioReadState gpio;
    HalMockI2CState i2c;
    HalMockAdcState adc;
    HalMockDacState dac;
} HalMockState;

extern HalMockState g_hal_mock_state;

void hal_mock_reset(void);
void hal_mock_set_i2c_callbacks(HalMockI2CCallbacks callbacks);

HalMockGpioState hal_mock_gpio_read_pin(uint16_t pin);

HalMockStatus hal_mock_i2c_is_device_ready(uint16_t address, uint32_t trials, uint32_t timeout_ms);
HalMockStatus hal_mock_i2c_master_transmit_dma(uint16_t address, const uint8_t *data, size_t size);
HalMockStatus hal_mock_i2c_master_receive_dma(uint16_t address, uint8_t *data, size_t size);

HalMockStatus hal_mock_adc_start_dma(uint16_t *buffer, size_t length);
HalMockStatus hal_mock_adc_stop_dma(void);

HalMockStatus hal_mock_dac_start_dma(const uint16_t *buffer, size_t length);
HalMockStatus hal_mock_dac_stop_dma(void);
HalMockStatus hal_mock_dac_set_value(uint16_t value);

#endif
