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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define AUDIO_MIDPOINT 2048
#define DEVIATION_LIMIT 700
#define AUDIO_BUFFER_SIZE 4000

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef hspi1;
DMA_HandleTypeDef hdma_spi1_rx;

TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim6;
TIM_HandleTypeDef htim7;
TIM_HandleTypeDef htim16;

UART_HandleTypeDef huart2;
DMA_HandleTypeDef hdma_usart2_tx;

/* USER CODE BEGIN PV */
uint8_t MSB_8;
uint8_t LSB_8;
uint16_t ADC_output;
uint16_t count_1;
uint16_t count_2;
double distance;
int current_mode = 0;
char string_1[40] = "\0";
float min_distance = 10;
char state[9] = "\0";
int flag = 0;
volatile int is_recording = 0;
int manual_flag = 0;
char duration_buffer[5] = "\0";

int remaining_seconds = 0;
uint16_t tick_counter = 0;
int distance_cooldown = 0;
int current_pos = 0;
uint16_t filter_buffer[3] = {0};
int sum = 0;
uint16_t last_valid = AUDIO_MIDPOINT;
uint16_t spi_rx_buffer[AUDIO_BUFFER_SIZE];
uint8_t tx_buf_a[(AUDIO_BUFFER_SIZE * 3 / 4) + 2];
uint8_t tx_buf_b[(AUDIO_BUFFER_SIZE * 3 / 4) + 2];
uint8_t temp_byte;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_SPI1_Init(void);
static void MX_TIM1_Init(void);
static void MX_TIM6_Init(void);
static void MX_TIM16_Init(void);
static void MX_TIM7_Init(void);
/* USER CODE BEGIN PFP */
void HCSR04_Read(void);
void delay_us(uint16_t delay);
uint16_t moving_average_filter(uint16_t *arr, int *sum, int pos, int len, uint16_t nextNum);
uint16_t outlier_rejection(uint16_t sample, uint16_t *last_valid);
void process_audio(uint16_t start_idx, uint16_t end_idx, uint8_t *tx_buf);

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
  MX_USART2_UART_Init();
  MX_SPI1_Init();
  MX_TIM1_Init();
  MX_TIM6_Init();
  MX_TIM16_Init();
  MX_TIM7_Init();
  /* USER CODE BEGIN 2 */
  HAL_SPI_Receive_DMA(&hspi1, (uint8_t *)spi_rx_buffer, AUDIO_BUFFER_SIZE);
  HAL_TIM_Base_Start(&htim6);
  HAL_TIM_IC_Start_IT(&htim1, TIM_CHANNEL_1);
  HAL_TIM_Base_Start_IT(&htim16);
  HAL_UART_Receive_IT(&huart2, (uint8_t *)state, 8);

  /* USER CODE END 2 */

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

  /** Configure the main internal regulator output voltage
   */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure LSE Drive Capability
   */
  HAL_PWR_EnableBkUpAccess();
  __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_LOW);

  /** Initializes the RCC Oscillators according to the specified parameters
   * in the RCC_OscInitTypeDef structure.
   */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSE | RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = 0;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_6;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_MSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 16;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
   */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Enable MSI Auto calibration
   */
  HAL_RCCEx_EnableMSIPLLMode();
}

/**
 * @brief SPI1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_SLAVE;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES_RXONLY;
  hspi1.Init.DataSize = SPI_DATASIZE_16BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 7;
  hspi1.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
  hspi1.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */
}

/**
 * @brief TIM1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM1_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_IC_InitTypeDef sConfigIC = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 32 - 1;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 65535;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_IC_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterOutputTrigger2 = TIM_TRGO2_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigIC.ICPolarity = TIM_INPUTCHANNELPOLARITY_BOTHEDGE;
  sConfigIC.ICSelection = TIM_ICSELECTION_DIRECTTI;
  sConfigIC.ICPrescaler = TIM_ICPSC_DIV1;
  sConfigIC.ICFilter = 0;
  if (HAL_TIM_IC_ConfigChannel(&htim1, &sConfigIC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */

  /* USER CODE END TIM1_Init 2 */
}

/**
 * @brief TIM6 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM6_Init(void)
{

  /* USER CODE BEGIN TIM6_Init 0 */

  /* USER CODE END TIM6_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM6_Init 1 */

  /* USER CODE END TIM6_Init 1 */
  htim6.Instance = TIM6;
  htim6.Init.Prescaler = 32 - 1;
  htim6.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim6.Init.Period = 65535;
  htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim6) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim6, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM6_Init 2 */

  /* USER CODE END TIM6_Init 2 */
}

/**
 * @brief TIM7 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM7_Init(void)
{

  /* USER CODE BEGIN TIM7_Init 0 */

  /* USER CODE END TIM7_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM7_Init 1 */

  /* USER CODE END TIM7_Init 1 */
  htim7.Instance = TIM7;
  htim7.Init.Prescaler = 32000 - 1;
  htim7.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim7.Init.Period = 250 - 1;
  htim7.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim7) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim7, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM7_Init 2 */

  /* USER CODE END TIM7_Init 2 */
}

/**
 * @brief TIM16 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM16_Init(void)
{

  /* USER CODE BEGIN TIM16_Init 0 */

  /* USER CODE END TIM16_Init 0 */

  /* USER CODE BEGIN TIM16_Init 1 */

  /* USER CODE END TIM16_Init 1 */
  htim16.Instance = TIM16;
  htim16.Init.Prescaler = 32000 - 1;
  htim16.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim16.Init.Period = 500 - 1;
  htim16.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim16.Init.RepetitionCounter = 0;
  htim16.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim16) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM16_Init 2 */

  /* USER CODE END TIM16_Init 2 */
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
  huart2.Init.BaudRate = 921600;
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

  /* DMA interrupt init */
  /* DMA1_Channel2_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel2_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel2_IRQn);
  /* DMA1_Channel7_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel7_IRQn, 1, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel7_IRQn);
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
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3 | GPIO_PIN_6, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LD3_GPIO_Port, LD3_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : PA3 PA6 */
  GPIO_InitStruct.Pin = GPIO_PIN_3 | GPIO_PIN_6;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : LD3_Pin */
  GPIO_InitStruct.Pin = LD3_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD3_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART2)
  {

    if (manual_flag)
    {
      duration_buffer[4] = '\0';
      tick_counter = 0;
      remaining_seconds = 0;
      sscanf(duration_buffer, "%d", &remaining_seconds);
      HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, GPIO_PIN_SET);
      is_recording = 1;
      HAL_TIM_Base_Start_IT(&htim7);
      manual_flag = 0;
      HAL_UART_Receive_IT(&huart2, (uint8_t *)state, 8);
    }

    else
    {
      if (strncmp(state, "manual  ", 8) == 0)
      {
        current_mode = 2;
        manual_flag = 1;
        HAL_UART_Receive_IT(&huart2, (uint8_t *)duration_buffer, 4);
      }

      else if (strncmp(state, "distance", 8) == 0)
      {
        current_mode = 1;
        HAL_UART_Receive_IT(&huart2, (uint8_t *)state, 8);
      }

      else
      {
        HAL_UART_Receive_IT(&huart2, (uint8_t *)state, 8);
      }
    }
  }
}

void process_audio(uint16_t start_idx, uint16_t end_idx, uint8_t *tx_buf)
{
  if (is_recording)
  {
    int pack_idx = 0;

    static uint16_t last_good_a = AUDIO_MIDPOINT;
    static uint16_t last_good_b = AUDIO_MIDPOINT;

    for (int i = start_idx; i < end_idx; i += 2)
    {
      uint16_t raw_a = spi_rx_buffer[i];
      uint16_t raw_b = spi_rx_buffer[i + 1];
      uint16_t a, b;

      if ((raw_a & 0xF000) == 0xA000)
      {
        a = raw_a & 0x0FFF;
      }

      else
      {
        a = last_good_a;
      }

      if ((raw_b & 0xF000) == 0xA000)
      {
        b = raw_b & 0x0FFF;
      }

      else
      {
        b = last_good_b;
      }

      // 2. NOW it is safe to mask the 12 bits and filter
      a = outlier_rejection(a & 0x0FFF, &last_valid);
      a = moving_average_filter(filter_buffer, &sum, current_pos, 3, a);
      current_pos = (current_pos + 1) % 3;

      b = outlier_rejection(b & 0x0FFF, &last_valid);
      b = moving_average_filter(filter_buffer, &sum, current_pos, 3, b);
      current_pos = (current_pos + 1) % 3;

      // 3. PACK two 12-bit samples into 3 bytes
      tx_buf[pack_idx++] = (a >> 4) & 0xFF;
      tx_buf[pack_idx++] = ((a & 0x0F) << 4) | ((b >> 8) & 0x0F);
      tx_buf[pack_idx++] = b & 0xFF;
    }

    // 4. NON-BLOCKING TRANSMIT (Fixes the dropped audio!)
    // Use '_IT' so the CPU doesn't freeze inside the DMA interrupt.
    if (huart2.gState == HAL_UART_STATE_READY)
    {
      HAL_UART_Transmit_DMA(&huart2, tx_buf, pack_idx);
    }
  }
}

/*void process_audio(uint16_t start_idx, uint16_t end_idx, uint8_t *tx_buf)
{
  if (is_recording) {
    int pack_idx = 0;

    for (int i = start_idx; i < end_idx; i+= 2) {
            // 1. Grab the real SPI data and safely mask to 12 bits
      uint16_t a = spi_rx_buffer[i] & 0x0FFF;
          uint16_t b = spi_rx_buffer[i+1] & 0x0FFF;

          // 2. Pack two 12-bit samples into 3 bytes
          tx_buf[pack_idx++] = (a >> 4) & 0xFF;
          tx_buf[pack_idx++] = ((a & 0x0F) << 4) | ((b >> 8) & 0x0F);
          tx_buf[pack_idx++] = b & 0xFF;
    }

        // 3. Fire the UART DMA to send it to Python
    if (huart2.gState == HAL_UART_STATE_READY) {
      HAL_UART_Transmit_DMA(&huart2, tx_buf, pack_idx);
    }
  }
}*/

void HAL_SPI_RxHalfCpltCallback(SPI_HandleTypeDef *hspi)
{
  if (hspi == &hspi1)
  {
    process_audio(0, AUDIO_BUFFER_SIZE / 2, tx_buf_a);
  }
}

void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi)
{
  if (hspi == &hspi1)
  {
    process_audio(AUDIO_BUFFER_SIZE / 2, AUDIO_BUFFER_SIZE, tx_buf_b);
  }
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim == &htim16)
  {
    HCSR04_Read();

    if (current_mode == 1)
    {
      if (distance_cooldown > 0)
      {
        --distance_cooldown;
      }

      else
      {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, GPIO_PIN_RESET);
        is_recording = 0;
      }
    }
  }

  if (htim == &htim7)
  {
    if (current_mode == 2)
    {
      tick_counter++;

      if (tick_counter >= 4)
      {
        remaining_seconds--;
        tick_counter = 0;
      }
    }

    if (remaining_seconds <= 0)
    {
      is_recording = 0;
      HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, GPIO_PIN_RESET);
      HAL_TIM_Base_Stop_IT(&htim7);
    }
  }
}

void delay_us(uint16_t delay)
{
  __HAL_TIM_SET_COUNTER(&htim6, 0);
  while (__HAL_TIM_GET_COUNTER(&htim6) < delay)
  {
    ;
  }
}

void HCSR04_Read(void)
{
  flag = 0;
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_SET);
  delay_us(10);
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET);
}

void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
  if (hspi->Instance == SPI1)
  {
    // Check if the specific error is an Overrun

    HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_3);
    // DEBUG: Toggle your PA3 LED so you can see the crash!
    __HAL_SPI_CLEAR_OVRFLAG(hspi);
    __HAL_SPI_CLEAR_FREFLAG(hspi);
    HAL_SPI_Receive_DMA(&hspi1, (uint8_t *)spi_rx_buffer, AUDIO_BUFFER_SIZE);

    // The HAL has already cleared the OVR flag at this point,
    // but the interrupt is now STOPPED. You must restart it:
  }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if (huart == &huart2)
  {
    HAL_UART_Receive_IT(&huart2, (uint8_t *)state, 8);
  }
}

void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
  if ((htim == &htim1) && (htim->Channel == 1))
  {
    if (!flag)
    {
      count_1 = HAL_TIM_ReadCapturedValue(&htim1, TIM_CHANNEL_1);
      flag = 1;
    }

    else
    {
      count_2 = HAL_TIM_ReadCapturedValue(&htim1, TIM_CHANNEL_1);
      __HAL_TIM_SET_COUNTER(htim, 0);
      distance = (uint16_t)(count_2 - count_1) / 58.0;

      if (current_mode == 1)
      {
        if (distance < min_distance)
        {
          HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, GPIO_PIN_SET);
          is_recording = 1;
          distance_cooldown = 3;
        }
      }
      flag = 0;
    }
  }
}

uint16_t moving_average_filter(uint16_t *arr, int *sum, int pos, int len, uint16_t nextNum)
{
  *sum = *sum - arr[pos] + nextNum;
  arr[pos] = nextNum;

  return (*sum / len);
}

uint16_t outlier_rejection(uint16_t sample, uint16_t *last_valid)
{
  int diff = sample - AUDIO_MIDPOINT;

  if (abs(diff) > DEVIATION_LIMIT)
  {
    return *last_valid;
  }

  *last_valid = sample;
  return sample;
}
/* USER CODE END 4 */

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
