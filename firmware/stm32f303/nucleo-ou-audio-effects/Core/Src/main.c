/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"
#include "usb_device.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"
#include "cli_session.h"
#include "cli_service_adapter.h"
#include "effects_model.h"
#include "effects_pipeline.h"
#include "effects_task.h"
#include "test_vector_source.h"
#include "usb/audio_stream.h"
#include "usb/cdc_cli.h"
#include "usb/uac.h"

#include "i2c_handler.h"
#include "i2c_task_support.h"
#include "peripheral_dispatch.h"
#include "rom_handler.h"
#include "rom_task_support.h"
#include "usbd_cdc_if.h"

#include "sample_convert.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define SAMPLE_BUF_LEN 480U
#define ADC_BUF_LEN (2U * SAMPLE_BUF_LEN)
#define DAC_BUF_LEN ADC_BUF_LEN
#define NUM_DELAY_SAMPLES MAX_ECHO_DELAY_SAMPLES
#define HEARTBEAT_PERIOD_MS_DEFAULT 500U
#define HEARTBEAT_PERIOD_MS_MIN 50U
#define HEARTBEAT_PERIOD_MS_MAX 5000U
#define EFFECTS_RUNTIME_LOG_EVERY_FAILURES 10U
#define CLI_USB_RX_BYTE_QUEUE_CAPACITY 256U

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

#ifdef ENABLE_DIAGNOSTICS
#define PANIC_USB_STR_(x) #x
#define PANIC_USB_STR(x) PANIC_USB_STR_(x)
#define PANIC_USB(msg) \
    effects_task_panic_write("[usb] " __FILE__ ":" PANIC_USB_STR(__LINE__) " " msg "\n", NULL)
#else
#define PANIC_USB(msg) ((void)0)
#endif

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc3;
DMA_HandleTypeDef hdma_adc3;

DAC_HandleTypeDef hdac1;
DMA_HandleTypeDef hdma_dac1_ch1;

I2C_HandleTypeDef hi2c1;

OPAMP_HandleTypeDef hopamp2;
OPAMP_HandleTypeDef hopamp3;

TIM_HandleTypeDef htim2;

UART_HandleTypeDef huart2;

osThreadId defaultTaskHandle;
/* USER CODE BEGIN PV */

osThreadId effectsTaskHandle;
osThreadId cliTaskHandle;

static volatile EffectsState effects_state;
static volatile EffectsParams effects_params;

static volatile uint16_t adc_buf[ADC_BUF_LEN] = {0};
static volatile uint16_t dac_buf[DAC_BUF_LEN] = {0};

/* Feedback for echo; stored in CCM RAM since DMA is not needed. */
#define ECHO_HISTORY_LEN (NUM_DELAY_SAMPLES + SAMPLE_BUF_LEN)
static uint16_t echo_history_buf[ECHO_HISTORY_LEN];
__attribute__((section(".ccmram"))) static uint16_t echo_fb_line[MAX_ECHO_FEEDBACK_DELAY];
static EchoState echo_state = {
    .history = echo_history_buf,
    .history_len = ECHO_HISTORY_LEN,
    .history_w = 0,
    .fb =
        {
            .line = echo_fb_line,
            .line_len = MAX_ECHO_FEEDBACK_DELAY,
            .write_idx = 0,
            .lpf_state = 0,
        },
    .prev_enabled = 0,
};

static volatile uint16_t *const adc_buf_a = adc_buf;
static volatile uint16_t *const adc_buf_b = &adc_buf[SAMPLE_BUF_LEN];
static volatile uint16_t *const dac_buf_a = dac_buf;
static volatile uint16_t *const dac_buf_b = &dac_buf[SAMPLE_BUF_LEN];
static volatile uint32_t effects_event_count_adc_a = 0U;
static volatile uint32_t effects_event_count_adc_b = 0U;
static volatile uint32_t effects_event_count_dac_a = 0U;
static volatile uint32_t effects_event_count_dac_b = 0U;
/* Cumulative ADC/DAC DMA-callback totals; never decremented, so they can be
 * sampled over a fixed window to measure event rate. */
static volatile uint32_t effects_event_cum_adc = 0U;
static volatile uint32_t effects_event_cum_dac = 0U;
static volatile uint32_t heartbeat_period_ms = HEARTBEAT_PERIOD_MS_DEFAULT;
static uint32_t effects_frames_ok = 0U;
static uint32_t effects_frames_failed = 0U;

static CliServiceAdapter cli_service_adapter_context;
static CliServices cli_services;
static CliSession cli_session;
static TestVectorSource test_vector_source;
static TestVectorSource test_vector_source_output;
static UsbAudioStream audio_stream;

static volatile bool b1_pressed = false;

static StaticQueue_t i2c_queue_control_block;
static uint8_t i2c_queue_buffer[16 * sizeof(I2CHandlerMessage)];
static QueueHandle_t i2cQueueHandle = NULL;

static StaticSemaphore_t i2c_completion_semaphore_buffer;
static SemaphoreHandle_t i2c_completion_semaphore = NULL;
static StaticSemaphore_t i2c_failed_rom_semaphore_buffer;
static SemaphoreHandle_t i2c_failed_rom_semaphore = NULL;
static StaticSemaphore_t i2c_failed_display_semaphore_buffer;
static SemaphoreHandle_t i2c_failed_display_semaphore = NULL;

static osThreadId i2cHandlerTaskHandle;
static osThreadId romHandlerTaskHandle;

static volatile bool i2c_transfer_failed = true;

static I2CTaskSupportContext i2c_task_support_context;
static I2CHandlerConfig i2c_handler_config;
static I2CHandlerOps i2c_handler_ops;

static RomTaskSupportConfig rom_task_support_config;
static RomTaskSupportOps rom_task_support_ops;

static bool i2c_rom_detected = false;

static SemaphoreHandle_t i2c_mutex = NULL;

static PeripheralDispatchContext peripheral_dispatch_context;
static PeripheralDispatchOps peripheral_dispatch_ops;

static uint8_t cli_usb_rx_bytes[CLI_USB_RX_BYTE_QUEUE_CAPACITY];
static uint16_t cli_usb_rx_head;
static uint16_t cli_usb_rx_tail;
static uint16_t cli_usb_rx_count;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_ADC3_Init(void);
static void MX_DAC1_Init(void);
static void MX_OPAMP3_Init(void);
static void MX_TIM2_Init(void);
static void MX_OPAMP2_Init(void);
static void MX_I2C1_Init(void);
void StartDefaultTask(void const * argument);

/* USER CODE BEGIN PFP */

static uint32_t effects_task_ms_to_ticks(uint32_t ms, void *context);
static bool effects_task_wait_for_adc_buffer(uint32_t timeout_ticks, uint16_t **buf_ptr,
                                             void *context);
static bool effects_task_wait_for_dac_buffer(uint32_t timeout_ticks, uint16_t **buf_ptr,
                                             void *context);
static bool effects_task_dma_copy(const uint16_t *src, uint16_t *dst, size_t count,
                                  uint32_t timeout_ticks, void *context);
static bool effects_task_read_latched_state(EffectsState *state, EffectsParams *params,
                                            void *context);
static bool effects_task_replace_input_for_testing(uint16_t *buf, size_t count, void *context);
static bool effects_task_replace_output_for_testing(uint16_t *buf, size_t count, void *context);
static void effects_task_report_failure(void *context);
static void effects_task_report_frame_complete(void *context);
static void effects_task_log_runtime_stats(const char *reason, uint8_t level);
static uint32_t effects_task_get_timestamp_us(void *context);
static void effects_task_on_frame_end(uint32_t frame_time_us, bool overrun, void *context);
static void effects_task_panic_write(const char *text, void *context);
static void effects_task_on_output_frame(const uint16_t *buf, size_t count, void *context);

static void dwt_init(void);
static uint32_t dwt_get_timestamp_us(void);

typedef enum
{
    EFFECTS_BUFFER_ADC,
    EFFECTS_BUFFER_DAC,
} EffectsBufferType;

typedef enum
{
    EFFECTS_BUFFER_EVENT_ADC_A,
    EFFECTS_BUFFER_EVENT_ADC_B,
    EFFECTS_BUFFER_EVENT_DAC_A,
    EFFECTS_BUFFER_EVENT_DAC_B,
} EffectsBufferEvent;

static bool boot_uart_write(const char *text);
static void boot_log_line(const char *text);
static void boot_log_u32(const char *label, uint32_t value);

static bool log_is_enabled(void *context);
static uint8_t log_get_level(void *context);
static bool log_write_line(const char *line, void *context);

static void notify_task_from_isr(osThreadId task_handle);
static void effects_task_record_buffer_event_from_isr(EffectsBufferEvent event);
static bool cli_dual_write(const char *text, void *context);
static bool cli_usb_rx_queue_pop_byte(uint8_t *byte_out);
static void cli_service_lock(void *context);
static void cli_service_unlock(void *context);
static void cli_uart_recover_rx_errors(void);
static void cli_system_reboot(void *context);
static void init_cli_service_adapter(void);
static void init_effects_defaults(void);
static uint32_t compute_sample_rate_hz(void);
static uint32_t compute_sampling_period_us(void);
static uint32_t read_heartbeat_period_ms(void);
void startEffectsTask(void const *argument);
void startCliTask(void const *argument);

static void effects_task_on_frame_begin(void *context);
static void peripheral_i2c_signal_completion_from_isr(void *i2c_context, bool transfer_failed,
                                                      void *context);

static bool i2c_hal_ping(uint8_t address, void *context);
static bool i2c_hal_transfer(uint8_t address, const uint8_t *tx_data, size_t tx_len,
                             uint8_t *rx_data, size_t *rx_len, uint32_t timeout_ms,
                             void *context);
static bool i2c_hal_scan(uint8_t start_addr, uint8_t end_addr,
                         uint8_t *found_addrs, size_t *count,
                         size_t max_count, void *context);
static bool rom_read_raw(uint16_t address, uint16_t length, uint8_t *payload_out,
                         size_t payload_capacity, size_t *payload_size_out, void *context);
static bool rom_write_raw(uint16_t address, const uint8_t *payload, size_t payload_size,
                          void *context);
static bool rom_save_state(void *context);
static bool rom_load_state(void *context);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

static uint32_t effects_task_ms_to_ticks(uint32_t ms, void *context)
{
    (void)context;
    return pdMS_TO_TICKS(ms);
}

static bool effects_task_try_take_pending_buffer(EffectsBufferType buffer_type, uint16_t **buf_ptr)
{
    if (buf_ptr == NULL)
    {
        return false;
    }

    bool has_pending = false;

    taskENTER_CRITICAL();
    if (buffer_type == EFFECTS_BUFFER_ADC)
    {
        if (effects_event_count_adc_a > 0U)
        {
            effects_event_count_adc_a--;
            *buf_ptr = (uint16_t *)adc_buf_a;
            has_pending = true;
        }
        else if (effects_event_count_adc_b > 0U)
        {
            effects_event_count_adc_b--;
            *buf_ptr = (uint16_t *)adc_buf_b;
            has_pending = true;
        }
    }
    else if (buffer_type == EFFECTS_BUFFER_DAC)
    {
        if (effects_event_count_dac_a > 0U)
        {
            effects_event_count_dac_a--;
            *buf_ptr = (uint16_t *)dac_buf_a;
            has_pending = true;
        }
        else if (effects_event_count_dac_b > 0U)
        {
            effects_event_count_dac_b--;
            *buf_ptr = (uint16_t *)dac_buf_b;
            has_pending = true;
        }
    }
    taskEXIT_CRITICAL();

    return has_pending;
}

static bool effects_task_wait_for_typed_buffer(uint32_t timeout_ticks, uint16_t **buf_ptr,
                                               EffectsBufferType buffer_type)
{
    if (buf_ptr == NULL)
    {
        return false;
    }

    if (effects_task_try_take_pending_buffer(buffer_type, buf_ptr))
    {
        return true;
    }

    TickType_t ticks_remaining = (TickType_t)timeout_ticks;
    TickType_t started_at = 0;
    const bool bounded_wait = (timeout_ticks != 0xFFFFFFFFU);

    if (bounded_wait)
    {
        started_at = xTaskGetTickCount();
    }

    for (;;)
    {
        if (ulTaskNotifyTake(pdTRUE, ticks_remaining) == 0)
        {
            return false;
        }

        if (effects_task_try_take_pending_buffer(buffer_type, buf_ptr))
        {
            return true;
        }

        if (!bounded_wait)
        {
            continue;
        }

        TickType_t elapsed_ticks = xTaskGetTickCount() - started_at;
        if (elapsed_ticks >= (TickType_t)timeout_ticks)
        {
            return false;
        }

        ticks_remaining = (TickType_t)timeout_ticks - elapsed_ticks;
    }
}

static bool effects_task_wait_for_adc_buffer(uint32_t timeout_ticks, uint16_t **buf_ptr,
                                             void *context)
{
    (void)context;
    return effects_task_wait_for_typed_buffer(timeout_ticks, buf_ptr, EFFECTS_BUFFER_ADC);
}

static bool effects_task_wait_for_dac_buffer(uint32_t timeout_ticks, uint16_t **buf_ptr,
                                             void *context)
{
    (void)context;

    return effects_task_wait_for_typed_buffer(timeout_ticks, buf_ptr, EFFECTS_BUFFER_DAC);
}

static bool effects_task_dma_copy(const uint16_t *src, uint16_t *dst, size_t count,
                                  uint32_t timeout_ticks, void *context)
{
    (void)context;
    (void)timeout_ticks;

    if (src == NULL || dst == NULL || count == 0)
    {
        return false;
    }

    (void)memcpy(dst, src, count * sizeof(uint16_t));
    return true;
}

static bool effects_task_read_latched_state(EffectsState *state, EffectsParams *params,
                                            void *context)
{
    (void)context;
    if (state == NULL || params == NULL)
    {
        return false;
    }

    taskENTER_CRITICAL();
    *state = effects_state;
    *params = effects_params;
    taskEXIT_CRITICAL();
    return true;
}

static bool effects_task_replace_input_for_testing(uint16_t *buf, size_t count, void *context)
{
    CliServiceAdapter *adapter_context = (CliServiceAdapter *)context;
    CliTestModeStatus status = {0};

    if (buf == NULL || adapter_context == NULL)
    {
        return false;
    }

    if (!cli_service_adapter_get_test_input_mode_status(adapter_context, &status))
    {
        return false;
    }

    static bool dma_redirected = false;
    static uint32_t consecutive_dma_failures = 0U;

    if ((!status.enabled || status.vector != 5U) && dma_redirected)
    {
        if (HAL_ADC_Stop_DMA(&hadc3) != HAL_OK)
        {
            if (consecutive_dma_failures % EFFECTS_RUNTIME_LOG_EVERY_FAILURES == 0U)
            {
                (void)log_write(LOG_LEVEL_WARN, "adc dma restore: stop failed");
            }
            consecutive_dma_failures++;
            return false;
        }
        if (HAL_ADC_Start_DMA(&hadc3, (uint32_t *)adc_buf, ADC_BUF_LEN) != HAL_OK)
        {
            if (consecutive_dma_failures % EFFECTS_RUNTIME_LOG_EVERY_FAILURES == 0U)
            {
                (void)log_write(LOG_LEVEL_WARN, "adc dma restore: start failed, retrying");
            }
            consecutive_dma_failures++;
            return false;
        }
        dma_redirected = false;
        consecutive_dma_failures = 0U;
    }

    if (!status.enabled)
    {
        return true;
    }

    if (status.vector == 5U)
    {
        if (!dma_redirected)
        {
            static uint16_t discard[ADC_BUF_LEN];
            if (HAL_ADC_Stop_DMA(&hadc3) != HAL_OK)
            {
                if (consecutive_dma_failures % EFFECTS_RUNTIME_LOG_EVERY_FAILURES == 0U)
                {
                    (void)log_write(LOG_LEVEL_WARN, "adc dma redirect: stop failed");
                }
                consecutive_dma_failures++;
                return false;
            }
            if (HAL_ADC_Start_DMA(&hadc3, (uint32_t *)discard, ADC_BUF_LEN) != HAL_OK)
            {
                if (consecutive_dma_failures % EFFECTS_RUNTIME_LOG_EVERY_FAILURES == 0U)
                {
                    (void)log_write(LOG_LEVEL_WARN, "adc dma redirect: start failed, retrying");
                }
                consecutive_dma_failures++;
                return false;
            }
            dma_redirected = true;
            consecutive_dma_failures = 0U;
        }
        for (size_t i = 0; i < count; i++)
        {
            int16_t s;
            (void)usb_audio_stream_pop_sample_or_hold(&audio_stream, &s);
            buf[i] = usb_i16_to_sample(s);
        }
        return true;
    }

    return test_vector_source_fill_buffer(&test_vector_source, &status, buf, count);
}

static bool effects_task_replace_output_for_testing(uint16_t *buf, size_t count, void *context)
{
    CliServiceAdapter *adapter_context = (CliServiceAdapter *)context;
    CliTestModeStatus status = {0};

    if (buf == NULL || adapter_context == NULL)
    {
        return false;
    }

    if (!cli_service_adapter_get_test_output_mode_status(adapter_context, &status))
    {
        return false;
    }

    if (!status.enabled)
    {
        return true;
    }

    return test_vector_source_fill_buffer(&test_vector_source_output, &status, buf, count);
}

static void effects_task_report_failure(void *context)
{
    effects_frames_failed++;
    cli_service_adapter_note_processing_failure((CliServiceAdapter *)context);
    HAL_GPIO_TogglePin(STATUS_LED_GPIO_Port, STATUS_LED_Pin);

    effects_task_panic_write("panic: effects processing failure\n", NULL);

    if (effects_frames_failed % EFFECTS_RUNTIME_LOG_EVERY_FAILURES == 0U)
    {
        effects_task_log_runtime_stats("failure", LOG_LEVEL_WARN);
    }
}

static void effects_task_report_frame_complete(void *context)
{
    effects_frames_ok++;
    cli_service_adapter_note_frame_processed((CliServiceAdapter *)context);
}

static void effects_task_log_runtime_stats(const char *reason, uint8_t level)
{
    char line[96];

    (void)snprintf(line, sizeof(line),
                   "effects runtime %s: ok=%lu fail=%lu",
                   reason,
                   (unsigned long)effects_frames_ok,
                   (unsigned long)effects_frames_failed);
    (void)log_write(level, line);
}

static bool dwt_ready;

static void dwt_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    DWT->CYCCNT = 0;
    dwt_ready = (DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk) != 0;
}

static uint32_t dwt_get_timestamp_us(void)
{
    if (!dwt_ready)
    {
        return 0;
    }
    return DWT->CYCCNT / (SystemCoreClock / 1000000U);
}

static uint32_t effects_task_get_timestamp_us(void *context)
{
    (void)context;
    return dwt_get_timestamp_us();
}

static void effects_task_on_frame_end(uint32_t frame_time_us, bool overrun, void *context)
{
    cli_service_adapter_note_frame_timing((CliServiceAdapter *)context, frame_time_us,
                                          overrun);
}

static void effects_task_panic_write(const char *text, void *context)
{
    (void)context;
    if (text != NULL && (HAL_UART_GetState(&huart2) & HAL_UART_STATE_BUSY_TX) == 0U)
    {
        (void)HAL_UART_Transmit(&huart2, (uint8_t *)text, strlen(text), 25);
    }
}

static void effects_task_on_output_frame(const uint16_t *buf, size_t count, void *context)
{
    (void)context;
    if (buf == NULL || count == 0)
    {
        return;
    }

    if (!audio_stream.output_active)
    {
        return;
    }

    usb_audio_stream_note_effects_frame(&audio_stream);

    for (size_t i = 0; i < count; i++)
    {
        usb_audio_stream_push_sample(&audio_stream, sample_to_usb_i16(buf[i]));
    }

#ifdef ENABLE_DIAGNOSTICS
    static uint32_t out_drop_last_reported = 0U;
    uint32_t out_dropped_now = usb_audio_stream_out_dropped(&audio_stream);
    if (out_dropped_now - out_drop_last_reported >= 256U)
    {
        out_drop_last_reported = out_dropped_now;
        PANIC_USB("out_ring dropping");
    }
#endif
}

static bool test_vector_stream_pop_sample(int16_t *sample, void *context)
{
    (void)context;
    return usb_audio_stream_pop_sample(&audio_stream, sample);
}

static bool boot_uart_write(const char *text)
{
    if (text == NULL)
    {
        return false;
    }

    const size_t length = strlen(text);
    if (length == 0 || length > UINT16_MAX)
    {
        return false;
    }

    return HAL_UART_Transmit(&huart2, (uint8_t *)text, (uint16_t)length, 25) == HAL_OK;
}

static void boot_log_line(const char *text)
{
    (void)boot_uart_write(text);
    (void)boot_uart_write("\r\n");
}

static void boot_log_u32(const char *label, uint32_t value)
{
    char line[96];
    (void)snprintf(line, sizeof(line), "%s%lu", label, (unsigned long)value);
    boot_log_line(line);
}

static void notify_task_from_isr(osThreadId task_handle)
{
    if (task_handle == NULL)
    {
        return;
    }

    BaseType_t higher_priority_task_woken = pdFALSE;
    vTaskNotifyGiveFromISR((TaskHandle_t)task_handle, &higher_priority_task_woken);
    portYIELD_FROM_ISR(higher_priority_task_woken);
}

static void effects_task_record_buffer_event_from_isr(EffectsBufferEvent event)
{
    UBaseType_t saved_interrupt_mask = taskENTER_CRITICAL_FROM_ISR();

    switch (event)
    {
    case EFFECTS_BUFFER_EVENT_ADC_A:
        effects_event_count_adc_a++;
        effects_event_cum_adc++;
        break;
    case EFFECTS_BUFFER_EVENT_ADC_B:
        effects_event_count_adc_b++;
        effects_event_cum_adc++;
        break;
    case EFFECTS_BUFFER_EVENT_DAC_A:
        effects_event_count_dac_a++;
        effects_event_cum_dac++;
        break;
    case EFFECTS_BUFFER_EVENT_DAC_B:
        effects_event_count_dac_b++;
        effects_event_cum_dac++;
        break;
    default:
        break;
    }

    taskEXIT_CRITICAL_FROM_ISR(saved_interrupt_mask);
}

static void effects_task_hw_events_get(uint32_t *adc_events, uint32_t *dac_events)
{
    taskENTER_CRITICAL();
    if (adc_events != NULL)
    {
        *adc_events = effects_event_cum_adc;
    }
    if (dac_events != NULL)
    {
        *dac_events = effects_event_cum_dac;
    }
    taskEXIT_CRITICAL();
}

static void effects_task_hw_events_reset(void)
{
    taskENTER_CRITICAL();
    effects_event_cum_adc = 0U;
    effects_event_cum_dac = 0U;
    taskEXIT_CRITICAL();
}

static void cli_service_lock(void *context)
{
    (void)context;
    taskENTER_CRITICAL();
}

static void cli_service_unlock(void *context)
{
    (void)context;
    taskEXIT_CRITICAL();
}

static bool log_is_enabled(void *context)
{
    bool enabled = false;
    return cli_service_adapter_get_log_enabled((CliServiceAdapter *)context, &enabled) &&
           enabled;
}

static uint8_t log_get_level(void *context)
{
    uint8_t level = 0;
    if (!cli_service_adapter_get_log_level((CliServiceAdapter *)context, &level))
    {
        return 0;
    }

    return level;
}

static bool log_write_line(const char *line, void *context)
{
    return cli_service_adapter_append_log_line((CliServiceAdapter *)context, line);
}

static void cli_uart_recover_rx_errors(void)
{
    if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_ORE) != RESET)
    {
        __HAL_UART_CLEAR_OREFLAG(&huart2);
    }

    if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_FE) != RESET)
    {
        __HAL_UART_CLEAR_FEFLAG(&huart2);
    }

    if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_NE) != RESET)
    {
        __HAL_UART_CLEAR_NEFLAG(&huart2);
    }

    if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_PE) != RESET)
    {
        __HAL_UART_CLEAR_PEFLAG(&huart2);
    }

    huart2.ErrorCode = HAL_UART_ERROR_NONE;
}

static void init_cli_service_adapter(void)
{
    CliServiceAdapterOps ops = {
        .lock = cli_service_lock,
        .unlock = cli_service_unlock,
        .rom_save_state = rom_save_state,
        .rom_load_state = rom_load_state,
        .rom_read_raw = rom_read_raw,
        .rom_write_raw = rom_write_raw,
        .log_set_level = NULL,
        .log_set_enabled = NULL,
        .log_set_stream = NULL,
        .system_reboot = cli_system_reboot,
        .i2c_ping = i2c_hal_ping,
        .i2c_transfer = i2c_hal_transfer,
        .i2c_scan = i2c_hal_scan,
        .context = NULL,
    };

    cli_service_adapter_init(&cli_service_adapter_context,
                             (EffectsState *)&effects_state,
                             (EffectsParams *)&effects_params,
                             &ops,
                             UINT16_MAX);
    cli_service_adapter_bind_heartbeat_period(&cli_service_adapter_context,
                                              (uint32_t *)&heartbeat_period_ms,
                                              HEARTBEAT_PERIOD_MS_MIN,
                                              HEARTBEAT_PERIOD_MS_MAX);

    LogOps log_ops = {
        .is_enabled = log_is_enabled,
        .get_level = log_get_level,
        .write_line = log_write_line,
        .context = &cli_service_adapter_context,
    };
    log_configure(&log_ops);

    cli_usb_rx_head = 0;
    cli_usb_rx_tail = 0;
    cli_usb_rx_count = 0;

    usb_audio_stream_init(&audio_stream);
    usbd_uac_register_stream(&audio_stream);
    cli_service_adapter_bind_uac_diag(&cli_service_adapter_context,
                                      usbd_uac_get_diag,
                                      usbd_uac_reset_diag);
    cli_service_adapter_bind_hw_events(&cli_service_adapter_context,
                                       effects_task_hw_events_get,
                                       effects_task_hw_events_reset);
}

static void init_effects_defaults(void)
{
    taskENTER_CRITICAL();

    (void)memset((void *)&effects_state, 0, sizeof(effects_state));
    (void)memset((void *)&effects_params, 0, sizeof(effects_params));

    effects_state_set_default_slots((EffectsState *)&effects_state);

    effects_params.overdrive.gain = MAX_OVERDRIVE_GAIN;
    effects_params.overdrive.level = MAX_OVERDRIVE_LEVEL;
    effects_params.overdrive.tone = MIN_OVERDRIVE_TONE;
    effects_params.overdrive.mix = 0;

    effects_params.echo.delay_samples = 1;
    effects_params.echo.pre_delay = MIN_ECHO_PRE_DELAY;
    effects_params.echo.density = MAX_ECHO_DENSITY;
    effects_params.echo.attack = MAX_ECHO_ATTACK;
    effects_params.echo.decay = MAX_ECHO_DECAY;

    effects_params.compression.threshold = X_AXIS;
    effects_params.compression.ratio = 0;

    taskEXIT_CRITICAL();
}

static uint32_t compute_sample_rate_hz(void)
{
    const uint32_t prescaler = (uint32_t)(htim2.Instance->PSC) + 1U;
    const uint32_t period = (uint32_t)(htim2.Instance->ARR) + 1U;

    return HAL_RCC_GetHCLKFreq() / (prescaler * period);
}

static uint32_t compute_sampling_period_us(void)
{
    const uint32_t sample_rate_hz = compute_sample_rate_hz();

    return sample_rate_hz == 0U ? 25U : (1000000U / sample_rate_hz);
}

static uint32_t read_heartbeat_period_ms(void)
{
    uint32_t period_ms = HEARTBEAT_PERIOD_MS_DEFAULT;

    taskENTER_CRITICAL();
    period_ms = heartbeat_period_ms;
    taskEXIT_CRITICAL();

    return period_ms;
}

void startEffectsTask(void const *argument)
{
    (void)argument;
    dwt_init();
    (void)log_write(LOG_LEVEL_INFO, "effects task start");

    EffectsPipeline effects_pipeline;
    if (effects_pipeline_init(&effects_pipeline) != 0)
    {
        vTaskDelete(NULL);
        return;
    }

    echo_state_reset(&echo_state);
    (void)effects_pipeline_attach_echo_state(&effects_pipeline, &echo_state);

    const uint32_t sampling_period_us = compute_sampling_period_us();
    EffectsTaskContext task_context = {
        .pipeline = &effects_pipeline,
        .effects_state = &effects_state,
        .effects_params = &effects_params,
        .adc_buf_a = (uint16_t *)adc_buf_a,
        .adc_buf_b = (uint16_t *)adc_buf_b,
        .dac_buf_a = (uint16_t *)dac_buf_a,
        .dac_buf_b = (uint16_t *)dac_buf_b,
        .sample_buf_len = SAMPLE_BUF_LEN,
        .sampling_period_us = sampling_period_us,
        .processing_slack_ms = 2,
    };

    cli_service_adapter_set_audio_info(&cli_service_adapter_context,
                                       compute_sample_rate_hz(),
                                       SAMPLE_BUF_LEN,
                                       sampling_period_us,
                                       NUM_DELAY_SAMPLES,
                                       2);

    test_vector_source_init(&test_vector_source,
                            task_context.sampling_period_us == 0U
                                ? 48000U
                                : (1000000U / task_context.sampling_period_us));
    test_vector_source_init(&test_vector_source_output,
                            task_context.sampling_period_us == 0U
                                ? 48000U
                                : (1000000U / task_context.sampling_period_us));

    test_vector_source.stream_pop = test_vector_stream_pop_sample;
    test_vector_source.stream_context = NULL;
    test_vector_source_output.stream_pop = test_vector_stream_pop_sample;
    test_vector_source_output.stream_context = NULL;

    EffectsTaskOps task_ops = {
        .wait_for_adc_buffer = effects_task_wait_for_adc_buffer,
        .wait_for_dac_buffer = effects_task_wait_for_dac_buffer,
        .dma_copy = effects_task_dma_copy,
        .read_latched_state = effects_task_read_latched_state,
        .replace_input_for_testing = effects_task_replace_input_for_testing,
        .replace_output_for_testing = effects_task_replace_output_for_testing,
        .report_failure = effects_task_report_failure,
        .report_frame_complete = effects_task_report_frame_complete,
        .ms_to_ticks = effects_task_ms_to_ticks,
        .on_frame_begin = effects_task_on_frame_begin,
        .on_frame_end = effects_task_on_frame_end,
        .get_timestamp_us = effects_task_get_timestamp_us,
        .panic_write = effects_task_panic_write,
        .on_output_frame = effects_task_on_output_frame,
        .context = &cli_service_adapter_context,
    };

    if (HAL_OPAMP_Start(&hopamp2) != HAL_OK ||
        HAL_OPAMP_Start(&hopamp3) != HAL_OK ||
        HAL_ADCEx_Calibration_Start(&hadc3, ADC_SINGLE_ENDED) != HAL_OK ||
        HAL_ADC_Start_DMA(&hadc3, (uint32_t *)adc_buf, ADC_BUF_LEN) != HAL_OK ||
        HAL_DAC_Start_DMA(&hdac1, DAC_CHANNEL_1, (uint32_t *)dac_buf, DAC_BUF_LEN,
                          DAC_ALIGN_12B_L) != HAL_OK ||
        HAL_TIM_OC_Start(&htim2, TIM_CHANNEL_1) != HAL_OK ||
        HAL_TIM_Base_Start(&htim2) != HAL_OK)
    {
        vTaskDelete(NULL);
        return;
    }

    for (;;)
    {
        bool step_ok = effects_task_step(&task_context, &task_ops);
        cli_service_adapter_note_step_result(&cli_service_adapter_context, step_ok);
    }
}

void startCliTask(void const *argument)
{
    (void)argument;

    (void)log_write(LOG_LEVEL_INFO, "cli task start");

    cli_service_adapter_set_audio_stream(&cli_service_adapter_context, &audio_stream);
    cli_service_adapter_bind(&cli_service_adapter_context, &cli_services);
    CliSessionTransport transport = {
        .write = cli_dual_write,
        .context = NULL,
    };
    cli_session_init(&cli_session, &cli_services, &transport);
    (void)cli_session_start(&cli_session);

    for (;;)
    {
        (void)cli_session_start(&cli_session);

        uint8_t byte = 0;
        while (cli_usb_rx_queue_pop_byte(&byte))
        {
            cli_session_push_bytes(&cli_session, &byte, 1);
        }

        while (HAL_UART_Receive(&huart2, &byte, 1, 0) == HAL_OK)
        {
            cli_session_push_bytes(&cli_session, &byte, 1);
        }

        cli_uart_recover_rx_errors();
        cli_session_poll(&cli_session);

#ifdef ENABLE_DIAGNOSTICS
        /* in_ring drop reporting deferred from ISR — PANIC_USB calls
         * a blocking UART transmit. */
        {
            static uint32_t in_drop_last_reported = 0U;
            uint32_t in_dropped_now = usb_audio_stream_in_dropped(&audio_stream);
            if (in_dropped_now - in_drop_last_reported >= 256U)
            {
                in_drop_last_reported = in_dropped_now;
                PANIC_USB("in_ring dropping");
            }
        }
#endif

        osDelay(1);
    }
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == B1_Pin)
    {
        b1_pressed = true;
    }
}

static void effects_task_on_frame_begin(void *context)
{
    CliServiceAdapter *adapter = (CliServiceAdapter *)context;

    if (!b1_pressed || adapter == NULL)
    {
        return;
    }

    b1_pressed = false;

    taskENTER_CRITICAL();
#ifdef B1_TOGGLE_INPUT
    adapter->test_input.enabled = !adapter->test_input.enabled;
#else
    adapter->test_output.enabled = !adapter->test_output.enabled;
#endif
    taskEXIT_CRITICAL();
}

static void cli_system_reboot(void *context)
{
    (void)context;
    NVIC_SystemReset();
}

static void peripheral_i2c_signal_completion_from_isr(void *i2c_context, bool transfer_failed,
                                                      void *context)
{
    (void)context;
    i2c_task_signal_completion_from_isr((const I2CTaskSupportContext *)i2c_context,
                                        transfer_failed);
}

static bool i2c_hal_ping(uint8_t address, void *context)
{
    (void)context;

    if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(100)) != pdTRUE)
    {
        return false;
    }

    bool result = HAL_I2C_IsDeviceReady(&hi2c1, (uint16_t)address << 1, 1, 10) == HAL_OK;

    xSemaphoreGive(i2c_mutex);
    return result;
}

static bool i2c_hal_transfer(uint8_t address, const uint8_t *tx_data, size_t tx_len,
                             uint8_t *rx_data, size_t *rx_len, uint32_t timeout_ms,
                             void *context)
{
    (void)context;

    if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(100)) != pdTRUE)
    {
        return false;
    }

    uint16_t hal_addr = (uint16_t)address << 1;
    bool result = false;

    if (tx_len > 0 && (rx_data == NULL || *rx_len == 0))
    {
        result = HAL_I2C_Master_Transmit(&hi2c1, hal_addr, (uint8_t *)tx_data,
                                         (uint16_t)tx_len, timeout_ms) == HAL_OK;
    }
    else if (tx_len > 0 && rx_data != NULL && *rx_len > 0)
    {
        result = HAL_I2C_Master_Transmit(&hi2c1, hal_addr, (uint8_t *)tx_data,
                                         (uint16_t)tx_len, timeout_ms) == HAL_OK;
        if (result)
        {
            result = HAL_I2C_Master_Receive(&hi2c1, hal_addr | 1U, rx_data,
                                            (uint16_t)*rx_len, timeout_ms) == HAL_OK;
        }
    }
    else if ((tx_len == 0 || tx_data == NULL) && rx_data != NULL && *rx_len > 0)
    {
        result = HAL_I2C_Master_Receive(&hi2c1, hal_addr | 1U, rx_data,
                                        (uint16_t)*rx_len, timeout_ms) == HAL_OK;
    }

    xSemaphoreGive(i2c_mutex);
    return result;
}

static bool i2c_hal_scan(uint8_t start_addr, uint8_t end_addr,
                         uint8_t *found_addrs, size_t *count,
                         size_t max_count, void *context)
{
    (void)context;
    if (found_addrs == NULL || count == NULL || max_count == 0)
    {
        return false;
    }

    if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(100)) != pdTRUE)
    {
        return false;
    }

    size_t found = 0;
    for (uint8_t addr = start_addr; addr <= end_addr && addr <= 0x7F; addr++)
    {
        if (HAL_I2C_IsDeviceReady(&hi2c1, (uint16_t)addr << 1, 1, 2) == HAL_OK)
        {
            if (found < max_count)
            {
                found_addrs[found++] = addr;
            }
        }
    }
    *count = found;

    xSemaphoreGive(i2c_mutex);
    return true;
}

static void i2c_boot_scan(void)
{
    boot_log_line("boot: i2c probing...");

    xSemaphoreTake(i2c_mutex, portMAX_DELAY);

    i2c_rom_detected = (HAL_I2C_IsDeviceReady(&hi2c1, (ROM_I2C_ADDR_7BIT << 1), 1, 10) == HAL_OK);
    bool display_detected = (HAL_I2C_IsDeviceReady(&hi2c1, (DISPLAY_I2C_ADDR_7BIT << 1), 1, 10) == HAL_OK);

    xSemaphoreGive(i2c_mutex);

    if (i2c_rom_detected)
    {
        boot_log_line("boot: i2c: ROM (FRAM) detected at 0x50");
    }
    else
    {
        boot_log_line("boot: i2c: ROM (FRAM) not detected at 0x50");
    }

    if (display_detected)
    {
        boot_log_line("boot: i2c: display (OLED) detected at 0x3C");
    }
    else
    {
        boot_log_line("boot: i2c: display (OLED) not detected at 0x3C");
    }
}

static uint8_t *allocate_payload_adapter(size_t size, void *context)
{
    (void)context;
    if (size == 0)
    {
        return NULL;
    }
    return (uint8_t *)pvPortMalloc(size);
}

static void free_payload_adapter(uint8_t *payload, void *context)
{
    (void)context;
    if (payload != NULL)
    {
        vPortFree(payload);
    }
}

static void set_rom_write_disable_adapter(bool disable_writes, void *context)
{
    (void)context;
    (void)disable_writes;
}

void startI2cHandlerTask(void const *argument)
{
    (void)argument;
    for (;;)
    {
        I2CHandlerMessage msg;
        if (xQueueReceive(i2cQueueHandle, &msg, portMAX_DELAY) == pdTRUE)
        {
            if (xSemaphoreTake(i2c_mutex, portMAX_DELAY) == pdTRUE)
            {
                (void)i2c_handler_process_message(&msg, &i2c_handler_config,
                                                  &i2c_handler_ops, &i2c_task_support_context);
                xSemaphoreGive(i2c_mutex);
            }
        }
    }
}

static bool rom_read_raw(uint16_t address, uint16_t length, uint8_t *payload_out,
                         size_t payload_capacity, size_t *payload_size_out, void *context)
{
    (void)context;
    if (payload_out == NULL || payload_size_out == NULL || payload_capacity == 0)
    {
        return false;
    }

    if (length == 0 || (size_t)length > payload_capacity)
    {
        return false;
    }

    uint8_t addr_buf[ROM_HANDLER_ADDRESS_BYTES];
    (void)rom_handler_encode_address(address, addr_buf, sizeof(addr_buf));

    if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(100)) != pdTRUE)
    {
        return false;
    }

    bool result = HAL_I2C_Master_Transmit(&hi2c1, (ROM_I2C_ADDR_7BIT << 1), addr_buf,
                                          ROM_HANDLER_ADDRESS_BYTES, 100) == HAL_OK;
    if (result)
    {
        result = HAL_I2C_Master_Receive(&hi2c1, (ROM_I2C_ADDR_7BIT << 1) | 1U,
                                        payload_out, length, 100) == HAL_OK;
    }

    xSemaphoreGive(i2c_mutex);

    if (result)
    {
        *payload_size_out = (size_t)length;
    }
    return result;
}

static bool rom_write_raw(uint16_t address, const uint8_t *payload, size_t payload_size,
                          void *context)
{
    (void)context;
    if (payload == NULL || payload_size == 0 || payload_size > 64)
    {
        return false;
    }

    uint8_t buf[2 + 64];
    (void)rom_handler_encode_address(address, buf, ROM_HANDLER_ADDRESS_BYTES);
    memcpy(buf + ROM_HANDLER_ADDRESS_BYTES, payload, payload_size);
    uint16_t total = (uint16_t)(payload_size + ROM_HANDLER_ADDRESS_BYTES);

    if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(100)) != pdTRUE)
    {
        return false;
    }

    bool result = HAL_I2C_Master_Transmit(&hi2c1, (ROM_I2C_ADDR_7BIT << 1), buf, total, 5000) == HAL_OK;

    xSemaphoreGive(i2c_mutex);
    return result;
}

static bool rom_save_state(void *context)
{
    (void)context;
    if (!i2c_rom_detected || !rom_task_support_ops.queue_message_and_wait)
    {
        return false;
    }
    volatile EffectsState *vstate = &effects_state;
    volatile EffectsParams *vparams = &effects_params;
    return rom_task_save_effects(&rom_task_support_config, &rom_task_support_ops,
                                 (const EffectsState *)vstate,
                                 (const EffectsParams *)vparams);
}

static bool rom_load_state(void *context)
{
    (void)context;
    if (!i2c_rom_detected || !rom_task_support_ops.queue_message_and_wait)
    {
        return false;
    }
    volatile EffectsState *vst = &effects_state;
    volatile EffectsParams *vp = &effects_params;
    return rom_task_bootstrap_effects(&rom_task_support_config, &rom_task_support_ops,
                                      (EffectsState *)vst,
                                      (EffectsParams *)vp);
}

void startRomHandlerTask(void const *argument)
{
    (void)argument;

    (void)log_write(LOG_LEVEL_INFO, "rom handler task start");

    if (!i2c_rom_detected)
    {
        (void)log_write(LOG_LEVEL_WARN, "rom handler: no ROM detected, skipping");
        for (;;)
        {
            osDelay(60000);
        }
    }

    volatile EffectsState *boot_vst = &effects_state;
    volatile EffectsParams *boot_vp = &effects_params;
    bool bootstrap_ok = rom_task_bootstrap_effects(&rom_task_support_config,
                                                   &rom_task_support_ops,
                                                   (EffectsState *)boot_vst,
                                                   (EffectsParams *)boot_vp);

    if (bootstrap_ok)
    {
        (void)log_write(LOG_LEVEL_INFO, "rom handler: bootstrap OK");
    }
    else
    {
        (void)log_write(LOG_LEVEL_WARN, "rom handler: bootstrap FAILED");
    }

    for (;;)
    {
        osDelay(5000);
        volatile EffectsState *save_vst = &effects_state;
        volatile EffectsParams *save_vp = &effects_params;
        bool save_ok = rom_task_save_effects(&rom_task_support_config, &rom_task_support_ops,
                                             (const EffectsState *)save_vst,
                                             (const EffectsParams *)save_vp);
        if (!save_ok)
        {
            (void)log_write(LOG_LEVEL_WARN, "rom handler: save FAILED");
        }
    }
}

static bool cli_dual_write(const char *text, void *context)
{
    (void)context;

    if (text == NULL)
    {
        return false;
    }

    const size_t length = strlen(text);
    if (length == 0 || length > UINT16_MAX)
    {
        return false;
    }

    bool usb_ok = false;
    for (uint8_t attempts = 0; attempts < 10; attempts++)
    {
        const uint8_t result =
            USBD_CDC_CLI_Transmit_FS((uint8_t *)text, (uint16_t)length);
        if (result == USBD_OK)
        {
            usb_ok = true;
            break;
        }

        if (result != USBD_BUSY)
        {
            break;
        }

        osDelay(1);
    }

    HAL_StatusTypeDef uart_status =
        HAL_UART_Transmit(&huart2, (uint8_t *)text, (uint16_t)length, 25);
    bool uart_ok = (uart_status == HAL_OK);

    return usb_ok || uart_ok;
}

/*
 * CDC0 (CLI) interface callbacks
 */

static uint8_t UserRxBufferFS_CLI[APP_RX_DATA_SIZE];
static uint8_t UserTxBufferFS_CLI[APP_TX_DATA_SIZE];

static int8_t CDC_CLI_Init(void)
{
    USBD_CDC_CLI_SetRxBuffer(UserRxBufferFS_CLI);
    USBD_CDC_CLI_SetTxBuffer(UserTxBufferFS_CLI, 0);
    PANIC_USB("CDC0 cli up");
    return 0;
}

static int8_t CDC_CLI_DeInit(void)
{
    PANIC_USB("CDC0 cli down");
    return 0;
}

static int8_t CDC_CLI_Control(uint8_t cmd, uint8_t *pbuf, uint16_t length)
{
    (void)cmd;
    (void)pbuf;
    (void)length;
    return 0;
}

static int8_t CDC_CLI_Receive(uint8_t *buf, uint32_t *len)
{
    USBD_CDC_CLI_SetRxBuffer(buf);
    USBD_CDC_CLI_ReceivePacket();

    if (buf != NULL && len != NULL)
    {
        for (uint32_t i = 0; i < *len; i++)
        {
            __disable_irq();
            if (cli_usb_rx_count >= CLI_USB_RX_BYTE_QUEUE_CAPACITY)
            {
                cli_usb_rx_head = (uint16_t)((cli_usb_rx_head + 1U) %
                                             CLI_USB_RX_BYTE_QUEUE_CAPACITY);
                cli_usb_rx_count--;
            }
            cli_usb_rx_bytes[cli_usb_rx_tail] = buf[i];
            cli_usb_rx_tail = (uint16_t)((cli_usb_rx_tail + 1U) %
                                         CLI_USB_RX_BYTE_QUEUE_CAPACITY);
            cli_usb_rx_count++;
            __enable_irq();
        }
    }
    return 0;
}

USBD_CDC_ItfTypeDef USBD_Interface_fops_FS_CLI = {
    CDC_CLI_Init,
    CDC_CLI_DeInit,
    CDC_CLI_Control,
    CDC_CLI_Receive,
};

static bool cli_usb_rx_queue_pop_byte(uint8_t *byte_out)
{
    if (byte_out == NULL)
    {
        return false;
    }

    __disable_irq();
    if (cli_usb_rx_count == 0)
    {
        __enable_irq();
        return false;
    }

    *byte_out = cli_usb_rx_bytes[cli_usb_rx_head];
    cli_usb_rx_head = (uint16_t)((cli_usb_rx_head + 1U) % CLI_USB_RX_BYTE_QUEUE_CAPACITY);
    cli_usb_rx_count--;
    __enable_irq();

    return true;
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_USART2_UART_Init();
  MX_ADC3_Init();
  MX_DAC1_Init();
  MX_OPAMP3_Init();
  MX_TIM2_Init();
  MX_OPAMP2_Init();
  MX_I2C1_Init();
  /* USER CODE BEGIN 2 */

  boot_log_line("");

  boot_log_line("boot: init effects defaults");

  init_effects_defaults();

  boot_log_line("boot: init cli service adapter");
  init_cli_service_adapter();

  boot_log_u32("boot: sample period us=", compute_sampling_period_us());
  boot_log_u32("boot: delay samples=", NUM_DELAY_SAMPLES);

  /* USER CODE END 2 */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  i2c_mutex = xSemaphoreCreateMutex();
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  i2c_completion_semaphore = xSemaphoreCreateBinaryStatic(&i2c_completion_semaphore_buffer);
  i2c_failed_rom_semaphore = xSemaphoreCreateBinaryStatic(&i2c_failed_rom_semaphore_buffer);
  i2c_failed_display_semaphore = xSemaphoreCreateBinaryStatic(&i2c_failed_display_semaphore_buffer);
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
    /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  i2cQueueHandle = xQueueCreateStatic(16, sizeof(I2CHandlerMessage),
                                      i2c_queue_buffer, &i2c_queue_control_block);
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* definition and creation of defaultTask */
  osThreadDef(defaultTask, StartDefaultTask, osPriorityNormal, 0, 128);
  defaultTaskHandle = osThreadCreate(osThread(defaultTask), NULL);

  /* USER CODE BEGIN RTOS_THREADS */
  i2c_boot_scan();

  peripheral_dispatch_context.i2c_handle = &hi2c1;
  peripheral_dispatch_context.i2c_task_support_context = &i2c_task_support_context;
  peripheral_dispatch_ops.i2c_signal_completion_from_isr = peripheral_i2c_signal_completion_from_isr;

  boot_log_line("boot: create i2c handler task");
  osThreadDef(i2cHandlerTask, startI2cHandlerTask, osPriorityNormal, 0, 128);
  i2cHandlerTaskHandle = osThreadCreate(osThread(i2cHandlerTask), NULL);

  if (i2c_rom_detected)
  {
      boot_log_line("boot: create rom handler task");
      osThreadDef(romHandlerTask, startRomHandlerTask, osPriorityNormal, 0, 128);
      romHandlerTaskHandle = osThreadCreate(osThread(romHandlerTask), NULL);
  }
  else
  {
      boot_log_line("boot: rom handler: not detected, skipped");
  }

  i2c_task_support_init(&i2c_task_support_context,
                        &i2c_handler_config,
                        &i2c_handler_ops,
                        &hi2c1,
                        romHandlerTaskHandle,
                        NULL,
                        i2c_completion_semaphore,
                        i2c_failed_rom_semaphore,
                        i2c_failed_display_semaphore,
                        &i2c_transfer_failed);

  if (i2c_rom_detected)
  {
      rom_task_support_init(&rom_task_support_config,
                            &rom_task_support_ops,
                            romHandlerTaskHandle,
                            i2cQueueHandle,
                            i2c_failed_rom_semaphore,
                            (ROM_I2C_ADDR_7BIT << 1),
                            0,
                            0,
                            pdMS_TO_TICKS(100),
                            pdMS_TO_TICKS(5000),
                            i2c_task_queue_message_and_wait,
                            allocate_payload_adapter,
                            free_payload_adapter,
                            set_rom_write_disable_adapter,
                            NULL);
  }

  boot_log_line("boot: create effects task");
  osThreadDef(effectsTask, startEffectsTask, osPriorityRealtime, 0, 384);
  effectsTaskHandle = osThreadCreate(osThread(effectsTask), NULL);
  if (effectsTaskHandle == NULL)
  {
      boot_log_line("boot: effects task create failed");
      Error_Handler();
  }

  boot_log_line("boot: create cli task");

  osThreadDef(cliTask, startCliTask, osPriorityNormal, 0, 512);
  cliTaskHandle = osThreadCreate(osThread(cliTask), NULL);
  if (cliTaskHandle == NULL)
  {
      boot_log_line("boot: cli task create failed");
      Error_Handler();
  }
  boot_log_line("boot: starting scheduler");
  /* USER CODE END RTOS_THREADS */

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL6;
  RCC_OscInitStruct.PLL.PREDIV = RCC_PREDIV_DIV1;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USB|RCC_PERIPHCLK_USART2
                              |RCC_PERIPHCLK_I2C1|RCC_PERIPHCLK_ADC34
                              |RCC_PERIPHCLK_TIM2;
  PeriphClkInit.Usart2ClockSelection = RCC_USART2CLKSOURCE_PCLK1;
  PeriphClkInit.Adc34ClockSelection = RCC_ADC34PLLCLK_DIV1;
  PeriphClkInit.I2c1ClockSelection = RCC_I2C1CLKSOURCE_HSI;
  PeriphClkInit.USBClockSelection = RCC_USBCLKSOURCE_PLL;
  PeriphClkInit.Tim2ClockSelection = RCC_TIM2CLK_HCLK;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC3_Init(void)
{

  /* USER CODE BEGIN ADC3_Init 0 */

  /* USER CODE END ADC3_Init 0 */

  ADC_MultiModeTypeDef multimode = {0};
  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC3_Init 1 */

  /* USER CODE END ADC3_Init 1 */

  /** Common config
  */
  hadc3.Instance = ADC3;
  hadc3.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV1;
  hadc3.Init.Resolution = ADC_RESOLUTION_12B;
  hadc3.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc3.Init.ContinuousConvMode = DISABLE;
  hadc3.Init.DiscontinuousConvMode = DISABLE;
  hadc3.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;
  hadc3.Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T2_TRGO;
  hadc3.Init.DataAlign = ADC_DATAALIGN_LEFT;
  hadc3.Init.NbrOfConversion = 1;
  hadc3.Init.DMAContinuousRequests = ENABLE;
  hadc3.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc3.Init.LowPowerAutoWait = DISABLE;
  hadc3.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
  if (HAL_ADC_Init(&hadc3) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure the ADC multi-mode
  */
  multimode.Mode = ADC_MODE_INDEPENDENT;
  if (HAL_ADCEx_MultiModeConfigChannel(&hadc3, &multimode) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_1;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.SamplingTime = ADC_SAMPLETIME_19CYCLES_5;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  if (HAL_ADC_ConfigChannel(&hadc3, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC3_Init 2 */

  /* USER CODE END ADC3_Init 2 */

}

/**
  * @brief DAC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_DAC1_Init(void)
{

  /* USER CODE BEGIN DAC1_Init 0 */

  /* USER CODE END DAC1_Init 0 */

  DAC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN DAC1_Init 1 */

  /* USER CODE END DAC1_Init 1 */

  /** DAC Initialization
  */
  hdac1.Instance = DAC1;
  if (HAL_DAC_Init(&hdac1) != HAL_OK)
  {
    Error_Handler();
  }

  /** DAC channel OUT1 config
  */
  sConfig.DAC_Trigger = DAC_TRIGGER_T2_TRGO;
  sConfig.DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE;
  if (HAL_DAC_ConfigChannel(&hdac1, &sConfig, DAC_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN DAC1_Init 2 */

  /* USER CODE END DAC1_Init 2 */

}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.Timing = 0x00201D2B;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief OPAMP2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_OPAMP2_Init(void)
{

  /* USER CODE BEGIN OPAMP2_Init 0 */

  /* USER CODE END OPAMP2_Init 0 */

  /* USER CODE BEGIN OPAMP2_Init 1 */

  /* USER CODE END OPAMP2_Init 1 */
  hopamp2.Instance = OPAMP2;
  hopamp2.Init.Mode = OPAMP_FOLLOWER_MODE;
  hopamp2.Init.NonInvertingInput = OPAMP_NONINVERTINGINPUT_IO0;
  hopamp2.Init.TimerControlledMuxmode = OPAMP_TIMERCONTROLLEDMUXMODE_DISABLE;
  hopamp2.Init.UserTrimming = OPAMP_TRIMMING_FACTORY;
  if (HAL_OPAMP_Init(&hopamp2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN OPAMP2_Init 2 */

  /* USER CODE END OPAMP2_Init 2 */

}

/**
  * @brief OPAMP3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_OPAMP3_Init(void)
{

  /* USER CODE BEGIN OPAMP3_Init 0 */

  /* USER CODE END OPAMP3_Init 0 */

  /* USER CODE BEGIN OPAMP3_Init 1 */

  /* USER CODE END OPAMP3_Init 1 */
  hopamp3.Instance = OPAMP3;
  hopamp3.Init.Mode = OPAMP_FOLLOWER_MODE;
  hopamp3.Init.NonInvertingInput = OPAMP_NONINVERTINGINPUT_IO1;
  hopamp3.Init.TimerControlledMuxmode = OPAMP_TIMERCONTROLLEDMUXMODE_DISABLE;
  hopamp3.Init.UserTrimming = OPAMP_TRIMMING_FACTORY;
  if (HAL_OPAMP_Init(&hopamp3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN OPAMP3_Init 2 */

  /* USER CODE END OPAMP3_Init 2 */

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 0;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 999;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_OC_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_TIMING;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_OC_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();
  __HAL_RCC_DMA2_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel3_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel3_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel3_IRQn);
  /* DMA2_Channel5_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Channel5_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA2_Channel5_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LED_STATUS_GPIO_Port, LED_STATUS_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(STATUS_LED_GPIO_Port, STATUS_LED_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LED_STATUS_Pin */
  GPIO_InitStruct.Pin = LED_STATUS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;
  HAL_GPIO_Init(LED_STATUS_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : STATUS_LED_Pin */
  GPIO_InitStruct.Pin = STATUS_LED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;
  HAL_GPIO_Init(STATUS_LED_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc == &hadc3)
    {
        effects_task_record_buffer_event_from_isr(EFFECTS_BUFFER_EVENT_ADC_A);
        notify_task_from_isr(effectsTaskHandle);
    }
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc == &hadc3)
    {
        effects_task_record_buffer_event_from_isr(EFFECTS_BUFFER_EVENT_ADC_B);
        notify_task_from_isr(effectsTaskHandle);
    }
}

void HAL_DAC_ConvHalfCpltCallbackCh1(DAC_HandleTypeDef *hdac)
{
    if (hdac == &hdac1)
    {
        effects_task_record_buffer_event_from_isr(EFFECTS_BUFFER_EVENT_DAC_A);
        notify_task_from_isr(effectsTaskHandle);
    }
}

void HAL_DAC_ConvCpltCallbackCh1(DAC_HandleTypeDef *hdac)
{
    if (hdac == &hdac1)
    {
        effects_task_record_buffer_event_from_isr(EFFECTS_BUFFER_EVENT_DAC_B);
        notify_task_from_isr(effectsTaskHandle);
    }
}

void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance == I2C1)
    {
        peripheral_on_i2c_tx_complete(&peripheral_dispatch_context, &peripheral_dispatch_ops, hi2c);
    }
}

void HAL_I2C_MasterRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance == I2C1)
    {
        peripheral_on_i2c_rx_complete(&peripheral_dispatch_context, &peripheral_dispatch_ops, hi2c);
    }
}

void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance == I2C1)
    {
        peripheral_on_i2c_error(&peripheral_dispatch_context, &peripheral_dispatch_ops, hi2c);
    }
}

void HAL_I2C_AbortCpltCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance == I2C1)
    {
        peripheral_on_i2c_abort_complete(&peripheral_dispatch_context, &peripheral_dispatch_ops, hi2c);
    }
}

/* USER CODE END 4 */

/* USER CODE BEGIN Header_StartDefaultTask */
/**
 * @brief  Function implementing the defaultTask thread.
 * @param  argument: Not used
 * @retval None
 */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void const * argument)
{
  /* init code for USB_DEVICE */
  MX_USB_DEVICE_Init();
  /* USER CODE BEGIN 5 */
  (void)argument;

  for (;;)
  {
      HAL_GPIO_TogglePin(LED_STATUS_GPIO_Port, LED_STATUS_Pin);
      osDelay(read_heartbeat_period_ms());
  }
  /* USER CODE END 5 */
}

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM1 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM1)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
    }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
