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

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define SAMPLE_BUF_LEN 405U
#define ADC_BUF_LEN (2U * SAMPLE_BUF_LEN)
#define DAC_BUF_LEN ADC_BUF_LEN
#define NUM_DELAY_SAMPLES MAX_ECHO_DELAY_SAMPLES
#define HEARTBEAT_PERIOD_MS_DEFAULT 500U
#define HEARTBEAT_PERIOD_MS_MIN 50U
#define HEARTBEAT_PERIOD_MS_MAX 5000U
#define EFFECTS_RUNTIME_LOG_EVERY_FAILURES 10U

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc3;
DMA_HandleTypeDef hdma_adc3;

DAC_HandleTypeDef hdac1;
DMA_HandleTypeDef hdma_dac1_ch1;

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
static volatile uint16_t delay_samples_buf[NUM_DELAY_SAMPLES] = {0};

static volatile uint16_t *const adc_buf_a = adc_buf;
static volatile uint16_t *const adc_buf_b = &adc_buf[SAMPLE_BUF_LEN];
static volatile uint16_t *const dac_buf_a = dac_buf;
static volatile uint16_t *const dac_buf_b = &dac_buf[SAMPLE_BUF_LEN];
static volatile uint32_t effects_event_count_adc_a = 0U;
static volatile uint32_t effects_event_count_adc_b = 0U;
static volatile uint32_t effects_event_count_dac_a = 0U;
static volatile uint32_t effects_event_count_dac_b = 0U;
static volatile uint32_t heartbeat_period_ms = HEARTBEAT_PERIOD_MS_DEFAULT;
static uint32_t effects_frames_ok = 0U;
static uint32_t effects_frames_failed = 0U;

static CliServiceAdapter cli_service_adapter_context;
static CliServices cli_services;
static CliSession cli_session;
static TestVectorSource test_vector_source;
static TestVectorSource test_vector_source_output;

static volatile bool b1_pressed = false;

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
void StartDefaultTask(void const * argument);

/* USER CODE BEGIN PFP */

static uint32_t effects_task_ms_to_ticks(uint32_t ms, void *context);
static bool effects_task_wait_for_adc_buffer(uint32_t timeout_ticks, uint16_t **buf_ptr,
                                             void *context);
static bool effects_task_wait_for_dac_buffer(uint32_t timeout_ticks, uint16_t **buf_ptr,
                                             void *context);
static bool effects_task_dma_copy(const uint16_t *src, uint16_t *dst, size_t count,
                                  uint32_t timeout_ticks, void *context);
static void *effects_task_alloc(size_t size, void *context);
static void effects_task_free(void *ptr, void *context);
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
static bool cli_uart_write(const char *text, void *context);
static void cli_service_lock(void *context);
static void cli_service_unlock(void *context);
static void cli_uart_recover_rx_errors(void);
static void cli_system_reboot(void *context);
static void init_cli_service_adapter(void);
static void init_effects_defaults(void);
static uint32_t compute_sampling_period_us(void);
static uint32_t read_heartbeat_period_ms(void);
void startEffectsTask(void const *argument);
void startCliTask(void const *argument);

static void effects_task_on_frame_begin(void *context);

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

static void *effects_task_alloc(size_t size, void *context)
{
    (void)context;
    return pvPortMalloc(size);
}

static void effects_task_free(void *ptr, void *context)
{
    (void)context;
    if (ptr != NULL)
    {
        vPortFree(ptr);
    }
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

    if (!status.enabled)
    {
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
    HAL_GPIO_TogglePin(LED_STATUS_GPIO_Port, LED_STATUS_Pin);

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
        break;
    case EFFECTS_BUFFER_EVENT_ADC_B:
        effects_event_count_adc_b++;
        break;
    case EFFECTS_BUFFER_EVENT_DAC_A:
        effects_event_count_dac_a++;
        break;
    case EFFECTS_BUFFER_EVENT_DAC_B:
        effects_event_count_dac_b++;
        break;
    default:
        break;
    }

    taskEXIT_CRITICAL_FROM_ISR(saved_interrupt_mask);
}

static bool cli_uart_write(const char *text, void *context)
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

    return HAL_UART_Transmit(&huart2, (uint8_t *)text, (uint16_t)length, 25) == HAL_OK;
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
        .rom_save_state = NULL,
        .rom_load_state = NULL,
        .rom_read_raw = NULL,
        .rom_write_raw = NULL,
        .log_set_level = NULL,
        .log_set_enabled = NULL,
        .log_set_stream = NULL,
        .system_reboot = cli_system_reboot,
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
}

static void init_effects_defaults(void)
{
    taskENTER_CRITICAL();

    (void)memset((void *)&effects_state, 0, sizeof(effects_state));
    (void)memset((void *)&effects_params, 0, sizeof(effects_params));

    effects_state_set_default_order((EffectsState *)&effects_state);
    effects_state.activeEffectSelection = 0;
    effects_state.isEnabled[OVERDRIVE] = true;
    effects_state.isEnabled[ECHO] = false;
    effects_state.isEnabled[COMPRESSION] = false;

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

    effects_params.overdriveMin.gain = 0;
    effects_params.overdriveMin.level = 0;
    effects_params.overdriveMin.tone = MIN_OVERDRIVE_TONE;
    effects_params.overdriveMin.mix = 0;

    effects_params.overdriveMax.gain = MAX_OVERDRIVE_GAIN;
    effects_params.overdriveMax.level = MAX_OVERDRIVE_LEVEL;
    effects_params.overdriveMax.tone = MAX_OVERDRIVE_TONE;
    effects_params.overdriveMax.mix = MAX_OVERDRIVE_MIX;

    effects_params.echoMin.delay_samples = 0;
    effects_params.echoMin.pre_delay = MIN_ECHO_PRE_DELAY;
    effects_params.echoMin.density = 0;
    effects_params.echoMin.attack = 0;
    effects_params.echoMin.decay = 0;

    effects_params.echoMax.delay_samples = MAX_ECHO_DELAY_SAMPLES;
    effects_params.echoMax.pre_delay = MAX_ECHO_PRE_DELAY;
    effects_params.echoMax.density = MAX_ECHO_DENSITY;
    effects_params.echoMax.attack = MAX_ECHO_ATTACK;
    effects_params.echoMax.decay = MAX_ECHO_DECAY;

    effects_params.compressionMin.threshold = 0;
    effects_params.compressionMin.ratio = 0;
    effects_params.compressionMax.threshold = MAX_COMPRESSION_THRESHOLD;
    effects_params.compressionMax.ratio = MAX_COMPRESSION_RATIO;

    taskEXIT_CRITICAL();
}

static uint32_t compute_sampling_period_us(void)
{
    const uint32_t timer_clock_hz = HAL_RCC_GetHCLKFreq();
    const uint32_t prescaler = (uint32_t)(htim2.Instance->PSC) + 1U;
    const uint32_t period = (uint32_t)(htim2.Instance->ARR) + 1U;
    const uint32_t sample_rate_hz = timer_clock_hz / (prescaler * period);

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

    EffectsTaskContext task_context = {
        .pipeline = &effects_pipeline,
        .effectsState = &effects_state,
        .effectsParams = &effects_params,
        .adcBufA = (uint16_t *)adc_buf_a,
        .adcBufB = (uint16_t *)adc_buf_b,
        .dacBufA = (uint16_t *)dac_buf_a,
        .dacBufB = (uint16_t *)dac_buf_b,
        .delaySamplesBuf = (uint16_t *)delay_samples_buf,
        .sampleBufLen = SAMPLE_BUF_LEN,
        .delaySamplesLen = NUM_DELAY_SAMPLES,
        .samplingPeriodUs = compute_sampling_period_us(),
        .processingSlackMs = 2,
    };

    test_vector_source_init(&test_vector_source,
                            task_context.samplingPeriodUs == 0U
                                ? 40500U
                                : (1000000U / task_context.samplingPeriodUs));
    test_vector_source_init(&test_vector_source_output,
                            task_context.samplingPeriodUs == 0U
                                ? 40500U
                                : (1000000U / task_context.samplingPeriodUs));

    EffectsTaskOps task_ops = {
        .wait_for_adc_buffer = effects_task_wait_for_adc_buffer,
        .wait_for_dac_buffer = effects_task_wait_for_dac_buffer,
        .dma_copy = effects_task_dma_copy,
        .alloc = effects_task_alloc,
        .free = effects_task_free,
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

    cli_service_adapter_bind(&cli_service_adapter_context, &cli_services);
    CliSessionTransport transport = {
        .write = cli_uart_write,
        .context = NULL,
    };
    cli_session_init(&cli_session, &cli_services, &transport);
    (void)cli_session_start(&cli_session);

    for (;;)
    {
        uint8_t byte = 0;
        while (HAL_UART_Receive(&huart2, &byte, 1, 0) == HAL_OK)
        {
            cli_session_push_bytes(&cli_session, &byte, 1);
        }

        cli_uart_recover_rx_errors();
        cli_session_poll(&cli_session);

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
    adapter->testInput.enabled = !adapter->testInput.enabled;
    taskEXIT_CRITICAL();
}

static void cli_system_reboot(void *context)
{
    (void)context;
    NVIC_SystemReset();
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
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* definition and creation of defaultTask */
  osThreadDef(defaultTask, StartDefaultTask, osPriorityNormal, 0, 128);
  defaultTaskHandle = osThreadCreate(osThread(defaultTask), NULL);

  /* USER CODE BEGIN RTOS_THREADS */
  boot_log_line("boot: create effects task");
  osThreadDef(effectsTask, startEffectsTask, osPriorityRealtime, 0, 384);
  effectsTaskHandle = osThreadCreate(osThread(effectsTask), NULL);
  if (effectsTaskHandle == NULL)
  {
      boot_log_line("boot: effects task create failed");
      Error_Handler();
  }

  boot_log_line("boot: create cli task");

  osThreadDef(cliTask, startCliTask, osPriorityNormal, 0, 256);
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
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
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
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART2|RCC_PERIPHCLK_ADC34
                              |RCC_PERIPHCLK_TIM2;
  PeriphClkInit.Usart2ClockSelection = RCC_USART2CLKSOURCE_PCLK1;
  PeriphClkInit.Adc34ClockSelection = RCC_ADC34PLLCLK_DIV1;
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
  hadc3.Init.ContinuousConvMode = ENABLE;
  hadc3.Init.DiscontinuousConvMode = DISABLE;
  hadc3.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc3.Init.ExternalTrigConv = ADC_SOFTWARE_START;
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
  htim2.Init.Period = 1184;
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

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LED_STATUS_Pin */
  GPIO_InitStruct.Pin = LED_STATUS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_STATUS_GPIO_Port, &GPIO_InitStruct);

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
