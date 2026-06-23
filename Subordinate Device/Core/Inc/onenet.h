#ifndef __ONENET_H
#define __ONENET_H

#include <stdint.h>

/* ==================== OneNET 平台配置 ==================== */

/* 产品与设备信息 */
#define PRODUCT_ID      "SSX0I7L3I2"
#define DEVICE_NAME     "ESP8266"
#define MQTT_SERVER     "mqtts.heclouds.com"
#define MQTT_PORT       1883          /* 非TLS端口 */
#define MQTT_TOKEN      "version=2018-10-31&res=products%2FSSX0I7L3I2%2Fdevices%2FESP8266&et=1907496286&method=md5&sign=VZXmOe0gX%2BUzHFoSndDYag%3D%3D"

/* ==================== MQTT 主题格式 ==================== */

/* 上报属性 (POST) */
#define TOPIC_PROPERTY_POST     "$sys/" PRODUCT_ID "/" DEVICE_NAME "/thing/property/post"

/* 属性设置 (SET) — 云端下发控制指令 */
#define TOPIC_PROPERTY_SET      "$sys/" PRODUCT_ID "/" DEVICE_NAME "/thing/property/set"

/* 上报回复 (POST REPLY) */
#define TOPIC_PROPERTY_POST_REPLY   "$sys/" PRODUCT_ID "/" DEVICE_NAME "/thing/property/post/reply"

/* ==================== 属性定义 ==================== */

/*
 * 窗帘 curtain_step - int32, 0-100, 步长10, 单位：步
 * 风扇 fan_speed    - int32, 0-1000, 步长10, 单位：r/min
 * 光照 light        - int32, 0-5000, 单位：Lux, 只读
 * 温度 temp         - float, 0-50.0, 单位：°C, 只读
 */

/* ==================== 连接状态枚举 ==================== */

typedef enum {
    ONENET_DISCONNECTED,
    ONENET_CONNECTING,
    ONENET_CONNECTED,
    ONENET_ERROR
} OneNET_StateTypeDef;

/* ==================== 工具宏 ==================== */

/* JSON 缓冲区大小 */
#define ONENET_JSON_BUF_SIZE    512

#endif /* __ONENET_H */
