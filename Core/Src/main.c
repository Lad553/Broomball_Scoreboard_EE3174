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
#include "display.h"
#include "scoreboard_model.h"
#include "scoreboard_ui.h"
#include "touch.h"
#include <string.h>/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
// Touch locations for the main page
#define HOME_PEN_BOX_X 14
#define HOME_PEN_BOX_Y 172
#define HOME_PEN_BOX_W 126
#define HOME_PEN_BOX_H 58

#define AWAY_PEN_BOX_X 180
#define AWAY_PEN_BOX_Y 172
#define AWAY_PEN_BOX_W 126
#define AWAY_PEN_BOX_H 58

#define MAIN_CLOCK_X 98
#define MAIN_CLOCK_Y 10
#define MAIN_CLOCK_W 126
#define MAIN_CLOCK_H 60

#define HOME_TIMEOUT_BTN_X 18
#define HOME_TIMEOUT_BTN_Y 92
#define HOME_TIMEOUT_BTN_W 126
#define HOME_TIMEOUT_BTN_H 58

#define AWAY_TIMEOUT_BTN_X 176
#define AWAY_TIMEOUT_BTN_Y 92
#define AWAY_TIMEOUT_BTN_W 126
#define AWAY_TIMEOUT_BTN_H 58

#define OVERTIME_REG_BTN_X 28
#define OVERTIME_REG_BTN_Y 84
#define OVERTIME_PLAYOFF_BTN_X 28
#define OVERTIME_PLAYOFF_BTN_Y 152
#define OVERTIME_BTN_W 264
#define OVERTIME_BTN_H 50

// Timing values
#define TEST_PERIOD_SECONDS 120U
#define TOUCH_DEBOUNCE_MS 250U
#define BUTTON_DEBOUNCE_MS 250U
#define TIMEOUT_SECONDS 60U
#define REGULAR_SEASON_OT_SECONDS 300U
#define PLAYOFF_OT_SECONDS 900U
#define ALARM_DURATION_MS 3000U
#define BUZZER_TIMER_PRESCALER 31U
#define BUZZER_TIMER_PERIOD 249U

/*
 * CubeMX should generate these in main.h if you label the pins exactly as
 * CLOCK_BUTTON, HOME_SCORE_BUTTON, and AWAY_SCORE_BUTTON. These fallbacks let
 * this file still compile before CubeMX regeneration. The fallback pinout is:
 * CLOCK=A0/PA0, HOME=A1/PA1, AWAY=A2/PA4.
 * BUZZER=PB4/TIM3_CH1, ALARM_LED=PB0.
 */
#ifndef CLOCK_BUTTON_Pin
#define CLOCK_BUTTON_Pin GPIO_PIN_0
#define CLOCK_BUTTON_GPIO_Port GPIOA
#endif
#ifndef HOME_SCORE_BUTTON_Pin
#define HOME_SCORE_BUTTON_Pin GPIO_PIN_1
#define HOME_SCORE_BUTTON_GPIO_Port GPIOA
#endif
#ifndef AWAY_SCORE_BUTTON_Pin
#define AWAY_SCORE_BUTTON_Pin GPIO_PIN_4
#define AWAY_SCORE_BUTTON_GPIO_Port GPIOA
#endif
#ifndef BUZZER_PWM_Pin
#define BUZZER_PWM_Pin GPIO_PIN_4
#define BUZZER_PWM_GPIO_Port GPIOB
#endif
#ifndef ALARM_LED_Pin
#define ALARM_LED_Pin GPIO_PIN_0
#define ALARM_LED_GPIO_Port GPIOB
#endif/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

SPI_HandleTypeDef hspi1;

TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim2;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
// Flags changed by timer interrupt
volatile uint8_t timers_tick_flag = 0;
volatile uint8_t gameClockRunning = 0;

// Struct: software debounce state for one physical button
typedef struct {
	GPIO_PinState lastRaw;
	uint32_t lastChangeMs;
	uint8_t pressLatched;
} DebouncedButton;

// Enum: overtime rule variant
typedef enum {
	OVERTIME_NONE = 0,
	OVERTIME_REGULAR_SEASON,
	OVERTIME_PLAYOFFS
} OvertimeMode;

// Page and game state variables
static uint8_t timeoutPageOpen = 0;
static uint8_t overtimeChoicePageOpen = 0;
static uint8_t timeoutCountdownActive = 0;
static uint16_t timeoutSecondsRemaining = 0;
static TeamId timeoutOwner = TEAM_HOME;
static uint8_t gameComplete = 0;
static uint8_t overtimeActive = 0;
static OvertimeMode overtimeMode = OVERTIME_NONE;

// Touch and button debounce variables
static uint32_t lastTouchAcceptedMs = 0;
static DebouncedButton clockButton = {GPIO_PIN_SET, 0, 0};
static DebouncedButton homeScoreButton = {GPIO_PIN_SET, 0, 0};
static DebouncedButton awayScoreButton = {GPIO_PIN_SET, 0, 0};

// Alarm variables
static uint8_t alarmActive = 0;
static uint32_t alarmStopTimeMs = 0;
static uint8_t mainPageNeedsFullRedraw = 0;/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_I2C1_Init(void);
static void MX_TIM3_Init(void);
static void MX_TIM2_Init(void);
/* USER CODE BEGIN PFP */
static void handle_main_page_touch(ScoreboardState *board, uint16_t x, uint16_t y);
static void handle_penalty_page_touch(ScoreboardState *board, uint16_t x, uint16_t y);
static void handle_timeout_page_touch(ScoreboardState *board, uint16_t x, uint16_t y);
static void handle_overtime_choice_page_touch(ScoreboardState *board, uint16_t x, uint16_t y);
static void handle_physical_buttons(ScoreboardState *board);
static uint8_t point_in_rect(uint16_t px, uint16_t py, uint16_t x, uint16_t y, uint16_t w, uint16_t h);
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim);
static void Scoreboard_Tick1Hz(ScoreboardState *board);
static PenaltySlot *team_penalties(ScoreboardState *board, TeamId team);
static uint16_t penalty_default_seconds(PenaltyType type);
static int find_empty_penalty_slot(ScoreboardState *board, TeamId team);
static void add_penalty(ScoreboardState *board, TeamId team, PenaltyType type);
static void delete_penalty(ScoreboardState *board, TeamId team, int idx);
static void sort_penalties(ScoreboardState *board, TeamId team);
static void clear_all_penalties(ScoreboardState *board);
static void score_goal(ScoreboardState *board, TeamId scoringTeam);
static void remove_shortest_minor_penalty(ScoreboardState *board, TeamId penalizedTeam);
static void handle_clock_action(ScoreboardState *board);
static uint8_t touch_debounce_ok(void);
static uint8_t button_pressed_once(GPIO_TypeDef *port, uint16_t pin, DebouncedButton *button);
static void open_penalty_page(ScoreboardState *board, TeamId team);
static void return_to_main_page(ScoreboardState *board);
static void open_timeout_page(ScoreboardState *board);
static void return_from_timeout_page(ScoreboardState *board);
static void start_team_timeout(ScoreboardState *board, TeamId team);
static void handle_period_expired(ScoreboardState *board);
static void open_overtime_choice_page(ScoreboardState *board);
static void start_overtime(ScoreboardState *board, OvertimeMode mode);
static void finish_game(ScoreboardState *board);
static void alarm_start(void);
static void alarm_stop(void);
static void alarm_update(void);
static void alarm_led_on(void);
static void alarm_led_off(void);
static void signal_clock_expired(void);

void Scoreboard_DrawTimeoutPage(const ScoreboardState *s, uint16_t timeoutSeconds, uint8_t active, TeamId timeoutTeam);
void Scoreboard_UpdateTimeoutPage(const ScoreboardState *s, uint16_t timeoutSeconds, uint8_t active, TeamId timeoutTeam);
void Scoreboard_DrawOvertimeChoicePage(const ScoreboardState *s);/* USER CODE END PFP */

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
  MX_SPI1_Init();
  MX_USART2_UART_Init();
  MX_I2C1_Init();
  MX_TIM3_Init();
  MX_TIM2_Init();
  /* USER CODE BEGIN 2 */
  // Initialize screen and touch
  HAL_Delay(50);
  ILI9341_Init(&hspi1);
  TSC2007_Init(&hi2c1);
  alarm_stop();

  // Initialize scoreboard
  ScoreboardState board = {
	  .period = 1,
	  .homeScore = 0,
	  .awayScore = 0,
	  .homeTimeoutAvailable = 1,
	  .awayTimeoutAvailable = 1,
	  .gameSeconds = TEST_PERIOD_SECONDS,
	  .page = PAGE_MAIN,
	  .penaltyPageTeam = TEAM_HOME
  };

  gameClockRunning = 0;
  gameComplete = 0;

  // Start 1 second timer
  if (HAL_TIM_Base_Start_IT(&htim2) != HAL_OK)
  {
	  Error_Handler();
  }

  Scoreboard_DrawMainPage(&board);/* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {/* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	  TouchPoint p;

	  // If timer interrupt happened, update the timers
	  if (timers_tick_flag) {
	      timers_tick_flag = 0;
	      Scoreboard_Tick1Hz(&board);

	      if (timeoutPageOpen) {
	          Scoreboard_UpdateTimeoutPage(&board, timeoutSecondsRemaining, timeoutCountdownActive, timeoutOwner);
	      } else if (overtimeChoicePageOpen) {
	          // Do not update the main page over the overtime choice page
	      } else if (board.page == PAGE_MAIN) {
	          // After a period ends, redraw the whole main page once
	          // Then go back to only updating the changing parts
	          if (mainPageNeedsFullRedraw) {
	              Scoreboard_DrawMainPage(&board);
	              mainPageNeedsFullRedraw = 0;
	          } else {
	              Scoreboard_UpdateMainClock(&board);
	              Scoreboard_UpdatePenalties(&board);
	          }
	      } else {
	          Scoreboard_UpdatePenaltyPageTimers(&board);
	      }
	  }

	  handle_physical_buttons(&board);
	  alarm_update();

	  // Read touch screen with debounce
	  if (Touch_ReadScreenPoint(&p) && touch_debounce_ok()) {
	      if (overtimeChoicePageOpen) {
	          handle_overtime_choice_page_touch(&board, p.x, p.y);
	      } else if (timeoutPageOpen) {
	          handle_timeout_page_touch(&board, p.x, p.y);
	      } else if (board.page == PAGE_MAIN) {
	          handle_main_page_touch(&board, p.x, p.y);
	      } else {
	          handle_penalty_page_touch(&board, p.x, p.y);
	      }
	  }

	  HAL_Delay(5);
  }/* USER CODE END 3 */
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
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL6;
  RCC_OscInitStruct.PLL.PLLDIV = RCC_PLL_DIV3;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
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
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */
/* USER CODE END I2C1_Init 2 */

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
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */
/* USER CODE END SPI1_Init 2 */

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */
/* USER CODE END TIM3_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM3_Init 1 */
/* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 31;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 249;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */
/* USER CODE END TIM3_Init 2 */
  HAL_TIM_MspPostInit(&htim3);

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

  /* USER CODE BEGIN TIM2_Init 1 */
/* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 31999;
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
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
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
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */
/* USER CODE END USART2_Init 2 */

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
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6|GPIO_PIN_7, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(ALARM_LED_GPIO_Port, ALARM_LED_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : PC7 */
  GPIO_InitStruct.Pin = GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : PB6 PB7 */
  GPIO_InitStruct.Pin = GPIO_PIN_6|GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */
  // Physical buttons use pullups and go low when pressed
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;

  GPIO_InitStruct.Pin = CLOCK_BUTTON_Pin;
  HAL_GPIO_Init(CLOCK_BUTTON_GPIO_Port, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = HOME_SCORE_BUTTON_Pin;
  HAL_GPIO_Init(HOME_SCORE_BUTTON_GPIO_Port, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = AWAY_SCORE_BUTTON_Pin;
  HAL_GPIO_Init(AWAY_SCORE_BUTTON_GPIO_Port, &GPIO_InitStruct);

  // Alarm LED output
  GPIO_InitStruct.Pin = ALARM_LED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(ALARM_LED_GPIO_Port, &GPIO_InitStruct);

  // Buzzer PWM output
  GPIO_InitStruct.Pin = BUZZER_PWM_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF2_TIM3;
  HAL_GPIO_Init(BUZZER_PWM_GPIO_Port, &GPIO_InitStruct);/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
/*
Takes in pixel coordinates (px, py) and a rectangle defined by top-left (x, y), width w, and height h
Returns 1 if the point falls inside the rectangle, 0 otherwise
*/
static uint8_t point_in_rect(uint16_t px, uint16_t py, uint16_t x, uint16_t y, uint16_t w, uint16_t h){
	return (px >= x) && (px < (x + w)) && (py >= y) && (py < (y + h));
}

/*
Takes in the board state and touch coordinates from the main page
Routes the touch to the correct action: clock toggle, score edit, or penalty page
Does not return anything
*/
static void handle_main_page_touch(ScoreboardState *board, uint16_t x, uint16_t y){
	// Clock box: start/stop the clock or open the timeout page
	if (point_in_rect(x, y, MAIN_CLOCK_X, MAIN_CLOCK_Y, MAIN_CLOCK_W, MAIN_CLOCK_H)) {
		handle_clock_action(board);
		return;
	}

	// Home score box: upper half adds a goal, lower half decrements
	if (point_in_rect(x, y, 18, 78, 84, 72)) {
		if (y < (78 + 36)) {
			score_goal(board, TEAM_HOME);
			Scoreboard_UpdateHomeScore(board);
			Scoreboard_UpdatePenalties(board);
		} else {
			if (board->homeScore > 0)
				board->homeScore--;
			Scoreboard_UpdateHomeScore(board);
		}
		return;
	}

	// Away score box: upper half adds a goal, lower half decrements
	if (point_in_rect(x, y, 218, 78, 84, 72)) {
		if (y < (78 + 36)) {
			score_goal(board, TEAM_AWAY);
			Scoreboard_UpdateAwayScore(board);
			Scoreboard_UpdatePenalties(board);
		} else {
			if (board->awayScore > 0)
				board->awayScore--;
			Scoreboard_UpdateAwayScore(board);
		}
		return;
	}

	// Penalty boxes: open the penalty input page for the tapped team
	if (point_in_rect(x, y, HOME_PEN_BOX_X, HOME_PEN_BOX_Y, HOME_PEN_BOX_W, HOME_PEN_BOX_H)) {
		open_penalty_page(board, TEAM_HOME);
		return;
	}

	if (point_in_rect(x, y, AWAY_PEN_BOX_X, AWAY_PEN_BOX_Y, AWAY_PEN_BOX_W, AWAY_PEN_BOX_H)) {
		open_penalty_page(board, TEAM_AWAY);
		return;
	}
}

/*
Takes in the board state and touch coordinates from the penalty input page
Routes the touch to: back button, add minor, add major, or delete a specific slot
Does not return anything
*/
static void handle_penalty_page_touch(ScoreboardState *board, uint16_t x, uint16_t y){
	TeamId team = board->penaltyPageTeam;

	if (point_in_rect(x, y, 8, 8, 60, 25)) {
		return_to_main_page(board);
		return;
	}

	if (point_in_rect(x, y, 32, 200, 110, 20)) {
		add_penalty(board, team, PENALTY_MINOR);
		Scoreboard_DrawPenaltyPage(board);
		return;
	}

	if (point_in_rect(x, y, 178, 200, 110, 20)) {
		add_penalty(board, team, PENALTY_MAJOR);
		Scoreboard_DrawPenaltyPage(board);
		return;
	}

	// Check each row's delete button
	for (int i = 0; i < MAX_TEAM_PENALTIES; i++) {
		uint16_t rowY = 40 + (uint16_t)(i * 24);

		if (point_in_rect(x, y, 240, rowY, 52, 16)) {
			delete_penalty(board, team, i);
			Scoreboard_DrawPenaltyPage(board);
			return;
		}
	}
}

/*
Takes in the board state and touch coordinates from the timeout page
Routes the touch to: return to main (clock area), home timeout, or away timeout
Does not return anything
*/
static void handle_timeout_page_touch(ScoreboardState *board, uint16_t x, uint16_t y){
	if (point_in_rect(x, y, MAIN_CLOCK_X, MAIN_CLOCK_Y, MAIN_CLOCK_W, MAIN_CLOCK_H)) {
		handle_clock_action(board);
		return;
	}

	if (point_in_rect(x, y, HOME_TIMEOUT_BTN_X, HOME_TIMEOUT_BTN_Y, HOME_TIMEOUT_BTN_W, HOME_TIMEOUT_BTN_H)) {
		start_team_timeout(board, TEAM_HOME);
		return;
	}

	if (point_in_rect(x, y, AWAY_TIMEOUT_BTN_X, AWAY_TIMEOUT_BTN_Y, AWAY_TIMEOUT_BTN_W, AWAY_TIMEOUT_BTN_H)) {
		start_team_timeout(board, TEAM_AWAY);
		return;
	}
}

/*
Takes in the board state and touch coordinates from the overtime choice page
Starts the selected overtime mode when the corresponding button is tapped
Does not return anything
*/
static void handle_overtime_choice_page_touch(ScoreboardState *board, uint16_t x, uint16_t y){
	if (point_in_rect(x, y, OVERTIME_REG_BTN_X, OVERTIME_REG_BTN_Y, OVERTIME_BTN_W, OVERTIME_BTN_H)) {
		start_overtime(board, OVERTIME_REGULAR_SEASON);
		return;
	}

	if (point_in_rect(x, y, OVERTIME_PLAYOFF_BTN_X, OVERTIME_PLAYOFF_BTN_Y, OVERTIME_BTN_W, OVERTIME_BTN_H)) {
		start_overtime(board, OVERTIME_PLAYOFFS);
		return;
	}
}

/*
Takes in the HAL timer handle that fired the interrupt
Sets the 1 Hz tick flag when TIM2 overflows
Does not return anything
*/
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim){
	if (htim->Instance == TIM2)
		timers_tick_flag = 1;
}

/*
Takes in the board state
Advances the game clock, all running penalty clocks, and the timeout clock by one second
Triggers period expiration logic when the game clock reaches zero
Does not return anything
*/
static void Scoreboard_Tick1Hz(ScoreboardState *board){
	// If the timeout page is open, only tick the timeout clock
	if (timeoutPageOpen) {
		if (timeoutCountdownActive && timeoutSecondsRemaining > 0) {
			timeoutSecondsRemaining--;

			if (timeoutSecondsRemaining == 0)
				timeoutCountdownActive = 0;
		}
		return;
	}

	if (!gameClockRunning || gameComplete) return;

	// Decrement game clock
	if (board->gameSeconds > 0)
		board->gameSeconds--;

	// Decrement all active penalty clocks and expire them when they hit zero
	for (int team = 0; team < 2; team++) {
		for (int i = 0; i < MAX_TEAM_PENALTIES; i++) {
			PenaltySlot *p = &board->penalties[team][i];

			if (!p->used || !p->running)
				continue;
			if (p->remainingSeconds == 0)
				continue;

			p->remainingSeconds--;

			if (p->remainingSeconds == 0) {
				p->used = 0;
				p->running = 0;
			}
		}
		sort_penalties(board, (TeamId)team);
	}

	if (board->gameSeconds == 0)
		handle_period_expired(board);
}

/*
Takes in the board state and a team ID
Returns a pointer to the penalty slot array for that team
*/
static PenaltySlot *team_penalties(ScoreboardState *board, TeamId team){
	return board->penalties[(int)team];
}

/*
Takes in a penalty type
Returns the default duration in seconds for that type (120 for minor, 300 for major)
*/
static uint16_t penalty_default_seconds(PenaltyType type){
	switch (type) {
		case PENALTY_MINOR: return 120;
		case PENALTY_MAJOR: return 300;
		default: return 0;
	}
}

/*
Takes in the board state and a team ID
Searches the team's penalty slots for the first empty one
Returns the slot index, or -1 if all slots are full
*/
static int find_empty_penalty_slot(ScoreboardState *board, TeamId team){
	PenaltySlot *p = team_penalties(board, team);

	for (int i = 0; i < MAX_TEAM_PENALTIES; i++) {
		if (!p[i].used)
			return i;
	}
	return -1;
}

/*
Takes in the board state, a team ID, and a penalty type
Adds a new running penalty of the given type to the first available slot then re-sorts
Does not return anything
*/
static void add_penalty(ScoreboardState *board, TeamId team, PenaltyType type){
	int idx = find_empty_penalty_slot(board, team);
	if (idx < 0)
		return;

	board->penalties[(int)team][idx].used = 1;
	board->penalties[(int)team][idx].running = 1;
	board->penalties[(int)team][idx].type = type;
	board->penalties[(int)team][idx].remainingSeconds = penalty_default_seconds(type);

	sort_penalties(board, team);
}

/*
Takes in the board state, a team ID, and a slot index
Clears the penalty at the given index and re-sorts the slot array
Does not return anything
*/
static void delete_penalty(ScoreboardState *board, TeamId team, int idx){
	if (idx < 0 || idx >= MAX_TEAM_PENALTIES)
		return;
	board->penalties[(int)team][idx].used = 0;
	board->penalties[(int)team][idx].running = 0;
	board->penalties[(int)team][idx].remainingSeconds = 0;

	sort_penalties(board, team);
}

/*
Takes in the board state and a team ID
Insertion-sorts the penalty array so used entries come before unused, and among used,
ascending by remainingSeconds so slot 0 is always the shortest active penalty
Does not return anything
*/
static void sort_penalties(ScoreboardState *board, TeamId team){
	PenaltySlot *p = team_penalties(board, team);

	for (int i = 1; i < MAX_TEAM_PENALTIES; i++) {
		PenaltySlot key = p[i];
		int j = i - 1;

		while (j >= 0) {
			uint8_t should_shift = 0;

			// Used slots go before unused slots
			if (!p[j].used && key.used)
				should_shift = 1;
			// Among used slots, shorter remaining times go first
			else if (p[j].used && key.used && key.remainingSeconds < p[j].remainingSeconds)
				should_shift = 1;

			if (!should_shift)
				break;
			p[j + 1] = p[j];
			j--;
		}
		p[j + 1] = key;
	}
}

/*
Takes in the board state
Clears all penalty slots for both teams by zeroing the entire penalties array
Does not return anything
*/
static void clear_all_penalties(ScoreboardState *board){
	memset(board->penalties, 0, sizeof(board->penalties));
}

/*
Takes in the board state and the scoring team
Increments the scoring team's score, removes the shortest minor penalty from the penalized
team, and ends the game immediately if it is overtime
Does not return anything
*/
static void score_goal(ScoreboardState *board, TeamId scoringTeam){
	uint8_t scoreChanged = 0;

	if (gameComplete || overtimeChoicePageOpen) return;

	if (scoringTeam == TEAM_HOME) {
		if (board->homeScore < 99) {
			board->homeScore++;
			scoreChanged = 1;
		}
		remove_shortest_minor_penalty(board, TEAM_AWAY);
	} else {
		if (board->awayScore < 99) {
			board->awayScore++;
			scoreChanged = 1;
		}
		remove_shortest_minor_penalty(board, TEAM_HOME);
	}

	// Overtime is sudden death in both regular season and playoff modes
	if (overtimeActive && scoreChanged)
		finish_game(board);
}

/*
Takes in the board state and the penalized team
Finds and removes the shortest remaining minor penalty from the penalized team's slot array
Does not return anything
*/
static void remove_shortest_minor_penalty(ScoreboardState *board, TeamId penalizedTeam){
	PenaltySlot *p = team_penalties(board, penalizedTeam);
	int bestIdx = -1;

	for (int i = 0; i < MAX_TEAM_PENALTIES; i++) {
		if (!p[i].used || p[i].type != PENALTY_MINOR)
			continue;

		if (bestIdx < 0 || p[i].remainingSeconds < p[bestIdx].remainingSeconds)
			bestIdx = i;
	}

	if (bestIdx >= 0) {
		p[bestIdx].used = 0;
		p[bestIdx].running = 0;
		p[bestIdx].remainingSeconds = 0;
		sort_penalties(board, penalizedTeam);
	}
}

/*
Takes in the board state
If the clock is stopped, starts it; if running, opens the timeout page;
if the timeout page is already open, closes it; ignores the action if the game is complete
Does not return anything
*/
static void handle_clock_action(ScoreboardState *board){
	if (overtimeChoicePageOpen)
		return;

	if (timeoutPageOpen) {
		return_from_timeout_page(board);
		return;
	}

	if (board->page != PAGE_MAIN || gameComplete)
		return;

	if (!gameClockRunning) {
		if (board->gameSeconds > 0) {
			alarm_stop();
			gameClockRunning = 1;
		}
	} else
		open_timeout_page(board);
}

/*
Checks whether enough time has passed since the last accepted touch to debounce the screen
Returns 1 if the touch should be accepted, 0 if it is still within the debounce window
*/
static uint8_t touch_debounce_ok(void) {
	uint32_t now = HAL_GetTick();

	if ((now - lastTouchAcceptedMs) < TOUCH_DEBOUNCE_MS)
		return 0;

	lastTouchAcceptedMs = now;
	return 1;
}

/*
Takes in a GPIO port, pin, and debounce state struct for one button
Returns 1 the first time the button is held low after settling for BUTTON_DEBOUNCE_MS, 0 otherwise
*/
static uint8_t button_pressed_once(GPIO_TypeDef *port, uint16_t pin, DebouncedButton *button) {
	GPIO_PinState raw = HAL_GPIO_ReadPin(port, pin);
	uint32_t now = HAL_GetTick();

	if (raw != button->lastRaw) {
		button->lastRaw = raw;
		button->lastChangeMs = now;
	}

	if ((now - button->lastChangeMs) < BUTTON_DEBOUNCE_MS)
		return 0;

	// Buttons are wired active-low: RESET means pressed, SET means released
	if (raw == GPIO_PIN_RESET) {
		if (!button->pressLatched) {
			button->pressLatched = 1;
			return 1;
		}
	} else
		button->pressLatched = 0;

	return 0;
}

/*
Takes in the board state
Polls the three physical buttons and triggers the appropriate scoreboard action for each
Does not return anything
*/
static void handle_physical_buttons(ScoreboardState *board){
	if (overtimeChoicePageOpen)
		return;

	if (button_pressed_once(CLOCK_BUTTON_GPIO_Port, CLOCK_BUTTON_Pin, &clockButton))
		handle_clock_action(board);

	if (button_pressed_once(HOME_SCORE_BUTTON_GPIO_Port, HOME_SCORE_BUTTON_Pin, &homeScoreButton)) {
		score_goal(board, TEAM_HOME);

		if (!timeoutPageOpen && board->page == PAGE_MAIN) {
			Scoreboard_UpdateHomeScore(board);
			Scoreboard_UpdatePenalties(board);
		} else if (!timeoutPageOpen && board->page == PAGE_PENALTY_INPUT)
			Scoreboard_DrawPenaltyPage(board);
	}

	if (button_pressed_once(AWAY_SCORE_BUTTON_GPIO_Port, AWAY_SCORE_BUTTON_Pin, &awayScoreButton)) {
		score_goal(board, TEAM_AWAY);

		if (!timeoutPageOpen && board->page == PAGE_MAIN) {
			Scoreboard_UpdateAwayScore(board);
			Scoreboard_UpdatePenalties(board);
		} else if (!timeoutPageOpen && board->page == PAGE_PENALTY_INPUT)
			Scoreboard_DrawPenaltyPage(board);
	}
}

/*
Takes in the board state
Opens the timeout selection page and freezes the game clock and all penalty clocks
Does not return anything
*/
static void open_timeout_page(ScoreboardState *board){
	timeoutPageOpen = 1;
	timeoutCountdownActive = 0;
	timeoutSecondsRemaining = TIMEOUT_SECONDS;
	timeoutOwner = TEAM_HOME;

	// Freeze the game clock and all penalty clocks while this page is open
	gameClockRunning = 0;

	Scoreboard_DrawTimeoutPage(board, timeoutSecondsRemaining, timeoutCountdownActive, timeoutOwner);
}

/*
Takes in the board state
Closes the timeout page and returns to the main page; resumes the game clock if time remains
Does not return anything
*/
static void return_from_timeout_page(ScoreboardState *board){
	timeoutPageOpen = 0;
	timeoutCountdownActive = 0;
	timeoutSecondsRemaining = 0;

	if (!gameComplete && board->gameSeconds > 0)
		gameClockRunning = 1;
	board->page = PAGE_MAIN;

	Scoreboard_DrawMainPage(board);
}

/*
Takes in the board state and which team is calling the timeout
Deducts the team's timeout and starts the countdown; does nothing if a countdown is already running
Does not return anything
*/
static void start_team_timeout(ScoreboardState *board, TeamId team){
	if (timeoutCountdownActive)
		return;

	if (team == TEAM_HOME) {
		if (!board->homeTimeoutAvailable)
			return;
		board->homeTimeoutAvailable = 0;
	} else {
		if (!board->awayTimeoutAvailable)
			return;
		board->awayTimeoutAvailable = 0;
	}

	timeoutOwner = team;
	timeoutSecondsRemaining = TIMEOUT_SECONDS;
	timeoutCountdownActive = 1;

	Scoreboard_DrawTimeoutPage(board, timeoutSecondsRemaining, timeoutCountdownActive, timeoutOwner);
}

/*
Takes in the board state
Handles end-of-period logic: advances period 1 to 2, or triggers overtime/game-end after period 2
Does not return anything
*/
static void handle_period_expired(ScoreboardState *board) {
	gameClockRunning = 0;
	signal_clock_expired();

	if (board->period == 1) {
		// Advance to period 2 and restore timeouts for both teams
		board->period = 2;
		board->gameSeconds = TEST_PERIOD_SECONDS;
		board->homeTimeoutAvailable = 1;
		board->awayTimeoutAvailable = 1;

		// Period number and timeout indicators are not in the partial refresh path,
		// so force one full main-page redraw
		if (!timeoutPageOpen && board->page == PAGE_MAIN)
			mainPageNeedsFullRedraw = 1;
		return;
	}

	if (board->period == 2) {
		// Tied game goes to overtime selection; otherwise end the game
		if (board->homeScore == board->awayScore)
			open_overtime_choice_page(board);
		else
			finish_game(board);
		return;
	}

	if (overtimeActive) {
		if (board->homeScore != board->awayScore) {
			finish_game(board);
			return;
		}

		if (overtimeMode == OVERTIME_REGULAR_SEASON) {
			// No shootout in this project: end the game if regular-season OT stays tied
			finish_game(board);
			return;
		}

		if (overtimeMode == OVERTIME_PLAYOFFS) {
			// Repeat 15-minute playoff OT periods until a goal is scored
			board->period++;
			board->gameSeconds = PLAYOFF_OT_SECONDS;
			board->homeTimeoutAvailable = 0;
			board->awayTimeoutAvailable = 0;

			if (!timeoutPageOpen && board->page == PAGE_MAIN)
				mainPageNeedsFullRedraw = 1;
			else {
				board->page = PAGE_MAIN;
				Scoreboard_DrawMainPage(board);
			}
		}
	}
}

/*
Takes in the board state
Stops all clocks, clears all penalties, and draws the final main page to signal game over
Does not return anything
*/
static void finish_game(ScoreboardState *board) {
	gameClockRunning = 0;
	gameComplete = 1;

	timeoutPageOpen = 0;
	timeoutCountdownActive = 0;
	timeoutSecondsRemaining = 0;
	overtimeChoicePageOpen = 0;
	overtimeActive = 0;

	clear_all_penalties(board);
	board->gameSeconds = 0;
	board->page = PAGE_MAIN;

	mainPageNeedsFullRedraw = 0;
	Scoreboard_DrawMainPage(board);
}

/*
Takes in the board state
Stops the game clock and shows the overtime type selection screen
Does not return anything
*/
static void open_overtime_choice_page(ScoreboardState *board){
	gameClockRunning = 0;
	overtimeChoicePageOpen = 1;
	overtimeActive = 0;
	overtimeMode = OVERTIME_NONE;

	timeoutPageOpen = 0;
	timeoutCountdownActive = 0;
	timeoutSecondsRemaining = 0;

	board->period++;
	board->homeTimeoutAvailable = 0;
	board->awayTimeoutAvailable = 0;
	board->gameSeconds = 0;
	board->page = PAGE_MAIN;

	mainPageNeedsFullRedraw = 0;
	Scoreboard_DrawOvertimeChoicePage(board);
}

/*
Takes in the board state and the selected overtime mode
Starts overtime with the appropriate duration; clock does not run until the user presses Start
Does not return anything
*/
static void start_overtime(ScoreboardState *board, OvertimeMode mode){
	overtimeChoicePageOpen = 0;
	overtimeActive = 1;
	overtimeMode = mode;

	timeoutPageOpen = 0;
	timeoutCountdownActive = 0;
	timeoutSecondsRemaining = 0;

	board->homeTimeoutAvailable = 0;
	board->awayTimeoutAvailable = 0;
	board->gameSeconds = (mode == OVERTIME_REGULAR_SEASON) ? REGULAR_SEASON_OT_SECONDS : PLAYOFF_OT_SECONDS;
	board->page = PAGE_MAIN;

	gameClockRunning = 0;
	mainPageNeedsFullRedraw = 0;
	Scoreboard_DrawMainPage(board);
}

// Turns the alarm LED on
static void alarm_led_on(void){
	HAL_GPIO_WritePin(ALARM_LED_GPIO_Port, ALARM_LED_Pin, GPIO_PIN_SET);
}

// Turns the alarm LED off
static void alarm_led_off(void){
	HAL_GPIO_WritePin(ALARM_LED_GPIO_Port, ALARM_LED_Pin, GPIO_PIN_RESET);
}

/*
Turns on the alarm LED and starts the buzzer PWM; sets the auto-stop timestamp
Does not return anything
*/
static void alarm_start(void){
	alarm_led_on();

	__HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, (BUZZER_TIMER_PERIOD + 1U) / 2U);
	HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1); // best-effort; LED stays on even if PWM fails

	alarmActive   = 1;
	alarmStopTimeMs = HAL_GetTick() + ALARM_DURATION_MS;
}

/*
Stops the buzzer PWM and turns off the alarm LED
Does not return anything
*/
static void alarm_stop(void){
	HAL_TIM_PWM_Stop(&htim3, TIM_CHANNEL_1);
	__HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 0);
	alarm_led_off();
	alarmActive = 0;
}

/*
Checks if the alarm duration has elapsed and stops the alarm if so
Does not return anything
*/
static void alarm_update(void){
	if (alarmActive && ((int32_t)(HAL_GetTick() - alarmStopTimeMs) >= 0))
		alarm_stop();
}

// Triggers the end-of-period alarm
static void signal_clock_expired(void){
	alarm_start();
}

/*
Takes in the board state and which team's penalty page to open
Sets the board page to penalty input and redraws
Does not return anything
*/
static void open_penalty_page(ScoreboardState *board, TeamId team){
	board->page = PAGE_PENALTY_INPUT;
	board->penaltyPageTeam = team;
	Scoreboard_DrawPenaltyPage(board);
}

/*
Takes in the board state
Switches back to the main page and redraws it
Does not return anything
*/
static void return_to_main_page(ScoreboardState *board){
	board->page = PAGE_MAIN;
	Scoreboard_DrawMainPage(board);
}/* USER CODE END 4 */

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
  }/* USER CODE END Error_Handler_Debug */
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
	 ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) *//* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
