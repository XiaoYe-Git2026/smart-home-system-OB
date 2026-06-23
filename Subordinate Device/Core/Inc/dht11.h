#ifndef __DHT11_H
#define __DHT11_H

#include "main.h"

/* 使用 main.h 中的定义 */
/* DHT11_Pin = GPIO_PIN_11 */
/* DHT11_GPIO_Port = GPIOA */

/* 函数声明 */
uint8_t DHT11_Init(void);
uint8_t DHT11_Read_Data(float *temp, float *humi);
uint8_t DHT11_Read_Byte(void);
uint8_t DHT11_Check(void);
void DHT11_Rst(void);

#endif /* __DHT11_H */
