/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
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
#include <string.h>
#include "button_task.h"
#include "effects_control_logic.h"
#include "effects_state_manager.h"
#include "effects_pipeline.h"
#include "effects_task.h"
#include "i2c_handler.h"
#include "i2c_task_support.h"
#include "peripheral_dispatch.h"
#include "pot_task.h"
#include "rom_task_support.h"
#include "switch_task.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* Data convention definitions */
#ifndef X_AXIS
#define X_AXIS (UINT16_MAX / 2)	// x-axis, the median representable signal using uint16
#endif

#ifndef FIXED_POINT_Q
#define FIXED_POINT_Q 8	// N in QN for fixed-point numbers
#endif

/* Buffer size definitions */
#define SAMPLE_BUF_LEN 405								// One buffer of samples (same size for both ADC and DAC
#define ADC_BUF_LEN (2 * SAMPLE_BUF_LEN)	// Ping-pong buffer (circular buffer of 2 buffers)
#define DAC_BUF_LEN ADC_BUF_LEN						// Each ADC buffer has a corresponding DAC buffer for processed samples

#define NUM_DELAY_SAMPLES MAX_ECHO_DELAY_SAMPLES	// Number of extra delayed samples for echo

/* I2C Device Addresses */
#define DISPLAY_I2C_ADDR 0x3C
#define ROM_I2C_ADDR 0xA0

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
ADC_HandleTypeDef hadc3;
DMA_HandleTypeDef hdma_adc3;

DAC_HandleTypeDef hdac1;
DMA_HandleTypeDef hdma_dac1_ch1;

I2C_HandleTypeDef hi2c1;

OPAMP_HandleTypeDef hopamp2;
OPAMP_HandleTypeDef hopamp3;
OPAMP_HandleTypeDef hopamp4;

TIM_HandleTypeDef htim2;

PCD_HandleTypeDef hpcd_USB_FS;

DMA_HandleTypeDef hdma_memtomem_dma1_channel1;
osThreadId effectsTaskHandle;
uint32_t effectsTaskBuffer[ 128 ];
osStaticThreadDef_t effectsTaskControlBlock;
osThreadId btnHandlerTaskHandle;
uint32_t btnHandlerTaskBuffer[ 128 ];
osStaticThreadDef_t btnHandlerTaskControlBlock;
osThreadId swHandlerTaskHandle;
uint32_t swHandlerTaskBuffer[ 128 ];
osStaticThreadDef_t swHandlerTaskControlBlock;
osThreadId potHandlerTaskHandle;
uint32_t potHandlerTaskBuffer[ 128 ];
osStaticThreadDef_t potHandlerTaskControlBlock;
osThreadId ledTaskHandle;
uint32_t ledTaskBuffer[ 128 ];
osStaticThreadDef_t ledTaskControlBlock;
osThreadId i2cHandlerTaskHandle;
uint32_t i2cHandlerTaskBuffer[ 128 ];
osStaticThreadDef_t i2cHandlerTaskControlBlock;
osThreadId romHandlerTaskHandle;
uint32_t romHandlerTaskBuffer[ 128 ];
osStaticThreadDef_t romHandlerTaskControlBlock;
osThreadId displayHandlerTaskHandle;
uint32_t displayHandlerTaskBuffer[ 128 ];
osStaticThreadDef_t displayHandlerTaskControlBlock;
osMessageQId i2cQueueHandle;
uint8_t i2cQueueBuffer[ 16 * sizeof( I2CHandlerMessage ) ];
osStaticMessageQDef_t i2cQueueControlBlock;
osSemaphoreId delaySamplesDmaSemaphoreHandle;
osSemaphoreId i2cCompletionSemaphoreHandle;
osSemaphoreId i2cFailedRomSemaphoreHandle;
osSemaphoreId i2cFailedDisplaySemaphoreHandle;
/* USER CODE BEGIN PV */

/* Structure to keep track of the effects group configuration state */
static volatile EffectsState effectsState;

/* Structure to manage individual effects' configurations (including min/max values) */
static volatile EffectsParams effectsParams;

/* Set by I2C ISR callbacks and consumed by i2c handler task. */
static volatile bool i2cTransferFailed = true;

static I2CTaskSupportContext i2cTaskSupportContext;
static I2CHandlerConfig i2cHandlerConfig;
static I2CHandlerOps i2cHandlerOps;
static PeripheralDispatchContext peripheralDispatchContext;
static PeripheralDispatchOps peripheralDispatchOps;
static RomTaskSupportConfig romTaskSupportConfig;
static RomTaskSupportOps romTaskSupportOps;

/* DMA buffer for ADC samples (note: samples are in order of decreasing age) */
static volatile uint16_t adcBuf[ADC_BUF_LEN] = {0};

/* Pointers to each half of ADC/DMA buffer for ping-pong buffering
		(note: DMA sees one buffer, alternating between the two) */
static volatile uint16_t* const adcBufA = adcBuf;
static volatile uint16_t* const adcBufB = &adcBuf[SAMPLE_BUF_LEN];

/* Extra buffer for carrying over enough past samples for echo.
		Note: This is constantly filled with last NUM_DELAY_SAMPLES samples, regardless of echo configuration */
static volatile uint16_t delaySamplesBuf[NUM_DELAY_SAMPLES] = {0};

/* Buffer for filter outputs (note: samples are in order of decreasing age) */
static volatile uint16_t dacBuf[DAC_BUF_LEN] = {0};

/* Pointers to each half of the output buffer, either buffer may be used at a time */
static volatile uint16_t* const dacBufA = dacBuf;
static volatile uint16_t* const dacBufB = &dacBuf[DAC_BUF_LEN / 2];

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_DAC1_Init(void);
static void MX_OPAMP2_Init(void);
static void MX_TIM2_Init(void);
static void MX_USB_PCD_Init(void);
static void MX_ADC1_Init(void);
static void MX_OPAMP3_Init(void);
static void MX_I2C1_Init(void);
static void MX_OPAMP4_Init(void);
static void MX_ADC3_Init(void);
void startEffectsTask(void const * argument);
void startBtnHandlerTask(void const * argument);
void startSwHandlerTask(void const * argument);
void startPotHandlerTask(void const * argument);
void startLedTask(void const * argument);
void startI2cHandlerTask(void const * argument);
void startRomHandlerTask(void const * argument);
void startDisplayHandlerTask(void const * argument);

/* USER CODE BEGIN PFP */
static bool peripheral_semaphore_take(void *semaphoreHandle, uint32_t timeoutTicks, void *context);
static void peripheral_task_notify_from_isr(void *taskHandle, uint32_t value, void *context);
static void peripheral_semaphore_give_from_isr(void *semaphoreHandle, void *context);
static bool peripheral_dma_start_it(void *dmaHandle, uint32_t srcAddress, uint32_t dstAddress,
  uint32_t dataLength, void *context);
static bool peripheral_adc_config_channel(void *adcHandle, void *config, void *context);
static bool peripheral_adc_start_it(void *adcHandle, void *context);
static uint32_t peripheral_adc_get_value(void *adcHandle, void *context);
static void peripheral_i2c_signal_completion_from_isr(void *i2cContext, bool transferFailed, void *context);

static bool effects_task_wait_for_adc_buffer(uint32_t timeoutTicks, uint16_t **bufPtr, void *context);
static bool effects_task_wait_for_dac_buffer(uint32_t timeoutTicks, uint16_t **bufPtr, void *context);
static bool effects_task_dma_copy(const uint16_t *src, uint16_t *dst, size_t count, uint32_t timeoutTicks,
  void *context);
static bool effects_task_read_latched_state(EffectsState *state, EffectsParams *params, void *context);
static uint32_t effects_task_ms_to_ticks(uint32_t ms, void *context);

static bool pot_task_start_adc(uint8_t potIndex, void *context);
static bool pot_task_wait_for_sample(uint32_t timeoutTicks, uint32_t *valueOut, void *context);
static bool pot_task_read_active_effect(Effect *activeEffect, void *context);
static void pot_task_apply_sample(Effect activeEffect, uint8_t potIndex, uint32_t adcValue,
  uint32_t adcMax, void *context);

static bool switch_task_read_switch(uint8_t index, bool *enabledOut, void *context);
static bool switch_task_read_state(EffectsState *stateOut, void *context);
static bool switch_task_write_state(const EffectsState *state, void *context);
static void switch_task_sleep_ms(uint32_t ms, void *context);

static bool button_task_wait_for_button(uint16_t *pinOut, void *context);
static void button_task_dispatch(uint16_t pin, void *context);

static uint8_t *allocate_payload_adapter(size_t size, void *context);
static void free_payload_adapter(uint8_t *payload, void *context);
static void set_rom_write_disable_adapter(bool disableWrites, void *context);
static void init_peripheral_dispatch(void);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

static bool peripheral_semaphore_take(void *semaphoreHandle, uint32_t timeoutTicks, void *context)
{
  (void) context;
  return xSemaphoreTake((osSemaphoreId) semaphoreHandle, (TickType_t) timeoutTicks) == pdTRUE;
}

static void peripheral_task_notify_from_isr(void *taskHandle, uint32_t value, void *context)
{
  (void) context;

  if (taskHandle == NULL)
  {
    return;
  }

  BaseType_t higherPriorityTaskWoken = pdFALSE;
  xTaskNotifyFromISR((osThreadId) taskHandle, value, eSetValueWithOverwrite, &higherPriorityTaskWoken);
  portYIELD_FROM_ISR(higherPriorityTaskWoken);
}

static void peripheral_semaphore_give_from_isr(void *semaphoreHandle, void *context)
{
  (void) context;

  if (semaphoreHandle == NULL)
  {
    return;
  }

  BaseType_t higherPriorityTaskWoken = pdFALSE;
  xSemaphoreGiveFromISR((osSemaphoreId) semaphoreHandle, &higherPriorityTaskWoken);
  portYIELD_FROM_ISR(higherPriorityTaskWoken);
}

static bool peripheral_dma_start_it(void *dmaHandle, uint32_t srcAddress, uint32_t dstAddress,
    uint32_t dataLength, void *context)
{
  (void) context;

  if (dmaHandle == NULL)
  {
    return false;
  }

  return HAL_DMA_Start_IT((DMA_HandleTypeDef *) dmaHandle, srcAddress, dstAddress, dataLength) == HAL_OK;
}

static bool peripheral_adc_config_channel(void *adcHandle, void *config, void *context)
{
  (void) context;

  if (adcHandle == NULL || config == NULL)
  {
    return false;
  }

  return HAL_ADC_ConfigChannel((ADC_HandleTypeDef *) adcHandle,
      (ADC_ChannelConfTypeDef *) config) == HAL_OK;
}

static bool peripheral_adc_start_it(void *adcHandle, void *context)
{
  (void) context;

  if (adcHandle == NULL)
  {
    return false;
  }

  return HAL_ADC_Start_IT((ADC_HandleTypeDef *) adcHandle) == HAL_OK;
}

static uint32_t peripheral_adc_get_value(void *adcHandle, void *context)
{
  (void) context;

  if (adcHandle == NULL)
  {
    return 0;
  }

  return HAL_ADC_GetValue((ADC_HandleTypeDef *) adcHandle);
}

static void peripheral_i2c_signal_completion_from_isr(void *i2cContext, bool transferFailed, void *context)
{
  (void) context;
  i2c_task_signal_completion_from_isr((const I2CTaskSupportContext *) i2cContext, transferFailed);
}

static bool effects_task_wait_for_adc_buffer(uint32_t timeoutTicks, uint16_t **bufPtr, void *context)
{
  (void) context;

  if (bufPtr == NULL)
  {
    return false;
  }

  uint32_t notificationValue;
  if (xTaskNotifyWait(0, 0xFFFFFFFFUL, &notificationValue, (TickType_t) timeoutTicks) != pdTRUE)
  {
    return false;
  }

  *bufPtr = (uint16_t *) notificationValue;
  return true;
}

static bool effects_task_wait_for_dac_buffer(uint32_t timeoutTicks, uint16_t **bufPtr, void *context)
{
  return effects_task_wait_for_adc_buffer(timeoutTicks, bufPtr, context);
}

static bool effects_task_dma_copy(const uint16_t *src, uint16_t *dst, size_t count, uint32_t timeoutTicks,
    void *context)
{
  (void) context;

  return peripheral_start_dma_transfer_and_wait(&hdma_memtomem_dma1_channel1,
      (uint32_t) src,
      (uint32_t) dst,
      count,
      delaySamplesDmaSemaphoreHandle,
      timeoutTicks,
      &peripheralDispatchOps);
}

static bool effects_task_read_latched_state(EffectsState *state, EffectsParams *params, void *context)
{
  (void) context;

  if (state == NULL || params == NULL)
  {
    return false;
  }

  taskENTER_CRITICAL();
  *state = effectsState;
  *params = effectsParams;
  taskEXIT_CRITICAL();
  return true;
}

static uint32_t effects_task_ms_to_ticks(uint32_t ms, void *context)
{
  (void) context;
  return pdMS_TO_TICKS(ms);
}

static bool pot_task_start_adc(uint8_t potIndex, void *context)
{
  (void) context;

  ADC_ChannelConfTypeDef sConfig;
  sConfig.Channel = ADC_CHANNEL_1;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;

  switch (potIndex)
  {
    case 0: sConfig.Channel = ADC_CHANNEL_1; break;
    case 1: sConfig.Channel = ADC_CHANNEL_2; break;
    case 2: sConfig.Channel = ADC_CHANNEL_4; break;
    case 3: sConfig.Channel = ADC_CHANNEL_6; break;
    default: return false;
  }

  return peripheral_start_adc_conversion_it(&hadc1, &sConfig, &peripheralDispatchOps);
}

static bool pot_task_wait_for_sample(uint32_t timeoutTicks, uint32_t *valueOut, void *context)
{
  (void) context;

  if (valueOut == NULL)
  {
    return false;
  }

  return xTaskNotifyWait(0, 0xFFFFFFFFUL, valueOut, (TickType_t) timeoutTicks) == pdTRUE;
}

static bool pot_task_read_active_effect(Effect *activeEffect, void *context)
{
  (void) context;

  if (activeEffect == NULL)
  {
    return false;
  }

  EffectsState latchedEffectsState;

  taskENTER_CRITICAL();
  latchedEffectsState = effectsState;
  taskEXIT_CRITICAL();

  effects_state_normalize(&latchedEffectsState);

  return effects_state_get_active_effect(&latchedEffectsState, activeEffect);
}

static void pot_task_apply_sample(Effect activeEffect, uint8_t potIndex, uint32_t adcValue,
    uint32_t adcMax, void *context)
{
  (void) context;

  taskENTER_CRITICAL();
  (void)effects_params_apply_pot_sample((EffectsParams *)&effectsParams, activeEffect, potIndex,
      adcValue, adcMax);
  taskEXIT_CRITICAL();
}

static bool switch_task_read_switch(uint8_t index, bool *enabledOut, void *context)
{
  (void) context;

  if (enabledOut == NULL)
  {
    return false;
  }

  uint16_t pin = 0;
  GPIO_TypeDef *port = NULL;

  switch (index)
  {
    case 0: port = SWITCH_A_GPIO_Port; pin = SWITCH_A_Pin; break;
    case 1: port = SWITCH_B_GPIO_Port; pin = SWITCH_B_Pin; break;
    case 2: port = SWITCH_C_GPIO_Port; pin = SWITCH_C_Pin; break;
    default: return false;
  }

  *enabledOut = HAL_GPIO_ReadPin(port, pin) == GPIO_PIN_SET;
  return true;
}

static bool switch_task_read_state(EffectsState *stateOut, void *context)
{
  (void) context;

  if (stateOut == NULL)
  {
    return false;
  }

  taskENTER_CRITICAL();
  *stateOut = effectsState;
  taskEXIT_CRITICAL();

  return true;
}

static bool switch_task_write_state(const EffectsState *state, void *context)
{
  (void) context;

  if (state == NULL)
  {
    return false;
  }

  taskENTER_CRITICAL();
  effectsState = *state;
  taskEXIT_CRITICAL();

  return true;
}

static void switch_task_sleep_ms(uint32_t ms, void *context)
{
  (void) context;
  osDelay(ms);
}

static bool button_task_wait_for_button(uint16_t *pinOut, void *context)
{
  (void) context;

  if (pinOut == NULL)
  {
    return false;
  }

  uint32_t notificationValue;
  if (xTaskNotifyWait(0, 0xFFFFFFFFUL, &notificationValue, portMAX_DELAY) != pdTRUE)
  {
    return false;
  }

  *pinOut = (uint16_t)notificationValue;
  return true;
}

static void button_task_dispatch(uint16_t pin, void *context)
{
  (void) context;

  switch (pin)
  {
    case BTN_A_Pin:
    {
    } break;
    case BTN_B_Pin:
    {
    } break;
    case BTN_C_Pin:
    {
    } break;
    default: break;
  }
}

static uint8_t *allocate_payload_adapter(size_t size, void *context)
{
  (void) context;

  return (uint8_t *) pvPortMalloc(size);
}

static void free_payload_adapter(uint8_t *payload, void *context)
{
  (void) context;

  if (payload != NULL)
  {
    vPortFree(payload);
  }
}

static void set_rom_write_disable_adapter(bool disableWrites, void *context)
{
  (void) context;

  HAL_GPIO_WritePin(nNVM_WE_GPIO_Port, nNVM_WE_Pin,
      disableWrites ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static void init_peripheral_dispatch(void)
{
  peripheralDispatchContext.effectsTaskHandle = effectsTaskHandle;
  peripheralDispatchContext.potHandlerTaskHandle = potHandlerTaskHandle;
  peripheralDispatchContext.btnHandlerTaskHandle = btnHandlerTaskHandle;
  peripheralDispatchContext.effectsAdcHandle = &hadc3;
  peripheralDispatchContext.potAdcHandle = &hadc1;
  peripheralDispatchContext.i2cHandle = &hi2c1;
  peripheralDispatchContext.memToMemDmaHandle = &hdma_memtomem_dma1_channel1;
  peripheralDispatchContext.i2cTaskSupportContext = &i2cTaskSupportContext;
  peripheralDispatchContext.delaySamplesDmaSemaphoreHandle = delaySamplesDmaSemaphoreHandle;
  peripheralDispatchContext.adcBufA = (void *) adcBufA;
  peripheralDispatchContext.adcBufB = (void *) adcBufB;
  peripheralDispatchContext.dacBufA = (void *) dacBufA;
  peripheralDispatchContext.dacBufB = (void *) dacBufB;

  peripheralDispatchOps.semaphore_take = peripheral_semaphore_take;
  peripheralDispatchOps.task_notify_from_isr = peripheral_task_notify_from_isr;
  peripheralDispatchOps.semaphore_give_from_isr = peripheral_semaphore_give_from_isr;
  peripheralDispatchOps.dma_start_it = peripheral_dma_start_it;
  peripheralDispatchOps.adc_config_channel = peripheral_adc_config_channel;
  peripheralDispatchOps.adc_start_it = peripheral_adc_start_it;
  peripheralDispatchOps.adc_get_value = peripheral_adc_get_value;
  peripheralDispatchOps.i2c_signal_completion_from_isr = peripheral_i2c_signal_completion_from_isr;
  peripheralDispatchOps.context = NULL;
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
  MX_DAC1_Init();
  MX_OPAMP2_Init();
  MX_TIM2_Init();
  MX_USB_PCD_Init();
  MX_ADC1_Init();
  MX_OPAMP3_Init();
  MX_I2C1_Init();
  MX_OPAMP4_Init();
  MX_ADC3_Init();
  /* USER CODE BEGIN 2 */

  /* USER CODE END 2 */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* Create the semaphores(s) */
  /* definition and creation of delaySamplesDmaSemaphore */
  osSemaphoreDef(delaySamplesDmaSemaphore);
  delaySamplesDmaSemaphoreHandle = osSemaphoreCreate(osSemaphore(delaySamplesDmaSemaphore), 1);

  /* definition and creation of i2cCompletionSemaphore */
  osSemaphoreDef(i2cCompletionSemaphore);
  i2cCompletionSemaphoreHandle = osSemaphoreCreate(osSemaphore(i2cCompletionSemaphore), 1);

  /* definition and creation of i2cFailedRomSemaphore */
  osSemaphoreDef(i2cFailedRomSemaphore);
  i2cFailedRomSemaphoreHandle = osSemaphoreCreate(osSemaphore(i2cFailedRomSemaphore), 1);

  /* definition and creation of i2cFailedDisplaySemaphore */
  osSemaphoreDef(i2cFailedDisplaySemaphore);
  i2cFailedDisplaySemaphoreHandle = osSemaphoreCreate(osSemaphore(i2cFailedDisplaySemaphore), 1);

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  PeripheralDispatchOps drainOps = {
      .semaphore_take = peripheral_semaphore_take,
      .task_notify_from_isr = NULL,
      .semaphore_give_from_isr = NULL,
      .dma_start_it = NULL,
      .adc_config_channel = NULL,
      .adc_start_it = NULL,
      .adc_get_value = NULL,
      .i2c_signal_completion_from_isr = NULL,
      .context = NULL,
  };
  peripheral_drain_semaphore(delaySamplesDmaSemaphoreHandle, &drainOps);
  peripheral_drain_semaphore(i2cCompletionSemaphoreHandle, &drainOps);
  peripheral_drain_semaphore(i2cFailedRomSemaphoreHandle, &drainOps);
  peripheral_drain_semaphore(i2cFailedDisplaySemaphoreHandle, &drainOps);
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* Create the queue(s) */
  /* definition and creation of i2cQueue */
  osMessageQStaticDef(i2cQueue, 16, I2CHandlerMessage, i2cQueueBuffer, &i2cQueueControlBlock);
  i2cQueueHandle = osMessageCreate(osMessageQ(i2cQueue), NULL);

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* definition and creation of effectsTask */
  osThreadStaticDef(effectsTask, startEffectsTask, osPriorityRealtime, 0, 128, effectsTaskBuffer, &effectsTaskControlBlock);
  effectsTaskHandle = osThreadCreate(osThread(effectsTask), NULL);

  /* definition and creation of btnHandlerTask */
  osThreadStaticDef(btnHandlerTask, startBtnHandlerTask, osPriorityNormal, 0, 128, btnHandlerTaskBuffer, &btnHandlerTaskControlBlock);
  btnHandlerTaskHandle = osThreadCreate(osThread(btnHandlerTask), NULL);

  /* definition and creation of swHandlerTask */
  osThreadStaticDef(swHandlerTask, startSwHandlerTask, osPriorityNormal, 0, 128, swHandlerTaskBuffer, &swHandlerTaskControlBlock);
  swHandlerTaskHandle = osThreadCreate(osThread(swHandlerTask), NULL);

  /* definition and creation of potHandlerTask */
  osThreadStaticDef(potHandlerTask, startPotHandlerTask, osPriorityNormal, 0, 128, potHandlerTaskBuffer, &potHandlerTaskControlBlock);
  potHandlerTaskHandle = osThreadCreate(osThread(potHandlerTask), NULL);

  /* definition and creation of ledTask */
  osThreadStaticDef(ledTask, startLedTask, osPriorityNormal, 0, 128, ledTaskBuffer, &ledTaskControlBlock);
  ledTaskHandle = osThreadCreate(osThread(ledTask), NULL);

  /* definition and creation of i2cHandlerTask */
  osThreadStaticDef(i2cHandlerTask, startI2cHandlerTask, osPriorityNormal, 0, 128, i2cHandlerTaskBuffer, &i2cHandlerTaskControlBlock);
  i2cHandlerTaskHandle = osThreadCreate(osThread(i2cHandlerTask), NULL);

  /* definition and creation of romHandlerTask */
  osThreadStaticDef(romHandlerTask, startRomHandlerTask, osPriorityNormal, 0, 128, romHandlerTaskBuffer, &romHandlerTaskControlBlock);
  romHandlerTaskHandle = osThreadCreate(osThread(romHandlerTask), NULL);

  /* definition and creation of displayHandlerTask */
  osThreadStaticDef(displayHandlerTask, startDisplayHandlerTask, osPriorityNormal, 0, 128, displayHandlerTaskBuffer, &displayHandlerTaskControlBlock);
  displayHandlerTaskHandle = osThreadCreate(osThread(displayHandlerTask), NULL);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
    i2c_task_support_init(&i2cTaskSupportContext, &i2cHandlerConfig, &i2cHandlerOps,
      &hi2c1,
      romHandlerTaskHandle,
      displayHandlerTaskHandle,
      i2cCompletionSemaphoreHandle,
      i2cFailedRomSemaphoreHandle,
      i2cFailedDisplaySemaphoreHandle,
      &i2cTransferFailed);

    init_peripheral_dispatch();

    rom_task_support_init(&romTaskSupportConfig,
      &romTaskSupportOps,
      romHandlerTaskHandle,
      i2cQueueHandle,
      i2cFailedRomSemaphoreHandle,
      (ROM_I2C_ADDR << 1) & ~1U,
      0,
      0,
      pdMS_TO_TICKS(100),
      pdMS_TO_TICKS(5000),
      i2c_task_queue_message_and_wait,
      allocate_payload_adapter,
      free_payload_adapter,
      set_rom_write_disable_adapter,
      NULL);
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
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL4;
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
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USB|RCC_PERIPHCLK_I2C1
                              |RCC_PERIPHCLK_ADC12|RCC_PERIPHCLK_ADC34
                              |RCC_PERIPHCLK_TIM2;
  PeriphClkInit.Adc12ClockSelection = RCC_ADC12PLLCLK_DIV16;
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
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_MultiModeTypeDef multimode = {0};
  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Common config
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV1;
  hadc1.Init.Resolution = ADC_RESOLUTION_8B;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.ContinuousConvMode = ENABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc1.Init.LowPowerAutoWait = DISABLE;
  hadc1.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure the ADC multi-mode
  */
  multimode.Mode = ADC_MODE_INDEPENDENT;
  if (HAL_ADCEx_MultiModeConfigChannel(&hadc1, &multimode) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_1;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

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
  hadc3.Init.DataAlign = ADC_DATAALIGN_RIGHT;
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
  sConfig.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
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
  hopamp2.Init.Mode = OPAMP_STANDALONE_MODE;
  hopamp2.Init.NonInvertingInput = OPAMP_NONINVERTINGINPUT_IO2;
  hopamp2.Init.InvertingInput = OPAMP_INVERTINGINPUT_IO1;
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
  hopamp3.Init.Mode = OPAMP_STANDALONE_MODE;
  hopamp3.Init.NonInvertingInput = OPAMP_NONINVERTINGINPUT_IO1;
  hopamp3.Init.InvertingInput = OPAMP_INVERTINGINPUT_IO1;
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
  * @brief OPAMP4 Initialization Function
  * @param None
  * @retval None
  */
static void MX_OPAMP4_Init(void)
{

  /* USER CODE BEGIN OPAMP4_Init 0 */

  /* USER CODE END OPAMP4_Init 0 */

  /* USER CODE BEGIN OPAMP4_Init 1 */

  /* USER CODE END OPAMP4_Init 1 */
  hopamp4.Instance = OPAMP4;
  hopamp4.Init.Mode = OPAMP_STANDALONE_MODE;
  hopamp4.Init.NonInvertingInput = OPAMP_NONINVERTINGINPUT_IO3;
  hopamp4.Init.InvertingInput = OPAMP_INVERTINGINPUT_IO0;
  hopamp4.Init.TimerControlledMuxmode = OPAMP_TIMERCONTROLLEDMUXMODE_DISABLE;
  hopamp4.Init.UserTrimming = OPAMP_TRIMMING_FACTORY;
  if (HAL_OPAMP_Init(&hopamp4) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN OPAMP4_Init 2 */

  /* USER CODE END OPAMP4_Init 2 */

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
  htim2.Init.Prescaler = 1000-1;
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
  * @brief USB Initialization Function
  * @param None
  * @retval None
  */
static void MX_USB_PCD_Init(void)
{

  /* USER CODE BEGIN USB_Init 0 */

  /* USER CODE END USB_Init 0 */

  /* USER CODE BEGIN USB_Init 1 */

  /* USER CODE END USB_Init 1 */
  hpcd_USB_FS.Instance = USB;
  hpcd_USB_FS.Init.dev_endpoints = 8;
  hpcd_USB_FS.Init.speed = PCD_SPEED_FULL;
  hpcd_USB_FS.Init.phy_itface = PCD_PHY_EMBEDDED;
  hpcd_USB_FS.Init.low_power_enable = DISABLE;
  hpcd_USB_FS.Init.lpm_enable = DISABLE;
  hpcd_USB_FS.Init.battery_charging_enable = DISABLE;
  if (HAL_PCD_Init(&hpcd_USB_FS) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USB_Init 2 */

  /* USER CODE END USB_Init 2 */

}

/**
  * Enable DMA controller clock
  * Configure DMA for memory to memory transfers
  *   hdma_memtomem_dma1_channel1
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();
  __HAL_RCC_DMA2_CLK_ENABLE();

  /* Configure DMA request hdma_memtomem_dma1_channel1 on DMA1_Channel1 */
  hdma_memtomem_dma1_channel1.Instance = DMA1_Channel1;
  hdma_memtomem_dma1_channel1.Init.Direction = DMA_MEMORY_TO_MEMORY;
  hdma_memtomem_dma1_channel1.Init.PeriphInc = DMA_PINC_ENABLE;
  hdma_memtomem_dma1_channel1.Init.MemInc = DMA_MINC_ENABLE;
  hdma_memtomem_dma1_channel1.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
  hdma_memtomem_dma1_channel1.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
  hdma_memtomem_dma1_channel1.Init.Mode = DMA_NORMAL;
  hdma_memtomem_dma1_channel1.Init.Priority = DMA_PRIORITY_LOW;
  if (HAL_DMA_Init(&hdma_memtomem_dma1_channel1) != HAL_OK)
  {
    Error_Handler( );
  }

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
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(nNVM_WE_GPIO_Port, nNVM_WE_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(STATUS_LED_GPIO_Port, STATUS_LED_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LED_STATUS_GPIO_Port, LED_STATUS_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : nNVM_WE_Pin STATUS_LED_Pin */
  GPIO_InitStruct.Pin = nNVM_WE_Pin|STATUS_LED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : SWITCH_A_Pin SWITCH_B_Pin SWITCH_C_Pin */
  GPIO_InitStruct.Pin = SWITCH_A_Pin|SWITCH_B_Pin|SWITCH_C_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : BTN_C_Pin BTN_B_Pin BTN_A_Pin */
  GPIO_InitStruct.Pin = BTN_C_Pin|BTN_B_Pin|BTN_A_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : LED_STATUS_Pin */
  GPIO_InitStruct.Pin = LED_STATUS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_STATUS_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI9_5_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);

  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  peripheral_on_gpio_exti(&peripheralDispatchContext, &peripheralDispatchOps, GPIO_Pin);
}

void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef* hadc)
{
  peripheral_on_adc_half_complete(&peripheralDispatchContext, &peripheralDispatchOps, hadc);
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
  peripheral_on_adc_complete(&peripheralDispatchContext, &peripheralDispatchOps, hadc);
}

void HAL_DAC_ConvHalfCpltCallbackCh1(DAC_HandleTypeDef* hdac)
{
  peripheral_on_dac_half_complete(&peripheralDispatchContext, &peripheralDispatchOps, hdac);
}
	
void HAL_DAC_ConvCpltCallbackCh1(DAC_HandleTypeDef* hdac)
{
  peripheral_on_dac_complete(&peripheralDispatchContext, &peripheralDispatchOps, hdac);
}

void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef *hi2c)
{
  peripheral_on_i2c_tx_complete(&peripheralDispatchContext, &peripheralDispatchOps, hi2c);
}

void HAL_I2C_MasterRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
  peripheral_on_i2c_rx_complete(&peripheralDispatchContext, &peripheralDispatchOps, hi2c);
}

void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c)
{
  peripheral_on_i2c_error(&peripheralDispatchContext, &peripheralDispatchOps, hi2c);
}

void HAL_I2C_AbortCpltCallback(I2C_HandleTypeDef *hi2c)
{
  peripheral_on_i2c_abort_complete(&peripheralDispatchContext, &peripheralDispatchOps, hi2c);
}

void HAL_DMA_CpltCallback(DMA_HandleTypeDef* hdma)
{
  peripheral_on_dma_complete(&peripheralDispatchContext, &peripheralDispatchOps, hdma);
}
/* USER CODE END 4 */

/* USER CODE BEGIN Header_startEffectsTask */
/**
  * @brief  Function implementing the effectsTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_startEffectsTask */
void startEffectsTask(void const * argument)
{
  /* USER CODE BEGIN 5 */
  EffectsPipeline effectsPipeline;
  if (effects_pipeline_init(&effectsPipeline) != 0)
  {
    vTaskDelete(NULL);
    return;
  }

  EffectsTaskContext taskContext = {
      .pipeline = &effectsPipeline,
      .effectsState = &effectsState,
      .effectsParams = &effectsParams,
      .adcBufA = (uint16_t *) adcBufA,
      .adcBufB = (uint16_t *) adcBufB,
      .dacBufA = (uint16_t *) dacBufA,
      .dacBufB = (uint16_t *) dacBufB,
      .delaySamplesBuf = (uint16_t *) delaySamplesBuf,
      .sampleBufLen = SAMPLE_BUF_LEN,
      .delaySamplesLen = NUM_DELAY_SAMPLES,
      .samplingPeriodUs = 25,
      .processingSlackMs = 2,
  };

  EffectsTaskOps taskOps = {
      .wait_for_adc_buffer = effects_task_wait_for_adc_buffer,
      .wait_for_dac_buffer = effects_task_wait_for_dac_buffer,
      .dma_copy = effects_task_dma_copy,
      .alloc = allocate_payload_adapter,
      .free = free_payload_adapter,
      .read_latched_state = effects_task_read_latched_state,
      .report_failure = NULL,
      .ms_to_ticks = effects_task_ms_to_ticks,
      .context = NULL,
  };
	
	/* Start ADC and DAC. Note: ADC expects buffers to be ready to be filled,
			DAC expects buffers to be ready to be read from. Software must ensure this. */
  if (HAL_ADC_Start_DMA(&hadc3, (uint32_t *) adcBuf, ADC_BUF_LEN) != HAL_OK ||
    HAL_DAC_Start_DMA(&hdac1, DAC_CHANNEL_1, (uint32_t *) dacBuf, DAC_BUF_LEN, DAC_ALIGN_12B_R) != HAL_OK ||
    HAL_TIM_OC_Start(&htim2, TIM_CHANNEL_1) != HAL_OK)
  {
    vTaskDelete(NULL);
    return;
  }
	
  /* Infinite loop */
  for(;;)
  {
    (void) effects_task_step(&taskContext, &taskOps);
  }
  /* USER CODE END 5 */
}

/* USER CODE BEGIN Header_startBtnHandlerTask */
/**
* @brief Function implementing the btnHandlerTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_startBtnHandlerTask */
void startBtnHandlerTask(void const * argument)
{
  /* USER CODE BEGIN startBtnHandlerTask */
  ButtonTaskOps taskOps = {
      .wait_for_button = button_task_wait_for_button,
      .dispatch_button = button_task_dispatch,
      .context = NULL,
  };

  /* Infinite loop */
  for(;;)
  {
    (void) button_task_step(&taskOps);
  }
  /* USER CODE END startBtnHandlerTask */
}

/* USER CODE BEGIN Header_startSwHandlerTask */
/**
* @brief Function implementing the swHandlerTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_startSwHandlerTask */
void startSwHandlerTask(void const * argument)
{
  /* USER CODE BEGIN startSwHandlerTask */
	SwitchTaskContext taskContext = {
      .pollingFrequencyMs = 5,
  };

  SwitchTaskOps taskOps = {
      .read_switch = switch_task_read_switch,
      .read_state = switch_task_read_state,
      .write_state = switch_task_write_state,
      .sleep_ms = switch_task_sleep_ms,
      .context = NULL,
  };
	
  /* Infinite loop */
  for(;;)
  {
    (void) switch_task_step(&taskContext, &taskOps);
  }
  /* USER CODE END startSwHandlerTask */
}

/* USER CODE BEGIN Header_startPotHandlerTask */
/**
* @brief Function implementing the potHandlerTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_startPotHandlerTask */
void startPotHandlerTask(void const * argument)
{
  /* USER CODE BEGIN startPotHandlerTask */
	PotTaskContext taskContext = {
      .samplingFrequencyTicks = pdMS_TO_TICKS(5),
      .timeoutSlackTicks = pdMS_TO_TICKS(1),
      .adcMax = UINT8_MAX,
      .potCount = 4,
  };

  PotTaskOps taskOps = {
      .start_adc = pot_task_start_adc,
      .wait_for_sample = pot_task_wait_for_sample,
      .read_active_effect = pot_task_read_active_effect,
      .apply_pot_sample = pot_task_apply_sample,
      .context = NULL,
  };
	
  /* Infinite loop */
  for(;;)
  {
    (void) pot_task_step(&taskContext, &taskOps);
  }
  /* USER CODE END startPotHandlerTask */
}

/* USER CODE BEGIN Header_startLedTask */
/**
* @brief Function implementing the ledTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_startLedTask */
void startLedTask(void const * argument)
{
  /* USER CODE BEGIN startLedTask */
	const uint16_t blinkFrequencyMs = 500;
	
  /* Infinite loop */
  for(;;)
  {
		HAL_GPIO_TogglePin(LED_STATUS_GPIO_Port, LED_STATUS_Pin);
		
    osDelay(blinkFrequencyMs);
  }
  /* USER CODE END startLedTask */
}

/* USER CODE BEGIN Header_startI2cHandlerTask */
/**
* @brief Function implementing the i2cHandlerTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_startI2cHandlerTask */
void startI2cHandlerTask(void const * argument)
{
  /* USER CODE BEGIN startI2cHandlerTask */
  /* Infinite loop */
  for(;;)
  {
		I2CHandlerMessage message;

    if (xQueueReceive(i2cQueueHandle, (void *) &message, portMAX_DELAY) != pdPASS)
    {
      continue;
    }

    (void) i2c_handler_process_message(
        &message,
        &i2cHandlerConfig,
        &i2cHandlerOps,
        &i2cTaskSupportContext);
  }
  /* USER CODE END startI2cHandlerTask */
}

/* USER CODE BEGIN Header_startRomHandlerTask */
/**
* @brief Function implementing the romHandlerTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_startRomHandlerTask */
void startRomHandlerTask(void const * argument)
{
  /* USER CODE BEGIN startRomHandlerTask */
	const uint16_t updateFrequencyMs = 5000;
  const uint16_t bootstrapRetryDelayMs = 10;

  while (true)
  {
    EffectsState loadedState;
    EffectsParams loadedParams;

    if (rom_task_bootstrap_effects(
        &romTaskSupportConfig,
        &romTaskSupportOps,
        &loadedState,
        &loadedParams))
    {
      taskENTER_CRITICAL();
      effectsState = loadedState;
      effectsParams = loadedParams;
      taskEXIT_CRITICAL();
      break;
    }

    osDelay(bootstrapRetryDelayMs);
  }
	
  /* Infinite loop */
  for(;;)
  {
		/* Copy effectsState and effectsParams TO ROM every x seconds */
		taskENTER_CRITICAL();	// Effects configs are accessed by other tasks and this is not atomic
		EffectsState latchedEffectsState = effectsState;
		EffectsParams latchedEffectsParams = effectsParams;
		taskEXIT_CRITICAL();
    effects_state_normalize(&latchedEffectsState);

    if (!rom_task_save_effects(
        &romTaskSupportConfig,
        &romTaskSupportOps,
        &latchedEffectsState,
        &latchedEffectsParams))
    {
      continue;
    }

    osDelay(updateFrequencyMs);
  }
  /* USER CODE END startRomHandlerTask */
}

/* USER CODE BEGIN Header_startDisplayHandlerTask */
/**
* @brief Function implementing the displayHandlerTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_startDisplayHandlerTask */
void startDisplayHandlerTask(void const * argument)
{
  /* USER CODE BEGIN startDisplayHandlerTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END startDisplayHandlerTask */
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
