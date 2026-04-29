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
#include "dma.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ModuleBase.h"     /* 模块基类：Init / Run / Cleanup 等公有接口 */
#include "LED.h"            /* LED 子类：On / Off / Toggle / SetBrightness */
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/**
  * @brief LED1 实例（板载 LED：PC0，高电平点亮）
  * @note  定义为全局变量，便于在调试器中查看状态；
  *        也可声明为 main() 内的局部变量。
  */
static LED_t led1;

/* 演示状态机枚举 */
typedef enum {
    LED_DEMO_BLINK,         /* 阶段1：闪烁 - 演示 On/Off/Toggle */
    LED_DEMO_BREATHE_IN,    /* 阶段2：呼吸渐亮 - 演示 SetBrightness 递增 */
    LED_DEMO_BREATHE_OUT,   /* 阶段3：呼吸渐灭 - 演示 SetBrightness 递减 */
    LED_DEMO_STEADY         /* 阶段4：常亮 50% - 演示稳定 PWM */
} LED_DemoPhase_t;

static LED_DemoPhase_t demo_phase = LED_DEMO_BLINK;
static uint32_t phase_start_tick = 0;   /* 当前阶段起始时刻 */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* 私有函数声明：LED 演示阶段切换 */
static void LED_Demo_Update(LED_t *led);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/**
  * @brief  LED 功能演示状态机
  * @note   依次展示 LED 模块的四大核心功能：
  *         1. 闪烁（On/Off/Toggle）
  *         2. 渐亮（SetBrightness 0->100）
  *         3. 渐灭（SetBrightness 100->0）
  *         4. 常亮 50%（稳定 PWM 输出）
  *
  *         每个阶段持续 5 秒，循环往复。
  *
  * @param  led  指向 LED 对象的指针
  */
static void LED_Demo_Update(LED_t *led)
{
    uint32_t now       = HAL_GetTick();
    uint32_t elapsed   = now - phase_start_tick;  /* 当前阶段已运行时间 (ms) */
    uint32_t phase_dur = 5000;                     /* 每阶段持续 5 秒 */

    /* 阶段超时 -> 切换到下一阶段 */
    if (elapsed >= phase_dur) {
        demo_phase = (LED_DemoPhase_t)(((int)demo_phase + 1) % 4);
        phase_start_tick = now;
        elapsed = 0;
    }

    switch (demo_phase) {

    /* ========== 阶段1：闪烁 ========== */
    case LED_DEMO_BLINK: {
        /*
         * 演示 LED_On / LED_Off / LED_Toggle
         * 每 500ms 翻转一次，共 10 次闪烁
         */
        static uint32_t last_toggle = 0;
        if (elapsed - last_toggle >= 500) {
            last_toggle = elapsed;
            LED_Toggle(led);                    /* 调用翻转接口 */
        }
        break;
    }

    /* ========== 阶段2：渐亮 (0->100) ========== */
    case LED_DEMO_BREATHE_IN: {
        /*
         * 演示 LED_SetBrightness 递增
         * 5 秒内亮度从 0 线性增加到 100
         * 软件 PWM 在 Run() 中非阻塞刷新，此处仅设置目标值
         */
        LED_On(led);                            /* 确保 LED 点亮 */
        uint8_t brightness = (uint8_t)(elapsed * 100 / phase_dur);
        LED_SetBrightness(led, brightness);     /* 设置亮度 */
        break;
    }

    /* ========== 阶段3：渐灭 (100->0) ========== */
    case LED_DEMO_BREATHE_OUT: {
        /*
         * 演示 LED_SetBrightness 递减
         * 5 秒内亮度从 100 线性减小到 0
         */
        LED_On(led);
        uint8_t brightness = (uint8_t)(100 - elapsed * 100 / phase_dur);
        LED_SetBrightness(led, brightness);
        break;
    }

    /* ========== 阶段4：常亮 50% ========== */
    case LED_DEMO_STEADY: {
        /*
         * 演示稳定的软件 PWM 输出
         * 亮度固定在 50%，由 Run() 维持 PWM 波形
         */
        LED_On(led);
        LED_SetBrightness(led, 50);             /* 50% 亮度 */
        break;
    }

    default:
        break;
    }
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
  MX_USART1_UART_Init();
  MX_TIM1_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  MX_TIM8_Init();
  /* USER CODE BEGIN 2 */

  /*
   * =============================================
   *  LED 模块初始化
   * =============================================
   * 接线信息：
   *   - LED1: PC0, GPIO_PIN_0, 高电平点亮
   *   - 该引脚已在 MX_GPIO_Init() 中配置为推挽输出
   *
   * 初始化流程（与 ModuleBase 规范一致）：
   *   1. LED_Constructor -- 构造子类对象，注册虚函数表
   *   2. ModuleBase_Init  -- 通过 vtable 调用 LED_init，
   *                          验证引脚合法性并初始熄灭
   */
  LED_Constructor(&led1, LED1_GPIO_Port, LED1_Pin, 1);  /* 构造：PC0, 高电平亮 */
  if (ModuleBase_Init((ModuleBase_t *)&led1) != 0) {
      Error_Handler();   /* 初始化失败（如端口/引脚参数无效） */
  }

  /* 记录演示起始时间 */
  phase_start_tick = HAL_GetTick();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /*
     * =============================================
     *  1. 非阻塞软件 PWM 刷新（必须周期性调用）
     * =============================================
     *
     * ModuleBase_Run() 通过虚函数表调用 LED_run()，
     * 后者基于 HAL_GetTick() 计算 PWM 相位，
     * 实时调整 GPIO 输出电平以维持目标亮度。
     *
     * 若不调用此函数，亮度 < 100% 时将无法维持 PWM 波形。
     * 调用频率建议 >= 200Hz（即周期 <= 5ms），
     * 以保证 10ms PWM 周期有足够的刷新分辨率。
     */
    ModuleBase_Run((ModuleBase_t *)&led1);

    /*
     * =============================================
     *  2. LED 演示状态机更新（每循环一次）
     * =============================================
     *
     * 依次演示以下功能（每阶段 5 秒，循环）：
     *   - LED_Toggle()        : 开关翻转
     *   - LED_SetBrightness() : 亮度 0->100（渐亮）
     *   - LED_SetBrightness() : 亮度 100->0（渐灭）
     *   - LED_SetBrightness() : 固定 50%（稳态 PWM）
     */
    LED_Demo_Update(&led1);

    /*
     * =============================================
     *  3. 其他周期任务（如按键检测、传感器读取等）
     * =============================================
     *
     * 示例：检测 KEY1（PC1，低电平有效）
     * 按键按下时切换 LED 开关状态：
     *
     * if (HAL_GPIO_ReadPin(KEY1_GPIO_Port, KEY1_Pin) == GPIO_PIN_RESET) {
     *     HAL_Delay(20);  // 消抖
     *     LED_Toggle(&led1);
     *     while (HAL_GPIO_ReadPin(KEY1_GPIO_Port, KEY1_Pin) == GPIO_PIN_RESET);
     * }
     */
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
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

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
