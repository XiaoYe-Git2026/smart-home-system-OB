#include "dht11.h"
#include "tim.h"

/* 延时函数 (基于定时器) */
static void DHT11_Delay_us(uint16_t us)
{
    __HAL_TIM_SET_COUNTER(&htim4, 0);
    HAL_TIM_Base_Start(&htim4);
    while (__HAL_TIM_GET_COUNTER(&htim4) < us);
    HAL_TIM_Base_Stop(&htim4);
}

/* 设置DHT11引脚为输出模式 */
static void DHT11_Pin_Output(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = DHT11_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(DHT11_GPIO_Port, &GPIO_InitStruct);
}

/* 设置DHT11引脚为输入模式 */
static void DHT11_Pin_Input(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = DHT11_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(DHT11_GPIO_Port, &GPIO_InitStruct);
}

/* 读取DHT11引脚状态 */
static uint8_t DHT11_Read_Pin(void)
{
    return HAL_GPIO_ReadPin(DHT11_GPIO_Port, DHT11_Pin);
}

/* DHT11复位 */
void DHT11_Rst(void)
{
    DHT11_Pin_Output();
    HAL_GPIO_WritePin(DHT11_GPIO_Port, DHT11_Pin, GPIO_PIN_RESET);
    HAL_Delay(20);
    HAL_GPIO_WritePin(DHT11_GPIO_Port, DHT11_Pin, GPIO_PIN_SET);
    DHT11_Delay_us(30);
}

/* 检测DHT11响应 */
uint8_t DHT11_Check(void)
{
    uint8_t retry = 0;
    DHT11_Pin_Input();
    while (DHT11_Read_Pin() && retry < 100) {
        retry++;
        DHT11_Delay_us(1);
    }
    if (retry >= 100) return 1;
    retry = 0;
    while (!DHT11_Read_Pin() && retry < 100) {
        retry++;
        DHT11_Delay_us(1);
    }
    if (retry >= 100) return 1;
    return 0;
}

/* 读取一个字节 */
uint8_t DHT11_Read_Byte(void)
{
    uint8_t i, dat = 0;
    for (i = 0; i < 8; i++) {
        dat <<= 1;
        while (DHT11_Read_Pin());
        while (!DHT11_Read_Pin());
        DHT11_Delay_us(40);
        if (DHT11_Read_Pin()) {
            dat |= 0x01;
        }
    }
    return dat;
}

/* 初始化DHT11 */
uint8_t DHT11_Init(void)
{
    DHT11_Rst();
    return DHT11_Check();
}

/* 读取温湿度数据 (带小数) */
uint8_t DHT11_Read_Data(float *temp, float *humi)
{
    uint8_t buf[5];
    uint8_t i;
    DHT11_Rst();
    if (DHT11_Check() == 0) {
        for (i = 0; i < 5; i++) {
            buf[i] = DHT11_Read_Byte();
        }
        if ((buf[0] + buf[1] + buf[2] + buf[3]) == buf[4]) {
            *humi = (float)buf[0] + (float)buf[1] / 10.0f;
            *temp = (float)buf[2] + (float)buf[3] / 10.0f;
        }
    } else {
        return 1;
    }
    return 0;
}
