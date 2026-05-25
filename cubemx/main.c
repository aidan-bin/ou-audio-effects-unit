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
#include "effects.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

#define NUM_EFFECTS 3

typedef enum
{
	OVERDRIVE,
	ECHO,
	COMPRESSION,
} Effect;

typedef struct
{
	Effect ordered[NUM_EFFECTS];		// Active effects in order
	bool isEnabled[NUM_EFFECTS];		// Whether each active effect is enabled (indexable by ordered)
	uint8_t activeEffectSelection;	// Which effect is currently selected (i.e., an index of ordered)
} EffectsState;

typedef struct
{
	/* Effects configurations */
	OverdriveParam overdrive;
	EchoParam echo;
	CompressionParam compression;
	
	/* Effects limits (min and max values for each effect, corresponding to each end of pot) */
	OverdriveParam overdriveMin, overdriveMax;
	EchoParam echoMin, echoMax;
	CompressionParam compressionMin, compressionMax;
} EffectsParams;

typedef struct
{
	osThreadId sourceTask;
	bool rxTxBar;					// Set to true if receive is enabled, false if transmit is enabled
	uint16_t address;			// Receiving device address
	uint8_t *pPayload;		// Pointer to payload buffer
	uint16_t items;				// Number of items in payload buffer
	bool *pFailed;				// Indicates whether message was dropped or not at any point in transmission
} I2CMessage;

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
uint8_t i2cQueueBuffer[ 16 * sizeof( I2CMessage ) ];
osStaticMessageQDef_t i2cQueueControlBlock;
osMessageQId displayCommandQueueHandle;
uint8_t displayCommandQueueBuffer[ 16 * sizeof( uint16_t ) ];
osStaticMessageQDef_t displayCommandQueueControlBlock;
osSemaphoreId delaySamplesDmaSemaphoreHandle;
osStaticSemaphoreDef_t delaySamplesDmaSemaphoreControlBlock;
osSemaphoreId i2cCompletionSemaphoreHandle;
osStaticSemaphoreDef_t i2cCompletionSemaphoreControlBlock;
osSemaphoreId i2cFailedRomSemaphoreHandle;
osStaticSemaphoreDef_t i2cFailedRomSemaphoreControlBlock;
osSemaphoreId i2cFailedDisplaySemaphoreHandle;
osStaticSemaphoreDef_t i2cFailedDisplaySemaphoreControlBlock;
/* USER CODE BEGIN PV */

/* Structure to keep track of the effects group configuration state */
static volatile EffectsState effectsState;

/* Structure to manage individual effects' configurations (including min/max values) */
static volatile EffectsParams effectsParams;

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

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

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
  osSemaphoreStaticDef(delaySamplesDmaSemaphore, &delaySamplesDmaSemaphoreControlBlock);
  delaySamplesDmaSemaphoreHandle = osSemaphoreCreate(osSemaphore(delaySamplesDmaSemaphore), 1);

  /* definition and creation of i2cCompletionSemaphore */
  osSemaphoreStaticDef(i2cCompletionSemaphore, &i2cCompletionSemaphoreControlBlock);
  i2cCompletionSemaphoreHandle = osSemaphoreCreate(osSemaphore(i2cCompletionSemaphore), 1);

  /* definition and creation of i2cFailedRomSemaphore */
  osSemaphoreStaticDef(i2cFailedRomSemaphore, &i2cFailedRomSemaphoreControlBlock);
  i2cFailedRomSemaphoreHandle = osSemaphoreCreate(osSemaphore(i2cFailedRomSemaphore), 1);

  /* definition and creation of i2cFailedDisplaySemaphore */
  osSemaphoreStaticDef(i2cFailedDisplaySemaphore, &i2cFailedDisplaySemaphoreControlBlock);
  i2cFailedDisplaySemaphoreHandle = osSemaphoreCreate(osSemaphore(i2cFailedDisplaySemaphore), 1);

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* Create the queue(s) */
  /* definition and creation of i2cQueue */
  osMessageQStaticDef(i2cQueue, 16, I2CMessage, i2cQueueBuffer, &i2cQueueControlBlock);
  i2cQueueHandle = osMessageCreate(osMessageQ(i2cQueue), NULL);

  /* definition and creation of displayCommandQueue */
  osMessageQStaticDef(displayCommandQueue, 16, uint16_t, displayCommandQueueBuffer, &displayCommandQueueControlBlock);
  displayCommandQueueHandle = osMessageCreate(osMessageQ(displayCommandQueue), NULL);

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
  hi2c1.Init.Timing = 0x2000090E;
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

}

/* USER CODE BEGIN 4 */

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	xTaskNotifyFromISR(btnHandlerTaskHandle, GPIO_Pin, eSetValueWithOverwrite, NULL);
}

void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef* hadc)
{
	if (hadc == &hadc3)
	{
		xTaskNotifyFromISR(effectsTaskHandle, (uint32_t) adcBufA, eSetValueWithOverwrite, NULL);
	}
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
	if (hadc == &hadc1)
	{
		xTaskNotifyFromISR(potHandlerTaskHandle, HAL_ADC_GetValue(hadc), eSetValueWithOverwrite, NULL);
	}
	else if (hadc == &hadc3)
	{
		xTaskNotifyFromISR(effectsTaskHandle, (uint32_t) adcBufB, eSetValueWithOverwrite, NULL);
	}
}

void HAL_DAC_ConvHalfCpltCallbackCh1(DAC_HandleTypeDef* hdac)
{
	xTaskNotifyFromISR(effectsTaskHandle, (uint32_t) dacBufA, eSetValueWithOverwrite, NULL);
}
	
void HAL_DAC_ConvCpltCallbackCh1(DAC_HandleTypeDef* hdac)
{
	xTaskNotifyFromISR(effectsTaskHandle, (uint32_t) dacBufB, eSetValueWithOverwrite, NULL);
}

void HAL_DMA_CpltCallback(DMA_HandleTypeDef* hdma)
{
	if (hdma == &hdma_memtomem_dma1_channel1)
	{
		xSemaphoreGiveFromISR(delaySamplesDmaSemaphoreHandle, NULL);
	}
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
	
	/* Start ADC and DAC. Note: ADC expects buffers to be ready to be filled,
			DAC expects buffers to be ready to be read from. Software must ensure this. */
	HAL_ADC_Start_DMA(&hadc3, (uint32_t *) adcBuf, ADC_BUF_LEN);
	HAL_DAC_Start_DMA(&hdac1, DAC_CHANNEL_1, (uint32_t *) dacBuf, DAC_BUF_LEN, DAC_ALIGN_12B_R);
	HAL_TIM_OC_Start(&htim2, TIM_CHANNEL_1);
	
  /* Infinite loop */
  for(;;)
  {
		uint16_t *currAdcBuf;
		uint16_t *currDacBuf;
		
		uint16_t *echoInputBuf;
		uint16_t *inputBuf;
		uint16_t *outputBuf;
		uint16_t *temp;
		
		/* To preserve effects for delayed samples (which are added for echo), they must be processed as well.
				echoDelaySamplesBuf is used for processing the delayed samples. */
		uint16_t *echoDelaySamplesBuf;
		
		/* Effects state and params must be latched before processing so they remain consistent for the whole buffer */
		EffectsState latchedEffectsState;
		EffectsParams latchedEffectsParams;
		
		/* DAC takes ticksToWait ticks to output a buffer before starting the next one.
				If software does not finish the next buffer before that time, the DAC outputs invalid data,
				so must give up and wait for next ADC buffer. */
		const uint16_t samplingPeriodUs = 25;	// Sampling period in microseconds
		TickType_t ticksToWait = pdMS_TO_TICKS(samplingPeriodUs * SAMPLE_BUF_LEN / 1000) - pdMS_TO_TICKS(2);	// Subtract some time to account for processing
		TimeOut_t timeOut;
		
		/* Wait for ADC buffer (and make sure it is an ADC buffer) */
		currAdcBuf = (uint16_t *) ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
		
		if (currAdcBuf != adcBufA && currAdcBuf != adcBufB)
		{
			continue;
		}
		
		/* Add ADC samples to delaySamplesBuf (must shift current samples first, then insert at end) */
		HAL_DMA_Start_IT(&hdma_memtomem_dma1_channel1, (uint32_t) &delaySamplesBuf[SAMPLE_BUF_LEN], (uint32_t) delaySamplesBuf, NUM_DELAY_SAMPLES - SAMPLE_BUF_LEN);
		
		if (xSemaphoreTake(delaySamplesDmaSemaphoreHandle, ticksToWait) != pdTRUE)
		{
			continue;
		}
		
		HAL_DMA_Start(&hdma_memtomem_dma1_channel1, (uint32_t) currAdcBuf, (uint32_t) &delaySamplesBuf[NUM_DELAY_SAMPLES - SAMPLE_BUF_LEN], NUM_DELAY_SAMPLES);
		
		/* Wait for DAC buffer (with timeout, and make sure it is a DAC buffer) */
		vTaskSetTimeOutState(&timeOut);
		currDacBuf = (uint16_t *) ulTaskNotifyTake(pdTRUE, ticksToWait);
		
		if (xTaskCheckForTimeOut(&timeOut, &ticksToWait) == pdTRUE)
		{
			continue;
		}
		
		if (currDacBuf != dacBufA && currDacBuf != dacBufB)
		{
			continue;
		}
		
		/* Allocate echoDelaySamplesBuf and copy over delaySamplesBuf */
		echoDelaySamplesBuf = (uint16_t *) pvPortMalloc(NUM_DELAY_SAMPLES * sizeof(uint16_t));
		
		HAL_DMA_Start_IT(&hdma_memtomem_dma1_channel1, (uint32_t) delaySamplesBuf, (uint32_t) echoDelaySamplesBuf, NUM_DELAY_SAMPLES);
		
		if (xSemaphoreTake(delaySamplesDmaSemaphoreHandle, ticksToWait) != pdTRUE)
		{
			continue;
		}
		
		/* Apply effects (in order) to inputBuf, storing in outputBuf.
				inputBuf and outputBuf atart as the ADC and DAC buffers respectively. */
		inputBuf = currAdcBuf;
		outputBuf = currDacBuf;
		
		latchedEffectsState = effectsState;
		latchedEffectsParams = effectsParams;
		
		for (int i = 0; i < 3; i++)
		{
			Effect effect = latchedEffectsState.ordered[i];
			
			if (latchedEffectsState.isEnabled[effect])
			{
				/* If the effect is echo, echoInputBuf must be prepared by
						concatenating echoDelaySamples to inputBuf in a new buffer */
				if (effect == ECHO)
				{
					size_t numDelaySamples = latchedEffectsParams.echo.delay_samples;	// Number of delayed samples needed (i.e., if index 0 represent n = 0 for signal x[n], x[-numDelaySamples] is the oldest sample)
					
					echoInputBuf = pvPortMalloc((numDelaySamples + SAMPLE_BUF_LEN) * sizeof(uint16_t));
					
					HAL_DMA_Start_IT(&hdma_memtomem_dma1_channel1, (uint32_t) &echoDelaySamplesBuf[NUM_DELAY_SAMPLES - numDelaySamples], (uint32_t) echoInputBuf, numDelaySamples);
					
					if (xSemaphoreTake(delaySamplesDmaSemaphoreHandle, ticksToWait) != pdTRUE)
					{
						continue;
					}
					
					HAL_DMA_Start_IT(&hdma_memtomem_dma1_channel1, (uint32_t) inputBuf, (uint32_t) &echoInputBuf[numDelaySamples], SAMPLE_BUF_LEN);
					
					if (xSemaphoreTake(delaySamplesDmaSemaphoreHandle, ticksToWait) != pdTRUE)
					{
						continue;
					}
				}
				
				/* Apply appropriate effect */
				switch (effect)
				{
					case OVERDRIVE: 	buf_overdrive(inputBuf, outputBuf, SAMPLE_BUF_LEN, &latchedEffectsParams.overdrive); break;
					case ECHO:				buf_echo(echoInputBuf, outputBuf, SAMPLE_BUF_LEN, &latchedEffectsParams.echo); break;
					case COMPRESSION:	buf_compression(inputBuf, outputBuf, SAMPLE_BUF_LEN, &latchedEffectsParams.compression); break;
					default: break;
				}
				
				/* If echo is enabled but not the first effect, also apply effect to delayed samples (echoDelaySamplesBuf).
						This only needs to be done so echoes of delayed samples are consistent with non-delayed samples.*/
				if (latchedEffectsState.isEnabled[ECHO] && latchedEffectsState.ordered[0] != ECHO)
				{
					/* Note: ECHO is ommitted since an effect can only appear once */
					switch (effect)
					{
						case OVERDRIVE: 	buf_overdrive(echoDelaySamplesBuf, echoDelaySamplesBuf, NUM_DELAY_SAMPLES, &latchedEffectsParams.overdrive); break;
						case COMPRESSION:	buf_compression(echoDelaySamplesBuf, echoDelaySamplesBuf, NUM_DELAY_SAMPLES, &latchedEffectsParams.compression); break;
						default: break;
					}
					
					if (effect == ECHO)
					{
						vPortFree(echoDelaySamplesBuf);
					}
				}
				
				/* Swap inputBuf and outputBuf. Since effects are applied sequentially,
						outputBuf becomes the input for the next effect. */
				temp = inputBuf;
				inputBuf = outputBuf;
				outputBuf = temp;
			}
		}
		
		/* If outputBuf is not the DAC buffer (i.e., the DAC buffer does not
				have the final output), copy outputBuf to dacBuf */
		if (outputBuf != dacBuf)
		{
			memcpy(currDacBuf, outputBuf, (DAC_BUF_LEN / 2) * sizeof(uint16_t));
		}
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
  /* Infinite loop */
  for(;;)
  {
		/* Wait for interrupt from button */
		uint16_t pin = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
		
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
	uint16_t pollingFrequencyMs = 5;
	
  /* Infinite loop */
  for(;;)
  {
		Effect effect_a = effectsState.ordered[0];
		Effect effect_b = effectsState.ordered[1];
		Effect effect_c = effectsState.ordered[2];
		
		effectsState.isEnabled[effect_a] = HAL_GPIO_ReadPin(SWITCH_A_GPIO_Port, SWITCH_A_Pin) == GPIO_PIN_SET;
		effectsState.isEnabled[effect_b] = HAL_GPIO_ReadPin(SWITCH_A_GPIO_Port, SWITCH_B_Pin) == GPIO_PIN_SET;
		effectsState.isEnabled[effect_c] = HAL_GPIO_ReadPin(SWITCH_A_GPIO_Port, SWITCH_C_Pin) == GPIO_PIN_SET;

		osDelay(pollingFrequencyMs);
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
	TickType_t samplingFrequency = pdMS_TO_TICKS(5);
	TickType_t ticksToWait = samplingFrequency + pdMS_TO_TICKS(1);
	
	const uint16_t adcMax = UINT8_MAX;	// Pot values are converted to 8-bit
	
	ADC_ChannelConfTypeDef sConfig;
	sConfig.Channel = ADC_CHANNEL_1;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
	
  /* Infinite loop */
  for(;;)
  {
		/* Sample each pot in turn */
		for (int pot = 0; pot < 4; pot++)
		{
			TimeOut_t timeOut;
			uint32_t adcValue;
			
			/* Set ADC to appropriate channel */
			switch (pot)
			{
				case 0: sConfig.Channel = ADC_CHANNEL_1; break;	// POT_A
				case 1: sConfig.Channel = ADC_CHANNEL_2; break;	// POT_B
				case 2: sConfig.Channel = ADC_CHANNEL_4; break;	// POT_C
				case 3: sConfig.Channel = ADC_CHANNEL_6; break;	// POT_D
				default: break;
			}
			
			/* Start ADC and wait (skip if timed out since data is obsolete) */
			HAL_ADC_ConfigChannel(&hadc1, &sConfig);
			HAL_ADC_Start_IT(&hadc1);

			vTaskSetTimeOutState(&timeOut);
			
			adcValue = ulTaskNotifyTake(pdTRUE, ticksToWait);

			if (xTaskCheckForTimeOut(&timeOut, &ticksToWait) == pdTRUE)
			{
				continue;
			}

			/* Handle POT_x depending on active effect */
			if (effectsState.ordered[effectsState.activeEffectSelection] == OVERDRIVE)
			{
				switch (pot)
				{
					case 0:
					{
						taskENTER_CRITICAL();	// Effects params are accessed by other tasks and this is not atomic
						/* Assumes max > min and 0 <= adcValue <= adcMax */
						size_t min = effectsParams.overdriveMin.gain;
						size_t max = effectsParams.overdriveMax.gain;
						/* Convert adcValue to value in [min, max] through linear mapping */
						effectsParams.overdrive.gain = (((uint32_t) adcValue << FIXED_POINT_Q) / adcMax * (max - min) + min) >> FIXED_POINT_Q;
						taskEXIT_CRITICAL();
					} break;
					case 1:
					{
						taskENTER_CRITICAL();
						size_t min = effectsParams.overdriveMin.level;
						size_t max = effectsParams.overdriveMax.level;
						effectsParams.overdrive.level = (((uint32_t) adcValue << FIXED_POINT_Q) / adcMax * (max - min) + min) >> FIXED_POINT_Q;
						taskEXIT_CRITICAL();
					} break;
					case 2:
					{
						taskENTER_CRITICAL();
						size_t min = effectsParams.overdriveMin.tone;
						size_t max = effectsParams.overdriveMax.tone;
						effectsParams.overdrive.tone = (((uint32_t) adcValue << FIXED_POINT_Q) / adcMax * (max - min) + min) >> FIXED_POINT_Q;
						taskEXIT_CRITICAL();
					} break;
					case 3:
					{
						taskENTER_CRITICAL();
						size_t min = effectsParams.overdriveMin.mix;
						size_t max = effectsParams.overdriveMax.mix;
						effectsParams.overdrive.mix = (((uint32_t) adcValue << FIXED_POINT_Q) / adcMax * (max - min) + min) >> FIXED_POINT_Q;
						taskEXIT_CRITICAL();
					} break;
					default: break;
				}
			}
			else if (effectsState.ordered[effectsState.activeEffectSelection] == ECHO)
			{
				switch (pot)
				{
					case 0:
					{
						taskENTER_CRITICAL();
						size_t min = effectsParams.echoMin.pre_delay;
						size_t max = effectsParams.echoMax.pre_delay;
						effectsParams.echo.pre_delay = (((uint32_t) adcValue << FIXED_POINT_Q) / adcMax * (max - min) + min) >> FIXED_POINT_Q;
						taskEXIT_CRITICAL();
					} break;
					case 1:
					{
						taskENTER_CRITICAL();
						size_t min = effectsParams.echoMin.density;
						size_t max = effectsParams.echoMax.density;
						effectsParams.echo.density = (((uint32_t) adcValue << FIXED_POINT_Q) / adcMax * (max - min) + min) >> FIXED_POINT_Q;
						taskEXIT_CRITICAL();
					} break;
					case 2:
					{
						taskENTER_CRITICAL();
						size_t min = effectsParams.echoMin.attack;
						size_t max = effectsParams.echoMax.attack;
						effectsParams.echo.attack = (((uint32_t) adcValue << FIXED_POINT_Q) / adcMax * (max - min) + min) >> FIXED_POINT_Q;
						taskEXIT_CRITICAL();
					} break;
					case 3:
					{
						taskENTER_CRITICAL();
						size_t min = effectsParams.echoMin.decay;
						size_t max = effectsParams.echoMax.decay;
						effectsParams.echo.decay = (((uint32_t) adcValue << FIXED_POINT_Q) / adcMax * (max - min) + min) >> FIXED_POINT_Q;
						taskEXIT_CRITICAL();
					} break;
					default: break;
				}
			}
			else if (effectsState.ordered[effectsState.activeEffectSelection] == COMPRESSION)
			{
				switch (pot)
				{
					case 0:
					{
						taskENTER_CRITICAL();
						size_t min = effectsParams.compressionMin.threshold;
						size_t max = effectsParams.compressionMax.threshold;
						effectsParams.compression.threshold = (((uint32_t) adcValue << FIXED_POINT_Q) / adcMax * (max - min) + min) >> FIXED_POINT_Q;
						taskEXIT_CRITICAL();
					} break;
					case 1:
					{
						taskENTER_CRITICAL();
						size_t min = effectsParams.compressionMin.ratio;
						size_t max = effectsParams.compressionMax.ratio;
						effectsParams.compression.ratio = (((uint32_t) adcValue << FIXED_POINT_Q) / adcMax * (max - min) + min) >> FIXED_POINT_Q;
						taskEXIT_CRITICAL();
					} break;
					case 2:	break;
					case 3:	break;
					default: break;
				}
			}
		}
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
	const uint32_t trials = 5;
	const uint32_t blockingTimeOutMs = 100;	// Time out for each HAL call (a blocking operation)
	const uint32_t tryAgainDelayMs = 100;	// How long to wait before trying HAL call again
	TickType_t ticksToWaitToDrop = pdMS_TO_TICKS(5000);	// Ticks to wait before dropping message
	TimeOut_t timeOut;
	
  /* Infinite loop */
  for(;;)
  {
		I2CMessage message; 
		
		xQueueReceive(i2cQueueHandle, (uint32_t *) &message, portMAX_DELAY);
		
		/* Check if device is ready, delay to check again if not, drop message if timed out */
		vTaskSetTimeOutState(&timeOut);
		
		HAL_StatusTypeDef status;
		
		do
		{
			status = HAL_I2C_IsDeviceReady(&hi2c1, message.address, trials, blockingTimeOutMs);
			
			if (status != HAL_OK)
			{	
				osDelay(tryAgainDelayMs);
			}
		} while (xTaskCheckForTimeOut(&timeOut, &ticksToWaitToDrop) != pdTRUE && status != HAL_OK);
		
		if (status != HAL_OK)
		{
			continue;
		}
		
		/* Transmit or receive message, dropping message if timed out */
		if (message.rxTxBar)
		{
			/* Receive */
			status = HAL_I2C_Master_Receive_IT(&hi2c1, message.address, message.pPayload, message.items);
		
			if (xSemaphoreTake(i2cCompletionSemaphoreHandle, ticksToWaitToDrop) != pdTRUE || status != HAL_OK)
			{
				*(message.pFailed) = true;
			}
			else
			{
				*(message.pFailed) = false;
			}
			
			/* Notify task that pFailed is valid (i.e., operation completed) */
			if (message.sourceTask == romHandlerTaskHandle)
			{
				xSemaphoreGive(i2cFailedRomSemaphoreHandle);
			}
			else if (message.sourceTask == displayHandlerTaskHandle)
			{
				xSemaphoreGive(i2cFailedDisplaySemaphoreHandle);
			}
		}
		else
		{
			/* Transmit */
			status = HAL_I2C_Master_Transmit_IT(&hi2c1, message.address, message.pPayload, message.items);
		
			if (xSemaphoreTake(i2cCompletionSemaphoreHandle, ticksToWaitToDrop) != pdTRUE || status != HAL_OK)
			{
				*(message.pFailed) = true;
			}
			else
			{
				*(message.pFailed) = false;
			}
			
			/* Notify task that pFailed is valid (i.e., operation completed) */
			if (message.sourceTask == romHandlerTaskHandle)
			{
				xSemaphoreGive(i2cFailedRomSemaphoreHandle);
			}
			else if (message.sourceTask == displayHandlerTaskHandle)
			{
				xSemaphoreGive(i2cFailedDisplaySemaphoreHandle);
			}
			
			vPortFree(message.pPayload);
		}
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
	
	uint16_t readAddress = 0;
	uint16_t writeAddress = 0;
	
	/* Copy effectsState and effectsParams FROM ROM */
	bool failed;
	
	do
	{
		const uint16_t timeOutMs = 100;
		
		I2CMessage message;
		
		/* Send address first */
		message.sourceTask = romHandlerTaskHandle;
		message.rxTxBar = false;
		message.address = (ROM_I2C_ADDR << 1) & ~1U;	// Address with write enabled
		
		message.pPayload = pvPortMalloc(payloadSizeBytes);	// Freed by I2C handler task
		message.pPayload[0] = (writeAddress & 0xFF00) >> 8;	// Upper byte of address
		message.pPayload[1] = (writeAddress & 0x00FF) >> 8;	// Lower byte of address
		
		message.items = 2;
		message.pFailed = &failed;
		
		HAL_GPIO_WritePin(GPIOC, nNVM_WE_Pin, GPIO_PIN_RESET);
		
		if (xQueueSend(i2cQueueHandle, (void *) &message, pdMS_TO_TICKS(timeOutMs)) != pdPASS)
		{
			HAL_GPIO_WritePin(GPIOC, nNVM_WE_Pin, GPIO_PIN_SET);
			vPortFree(message.pPayload);
			continue;
		}
		
		if (xSemaphoreTake(i2cFailedRomSemaphoreHandle, pdMS_TO_TICKS(timeOutMs))!= pdTRUE)
		{
			HAL_GPIO_WritePin(GPIOC, nNVM_WE_Pin, GPIO_PIN_SET);
			vPortFree(message.pPayload);
			continue;
		}
		
		if (failed)
		{
			HAL_GPIO_WritePin(GPIOC, nNVM_WE_Pin, GPIO_PIN_SET);
			vPortFree(message.pPayload);
			continue;
		}
		
		HAL_GPIO_WritePin(GPIOC, nNVM_WE_Pin, GPIO_PIN_SET);
		
		/* Read data */
		size_t payloadSizeBytes = sizeof(EffectsState) + sizeof(EffectsParams);
		
		message.sourceTask = romHandlerTaskHandle;
		message.rxTxBar = true;
		message.address = (ROM_I2C_ADDR << 1) | 1U;	// Address with read enabled
		message.pPayload = pvPortMalloc(payloadSizeBytes);
		message.items = payloadSizeBytes;
		message.pFailed = &failed;
		
		if (xQueueSend(i2cQueueHandle, (void *) &message, pdMS_TO_TICKS(timeOutMs)) != pdPASS)
		{
			vPortFree(message.pPayload);
			continue;
		}
		
		if (xSemaphoreTake(i2cFailedRomSemaphoreHandle, pdMS_TO_TICKS(timeOutMs))!= pdTRUE)
		{
			vPortFree(message.pPayload);
			continue;
		}
		
		if (failed)
		{
			vPortFree(message.pPayload);
			continue;
		}
		
		taskENTER_CRITICAL();	// Effects configs are accessed by other tasks and this is not atomic
		memcpy(&effectsState, message.pPayload, sizeof(EffectsState);
		memcpy(&effectsParams, &message.pPayload[sizeof(EffectsState)], sizeof(EffectsParams));
		taskEXIT_CRITICAL();
	} while (failed);
	
  /* Infinite loop */
  for(;;)
  {
		/* Copy effectsState and effectsParams TO ROM every x seconds */
		taskENTER_CRITICAL();	// Effects configs are accessed by other tasks and this is not atomic
		EffectsState latchedEffectsState = effectsState;
		EffectsParams latchedEffectsParams = effectsParams;
		taskEXIT_CRITICAL();
		
		size_t numBytesToWrite = sizeof(EffectsState) + sizeof(EffectsParams);
		size_t payloadSizeBytes = sizeof(uint16_t) + numBytesToWrite;	// One 16-bit address to start page write
		
		bool failed;
		
		I2CMessage message;
		
		message.sourceTask = romHandlerTaskHandle;
		message.rxTxBar = false;
		message.address = (ROM_I2C_ADDR << 1) & ~1U;	// Address with write enabled
		
		message.pPayload = pvPortMalloc(payloadSizeBytes);	// Freed by I2C handler task
		message.pPayload[0] = (writeAddress & 0xFF00) >> 8;	// Upper byte of address
		message.pPayload[1] = (writeAddress & 0x00FF) >> 8;	// Lower byte of address
		memcpy(&message.pPayload[2], &latchedEffectsState, sizeof(EffectsState));
		memcpy(&message.pPayload[2 + sizeof(EffectsState)], &latchedEffectsParams, sizeof(EffectsParams));
		
		message.items = payloadSizeBytes;	// Number of bytes in payload
		message.pFailed = &failed;
		
		HAL_GPIO_WritePin(GPIOC, nNVM_WE_Pin, GPIO_PIN_RESET);
		
		if (xQueueSend(i2cQueueHandle, (void *) &message, pdMS_TO_TICKS(updateFrequencyMs)) != pdPASS)
		{
			HAL_GPIO_WritePin(GPIOC, nNVM_WE_Pin, GPIO_PIN_SET);
			vPortFree(message.pPayload);
			continue;	// Update period already elapsed, so just continue
		}
		
		if (xSemaphoreTake(i2cFailedRomSemaphoreHandle, pdMS_TO_TICKS(updateFrequencyMs))!= pdTRUE)
		{
			HAL_GPIO_WritePin(GPIOC, nNVM_WE_Pin, GPIO_PIN_SET);
			vPortFree(message.pPayload);
			continue;	// Update period already elapsed, so just continue
		}
		
		if (failed)
		{
			HAL_GPIO_WritePin(GPIOC, nNVM_WE_Pin, GPIO_PIN_SET);
			vPortFree(message.pPayload);
			continue;	// Try again immediately
		}
		
		HAL_GPIO_WritePin(GPIOC, nNVM_WE_Pin, GPIO_PIN_SET);
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
  if (htim->Instance == TIM1) {
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

#ifdef  USE_FULL_ASSERT
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

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
