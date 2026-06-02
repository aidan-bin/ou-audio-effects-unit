#ifndef HAL_MOCK_H
#define HAL_MOCK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define HAL_MOCK_CAPTURE_BYTES 64

typedef enum {
    HAL_MOCK_STATUS_OK = 0,
    HAL_MOCK_STATUS_ERROR = 1,
    HAL_MOCK_STATUS_BUSY = 2,
    HAL_MOCK_STATUS_TIMEOUT = 3,
} HalMockStatus;

typedef enum {
    HAL_MOCK_GPIO_RESET = 0,
    HAL_MOCK_GPIO_SET = 1,
} HalMockGpioState;

typedef void (*HalMockI2CTxCompleteCallback)(void *context);
typedef void (*HalMockI2CRxCompleteCallback)(void *context);
typedef void (*HalMockI2CErrorCallback)(void *context);

typedef struct {
    HalMockI2CTxCompleteCallback txComplete;
    HalMockI2CRxCompleteCallback rxComplete;
    HalMockI2CErrorCallback error;
    void *context;
} HalMockI2CCallbacks;

typedef struct {
    uint32_t readCount;
    uint16_t lastPin;
    HalMockGpioState nextState;
} HalMockGpioReadState;

typedef struct {
    uint32_t isReadyCount;
    uint32_t txCount;
    uint32_t rxCount;

    uint16_t lastReadyAddress;
    uint32_t lastReadyTrials;
    uint32_t lastReadyTimeoutMs;

    uint16_t lastTxAddress;
    size_t lastTxSize;
    uint8_t lastTxBytes[HAL_MOCK_CAPTURE_BYTES];

    uint16_t lastRxAddress;
    size_t lastRxSize;
    uint8_t nextRxBytes[HAL_MOCK_CAPTURE_BYTES];

    HalMockStatus nextReadyStatus;
    HalMockStatus nextTxStatus;
    HalMockStatus nextRxStatus;

    bool autoInvokeTxComplete;
    bool autoInvokeRxComplete;
    bool autoInvokeError;

    HalMockI2CCallbacks callbacks;
} HalMockI2CState;

typedef struct {
    uint32_t startDmaCount;
    uint32_t stopDmaCount;
    size_t lastStartLength;
    uint16_t *lastStartBuffer;
    HalMockStatus nextStartStatus;
    HalMockStatus nextStopStatus;
} HalMockAdcState;

typedef struct {
    uint32_t startDmaCount;
    uint32_t stopDmaCount;
    uint32_t setValueCount;

    size_t lastStartLength;
    const uint16_t *lastStartBuffer;
    uint16_t lastValue;

    HalMockStatus nextStartStatus;
    HalMockStatus nextStopStatus;
    HalMockStatus nextSetValueStatus;
} HalMockDacState;

typedef struct {
    HalMockGpioReadState gpio;
    HalMockI2CState i2c;
    HalMockAdcState adc;
    HalMockDacState dac;
} HalMockState;

extern HalMockState g_halMockState;

void hal_mock_reset(void);
void hal_mock_set_i2c_callbacks(HalMockI2CCallbacks callbacks);

HalMockGpioState hal_mock_gpio_read_pin(uint16_t pin);

HalMockStatus hal_mock_i2c_is_device_ready(uint16_t address, uint32_t trials, uint32_t timeoutMs);
HalMockStatus hal_mock_i2c_master_transmit_dma(uint16_t address, const uint8_t *data, size_t size);
HalMockStatus hal_mock_i2c_master_receive_dma(uint16_t address, uint8_t *data, size_t size);

HalMockStatus hal_mock_adc_start_dma(uint16_t *buffer, size_t length);
HalMockStatus hal_mock_adc_stop_dma(void);

HalMockStatus hal_mock_dac_start_dma(const uint16_t *buffer, size_t length);
HalMockStatus hal_mock_dac_stop_dma(void);
HalMockStatus hal_mock_dac_set_value(uint16_t value);

#endif
