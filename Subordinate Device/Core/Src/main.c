/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : 智能家居系统 - OneNET 云平台
  *                   温度(temp) + 光照(light) + 风扇(fan_speed) + 警报器(alarm)
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "dht11.h"
#include "onenet.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* WiFi 配置 */
#define WIFI_SSID       "abc"
#define WIFI_PASSWORD   "abc123456"

/* 温度阈值 */
#define TEMP_THRESHOLD  30.0f

/* 上报间隔 (毫秒) */
#define REPORT_INTERVAL 3000

/* DHT11 返回值 */
#define DHT11_OK    0
#define DHT11_ERROR 1

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

uint8_t usart3_rx_buffer[500];
uint16_t usart3_rx_index = 0;
uint8_t usart3_rx_done = 0;

/* 连接状态 */
typedef enum {
    STATE_AT_TEST,
    STATE_SET_MODE,
    STATE_WAIT_WIFI,
    STATE_MQTT_USER,
    STATE_MQTT_CONN,
    STATE_MQTT_SUB,
    STATE_READY
} ConnectState;

ConnectState current_state = STATE_AT_TEST;
uint32_t last_report_time = 0;
uint32_t last_resub_time = 0;
#define RESUB_INTERVAL  30000  /* 每30秒重新订阅一次 */

/* ==================== 传感器数据 (只读) ==================== */
float    temperature = 0.0f;      /* 温度 temp (°C) */
uint16_t light_value = 0;         /* 光照 light (Lux) */

/* ==================== 设备控制 (读写) ==================== */
int16_t  fan_speed = 0;           /* 风扇转速 fan_speed (0-1000 r/min) */
uint8_t  led_on = 0;              /* LED led (0=灭, 1=亮) */

/* 连接状态 */
uint8_t wifi_connected = 0;
uint8_t onenet_connected = 0;
uint8_t dht11_error = 0;

/* 多行MQTT消息暂存 (处理某些AT固件分两行下发) */
uint8_t  pending_set_payload = 0;
char     set_payload_buf[ONENET_JSON_BUF_SIZE];

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
void SendCmd(const char* cmd);
void CheckResponse(const char* resp);
void ReportAllData(void);
void ReadSensors(void);
void ControlFan(int16_t speed);
void ParseSetCommand(const char* resp);
uint16_t ReadLightSensor(void);
void ControlLedByLight(void);
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
  MX_TIM4_Init();
  MX_ADC1_Init();
  MX_USART3_UART_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */

  /* 启动 TIM4 PWM (风扇) */
  HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_1);

  /* 初始化 DHT11 */
  DHT11_Init();

  /* 初始状态：风扇关闭，LED 默认亮（高电平） */
  ControlFan(0);
  HAL_GPIO_WritePin(IN1_GPIO_Port, IN1_Pin, GPIO_PIN_SET);  /* PA3 输出高电平，LED 灭（低电平点亮） */

  const char* msg = "\r\n=== Smart Home (temp+light+fan+led) ===\r\n";
  HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), 100);

  /* 开始连接 OneNET */
  HAL_UART_Receive_IT(&huart3, usart3_rx_buffer, 1);
  SendCmd("AT\r\n");

  /* 首次读取传感器 */
  ReadSensors();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    if (usart3_rx_done) {
        usart3_rx_done = 0;
        HAL_UART_Transmit(&huart1, (uint8_t*)usart3_rx_buffer, usart3_rx_index, 100);
        CheckResponse((const char*)usart3_rx_buffer);
        usart3_rx_index = 0;
    }

    if (current_state == STATE_READY) {
        /* 定期重新订阅 set 主题，防止订阅丢失 */
        if (HAL_GetTick() - last_resub_time >= RESUB_INTERVAL) {
            last_resub_time = HAL_GetTick();
            char sub_cmd[200];
            sprintf(sub_cmd, "AT+MQTTSUB=0,\"$sys/%s/%s/thing/property/set\",0\r\n", PRODUCT_ID, DEVICE_NAME);
            SendCmd(sub_cmd);
        }

        if (HAL_GetTick() - last_report_time >= REPORT_INTERVAL) {
            last_report_time = HAL_GetTick();
            ReadSensors();
            ControlLedByLight();  /* 根据光照自动控制 LED */
            ReportAllData();
        }
    }

    HAL_Delay(10);

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
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
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

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/**
  * @brief  读取光照传感器 (PA1, ADC1_CH1)
  * @retval ADC值转换为 Lux (0-5000)
  */
uint16_t ReadLightSensor(void)
{
    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 100);
    uint16_t adc_value = HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);

    /* 反转映射：ADC值越小，光照越强 (光敏电阻分压电路) */
    return (uint16_t)((uint32_t)(4095 - adc_value) * 5000 / 4095);
}

/**
  * @brief  发送AT指令到ESP8266
  */
void SendCmd(const char* cmd)
{
    HAL_UART_Transmit(&huart3, (uint8_t*)cmd, strlen(cmd), 100);
    HAL_UART_Transmit(&huart1, (uint8_t*)cmd, strlen(cmd), 100);
}

/**
  * @brief  检查AT响应并处理状态机
  */
void CheckResponse(const char* resp)
{
    if (current_state == STATE_AT_TEST && strstr(resp, "OK") != NULL) {
        current_state = STATE_SET_MODE;
        SendCmd("AT+CWMODE=1\r\n");
        return;
    }

    if (current_state == STATE_SET_MODE && strstr(resp, "OK") != NULL) {
        current_state = STATE_WAIT_WIFI;
        char wifi_cmd[100];
        sprintf(wifi_cmd, "AT+CWJAP=\"%s\",\"%s\"\r\n", WIFI_SSID, WIFI_PASSWORD);
        SendCmd(wifi_cmd);
        return;
    }

    if (current_state == STATE_WAIT_WIFI) {
        if (strstr(resp, "WIFI CONNECTED") != NULL || strstr(resp, "GOT IP") != NULL) {
            wifi_connected = 1;
            const char* succ = "\r\n[INFO] WiFi connected\r\n";
            HAL_UART_Transmit(&huart1, (uint8_t*)succ, strlen(succ), 100);
            current_state = STATE_MQTT_USER;
            /* scheme=1 表示 MQTT over TCP，使用 1883 端口 */
            SendCmd("AT+MQTTUSERCFG=0,1,\"" DEVICE_NAME "\",\"" PRODUCT_ID "\",\"" MQTT_TOKEN "\",0,0,\"\"\r\n");
        }
        return;
    }

    if (current_state == STATE_MQTT_USER && strstr(resp, "OK") != NULL) {
        current_state = STATE_MQTT_CONN;
        char mqtt_cmd[150];
        sprintf(mqtt_cmd, "AT+MQTTCONN=0,\"%s\",%d,1\r\n", MQTT_SERVER, MQTT_PORT);
        SendCmd(mqtt_cmd);
        return;
    }

    if (current_state == STATE_MQTT_CONN && strstr(resp, "CONNECT") != NULL) {
        onenet_connected = 1;
        current_state = STATE_MQTT_SUB;

        /* 订阅属性设置主题 */
        char sub_cmd[200];
        sprintf(sub_cmd, "AT+MQTTSUB=0,\"$sys/%s/%s/thing/property/set\",0\r\n", PRODUCT_ID, DEVICE_NAME);
        SendCmd(sub_cmd);
        HAL_Delay(500);

        /* 订阅上报回复主题 (用于确认平台是否收到) */
        sprintf(sub_cmd, "AT+MQTTSUB=0,\"$sys/%s/%s/thing/property/post/reply\",0\r\n", PRODUCT_ID, DEVICE_NAME);
        SendCmd(sub_cmd);

        return;
    }

    if (current_state == STATE_MQTT_SUB && strstr(resp, "OK") != NULL) {
        current_state = STATE_READY;
        last_report_time = HAL_GetTick();

        const char* succ = "\r\n=== OneNET Ready! ===\r\n";
        HAL_UART_Transmit(&huart1, (uint8_t*)succ, strlen(succ), 100);

        /* 打印配置信息 */
        char config_msg[300];
        sprintf(config_msg, "[CONFIG] Server: %s:%d\r\n[CONFIG] Product: %s\r\n[CONFIG] Device: %s\r\n",
                MQTT_SERVER, MQTT_PORT, PRODUCT_ID, DEVICE_NAME);
        HAL_UART_Transmit(&huart1, (uint8_t*)config_msg, strlen(config_msg), 100);

        return;
    }

    /* 处理云端下发指令 (只处理set消息，忽略set_reply) */
    if (strstr(resp, "thing/property/set") != NULL && strstr(resp, "set_reply") == NULL) {
        /* 单行格式：+MQTTPUBRECV:0,"topic",len,{...} — 包含JSON负载 */
        if (strstr(resp, "{") != NULL) {
            ParseSetCommand(resp);
        } else {
            /* 多行格式：+MQTTPUBRECV:0,"topic",len\n
                         {...}                          — 负载在下一行 */
            pending_set_payload = 1;
            set_payload_buf[0] = '\0';
        }
        return;
    }

    /* 处理多行MQTT消息的下一行（纯JSON负载行） */
    if (pending_set_payload && resp[0] == '{') {
        pending_set_payload = 0;
        strncpy(set_payload_buf, resp, sizeof(set_payload_buf) - 1);
        set_payload_buf[sizeof(set_payload_buf) - 1] = '\0';
        ParseSetCommand(set_payload_buf);
        return;
    }

    /* 打印收到的 MQTT 消息 (调试用) */
    if (strstr(resp, "MQTTPUBRECV") != NULL || strstr(resp, "post/reply") != NULL) {
        char debug_msg[200];
        sprintf(debug_msg, "\r\n[MQTT RECV] %s\r\n", resp);
        HAL_UART_Transmit(&huart1, (uint8_t*)debug_msg, strlen(debug_msg), 100);
    }
}

/**
  * @brief  解析云端下发的控制指令
  *
  * 属性说明：
  *   fan_speed : 0-1000 r/min (步长10)
  *   alarm     : 0=关, 1=开
  */
void ParseSetCommand(const char* resp)
{
    char log_msg[200];
    sprintf(log_msg, "\r\n[DEBUG] Received: %s\r\n", resp);
    HAL_UART_Transmit(&huart1, (uint8_t*)log_msg, strlen(log_msg), 100);

    /* 提取原始 id — OneNET 要求 set_reply 的 id 必须与 set 请求一致 */
    char msg_id[32] = "0";
    const char *id_ptr = strstr(resp, "\"id\"");
    if (id_ptr) {
        /* 跳过 "id":" 或 "id": " */
        const char *val_start = strchr(id_ptr + 4, '"');
        if (val_start) {
            val_start++;  /* 跳过前引号 */
            const char *val_end = strchr(val_start, '"');
            if (val_end && (val_end - val_start) < (int)sizeof(msg_id) - 1) {
                strncpy(msg_id, val_start, val_end - val_start);
                msg_id[val_end - val_start] = '\0';
            }
        }
    }

    char id_log[60];
    sprintf(id_log, "\r\n[ID] %s\r\n", msg_id);
    HAL_UART_Transmit(&huart1, (uint8_t*)id_log, strlen(id_log), 100);

    /* 解析风扇转速 - OneNET set 格式: "fan_speed":500 */
    if (strstr(resp, "\"fan_speed\"") != NULL) {
        char *ptr = strstr(resp, "\"fan_speed\":");
        if (ptr) {
            ptr += 12;  /* 跳过 "fan_speed": */
            int16_t speed = atoi(ptr);
            /* 限制范围 0-1000，步长10 */
            if (speed < 0) speed = 0;
            if (speed > 1000) speed = 1000;
            speed = (speed / 10) * 10;  /* 对齐步长 */

            char cmd_log[60];
            sprintf(cmd_log, "\r\n[PARSE] fan_speed = %d\r\n", speed);
            HAL_UART_Transmit(&huart1, (uint8_t*)cmd_log, strlen(cmd_log), 100);

            ControlFan(speed);
        }
    }

    /* LED 由光照自动控制，无需手动解析 */

    /* 发送回复 — 使用原始 msg_id */
    char reply[128];
    sprintf(reply, "{\"id\":\"%s\",\"code\":200,\"msg\":\"success\"}", msg_id);

    char cmd_buf[256];
    sprintf(cmd_buf, "AT+MQTTPUBRAW=0,\"$sys/%s/%s/thing/property/set_reply\",%d,0,0\r\n",
            PRODUCT_ID, DEVICE_NAME, strlen(reply));

    HAL_UART_Transmit(&huart3, (uint8_t*)cmd_buf, strlen(cmd_buf), 100);
    HAL_Delay(100);
    HAL_UART_Transmit(&huart3, (uint8_t*)reply, strlen(reply), 200);
    HAL_UART_Transmit(&huart3, (uint8_t*)"\r\n", 2, 100);
}

/**
  * @brief  读取所有传感器
  */
void ReadSensors(void)
{
    /* 1. 读取 DHT11 温度（带小数） */
    float temp, humi;
    if (DHT11_Read_Data(&temp, &humi) == DHT11_OK) {
        dht11_error = 0;
        temperature = temp;
        char debug_buf[60];
        sprintf(debug_buf, "\r\n[DHT11] Temp=%.1f C\r\n", temperature);
        HAL_UART_Transmit(&huart1, (uint8_t*)debug_buf, strlen(debug_buf), 100);
    } else {
        dht11_error = 1;
        char err_msg[] = "\r\n[WARN] DHT11 read failed!\r\n";
        HAL_UART_Transmit(&huart1, (uint8_t*)err_msg, strlen(err_msg), 100);
    }

    /* 2. 读取光照传感器 */
    light_value = ReadLightSensor();
    char light_buf[40];
    sprintf(light_buf, "[LIGHT] %d Lux\r\n", light_value);
    HAL_UART_Transmit(&huart1, (uint8_t*)light_buf, strlen(light_buf), 100);
}

/**
  * @brief  控制风扇转速 (PWM)
  * @param  speed: 0-1000 r/min
  */
void ControlFan(int16_t speed)
{
    fan_speed = speed;

    if (speed == 0) {
        /* 关闭 PWM 输出，强制拉低 PB6 */
        HAL_TIM_PWM_Stop(&htim4, TIM_CHANNEL_1);
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET);
    } else {
        /* 确保 PWM 已启动 */
        HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_1);
        /* 将 0-1000 映射到 PWM 占空比 0-100% */
        uint32_t pulse = (uint32_t)speed * htim4.Init.Period / 1000;
        __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_1, pulse);
    }

    char log_msg[60];
    sprintf(log_msg, "\r\n[CMD] Fan speed: %d r/min\r\n", speed);
    HAL_UART_Transmit(&huart1, (uint8_t*)log_msg, strlen(log_msg), 100);
}

/**
  * @brief  根据光照强度自动控制 LED (PA3，低电平点亮)
  *         光照 < 1000 Lux → LED 亮 (低电平)
  *         光照 >= 1000 Lux → LED 灭 (高电平)
  */
void ControlLedByLight(void)
{
    if (light_value < 1000) {
        HAL_GPIO_WritePin(IN1_GPIO_Port, IN1_Pin, GPIO_PIN_RESET);  /* LED 亮 (低电平) */
        led_on = 1;
    } else {
        HAL_GPIO_WritePin(IN1_GPIO_Port, IN1_Pin, GPIO_PIN_SET);    /* LED 灭 (高电平) */
        led_on = 0;
    }
}

/**
  * @brief  发送 MQTT 数据到 ESP8266
  * @param  topic: MQTT 主题
  * @param  payload: JSON 数据
  */
static void MQTT_Publish(const char* topic, const char* payload)
{
    char cmd_buf[256];
    uint16_t payload_len = strlen(payload);

    /* 构建 AT+MQTTPUBRAW 命令 */
    sprintf(cmd_buf, "AT+MQTTPUBRAW=0,\"%s\",%d,0,0\r\n", topic, payload_len);

    /* 先处理缓冲区中已有的数据（可能是控制指令） */
    if (usart3_rx_done) {
        CheckResponse((const char*)usart3_rx_buffer);
        usart3_rx_index = 0;
        usart3_rx_done = 0;
    }

    /* 仅在没有待处理数据时清空缓冲区 */
    usart3_rx_index = 0;
    usart3_rx_done = 0;

    /* 发送 AT 命令 */
    HAL_UART_Transmit(&huart3, (uint8_t*)cmd_buf, strlen(cmd_buf), 100);

    /* 等待 ESP8266 准备接收数据 */
    HAL_Delay(100);

    /* 发送 JSON 数据 */
    HAL_UART_Transmit(&huart3, (uint8_t*)payload, payload_len, 1000);

    /* 等待 ESP8266 处理完成 */
    HAL_Delay(100);

    /* 发布后检查是否有收到的 MQTT 消息 */
    if (usart3_rx_done) {
        CheckResponse((const char*)usart3_rx_buffer);
        usart3_rx_index = 0;
        usart3_rx_done = 0;
    }

    /* 打印发送的数据到调试串口 */
    HAL_UART_Transmit(&huart1, (uint8_t*)"[SEND] ", 7, 100);
    HAL_UART_Transmit(&huart1, (uint8_t*)payload, payload_len, 100);
    HAL_UART_Transmit(&huart1, (uint8_t*)"\r\n", 2, 100);
}

/**
  * @brief  上报所有数据到 OneNET
  *
  * 上报属性：
  *   temp      : float, 温度 (°C)
  *   light     : int32, 光照 (Lux)
  *   fan_speed : int32, 风扇转速 (r/min)
  *   alarm     : int32, 警报器 (0=关, 1=开)
  */
void ReportAllData(void)
{
    char json_data[500];
    char topic[128];

    /* 构建 JSON 数据 */
    /* 所有属性步长=1，直接上报整数
     * temp 步长=0.1 → 保留一位小数
     */
    sprintf(json_data,
        "{\"id\":\"%lu\",\"params\":{"
        "\"temp\":{\"value\":%.1f},"
        "\"light\":{\"value\":%d},"
        "\"fan_speed\":{\"value\":%d},"
        "\"led\":{\"value\":%d}}}",
        HAL_GetTick(),
        temperature,
        light_value,
        fan_speed,
        led_on);

    /* 构建主题 */
    sprintf(topic, "$sys/%s/%s/thing/property/post", PRODUCT_ID, DEVICE_NAME);

    /* 发送数据 */
    MQTT_Publish(topic, json_data);

    /* 打印日志 */
    char log_msg[200];
    sprintf(log_msg, "\r\n[UPLOAD] T=%.1fC Light=%dLux Fan=%d LED=%d\r\n",
            temperature, light_value, fan_speed, led_on);
    HAL_UART_Transmit(&huart1, (uint8_t*)log_msg, strlen(log_msg), 100);
}

/**
  * @brief  UART接收回调
  */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART3) {
        uint8_t data = usart3_rx_buffer[0];
        if (usart3_rx_index < sizeof(usart3_rx_buffer) - 1) {
            usart3_rx_buffer[usart3_rx_index++] = data;
            if (data == '\n') {
                usart3_rx_buffer[usart3_rx_index] = '\0';
                usart3_rx_done = 1;
            }
        }
        HAL_UART_Receive_IT(&huart3, usart3_rx_buffer, 1);
    }
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
