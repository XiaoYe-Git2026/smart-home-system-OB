# 🏠 智能家居系统

基于 **OneNET 物联网平台**的智能家居系统，采用 Django 上位机 + STM32 下位机架构，实现传感器数据采集、远程控制和智能预测。

## 📐 系统架构

```
┌─────────────────────────────────────────────────────────────┐
│                        浏览器前端                            │
│            实时数据展示 / 手动控制 / 预测面板                  │
└──────────────────────────┬──────────────────────────────────┘
                           │ HTTP
┌──────────────────────────▼──────────────────────────────────┐
│                   Django 上位机 (Host Computer)              │
│         OneNET HTTP API 轮询 / ML 温度预测 / 自动控制        │
└──────────────────────────┬──────────────────────────────────┘
                           │ HTTPS
┌──────────────────────────▼──────────────────────────────────┐
│                     OneNET 物联网平台                        │
│              物模型管理 / 数据存储 / 消息转发                  │
└──────────────────────────┬──────────────────────────────────┘
                           │ MQTT
┌──────────────────────────▼──────────────────────────────────┐
│              STM32 下位机 (Subordinate Device)               │
│      ESP8266 WiFi / DHT11 温度 / 光敏电阻 / PWM 风扇 / LED  │
└─────────────────────────────────────────────────────────────┘
```

## 📁 项目结构

```
智能家居系统/
├── Host Computer/              # Django 上位机
│   ├── smart_project/          # Django 项目配置
│   │   ├── settings.py         # OneNET 配置 + 系统设置
│   │   └── urls.py             # 主路由
│   ├── dashboard/              # 核心应用
│   │   ├── onenet.py           # OneNET HTTP API 客户端
│   │   ├── ml_predict.py       # ML 温度预测模块
│   │   ├── views.py            # API 接口 + 视图
│   │   ├── models.py           # 数据模型
│   │   ├── templates/          # 前端页面
│   │   └── static/             # CSS 样式
│   ├── generate_token.py       # OneNET Token 生成工具
│   ├── manage.py
│   └── requirements.txt
│
├── Subordinate Device/         # STM32 下位机
│   ├── Core/
│   │   ├── Inc/                # 头文件
│   │   │   ├── main.h          # 引脚定义
│   │   │   └── onenet.h        # OneNET 配置
│   │   └── Src/                # 源文件
│   │       ├── main.c          # 主程序 (MQTT通信/传感器/控制)
│   │       ├── dht11.c         # 温湿度传感器驱动
│   │       ├── adc.c           # ADC 光照采集
│   │       └── tim.c           # TIM4 PWM 风扇控制
│   ├── Drivers/                # STM32 HAL 库
│   └── MDK-ARM/                # Keil 工程文件
│
└── README.md
```

## 🛠️ 硬件配置

| 组件 | 型号/引脚 | 功能 |
|------|----------|------|
| MCU | STM32F103C8T6 | 主控制器 |
| WiFi | ESP8266 (USART3) | MQTT 通信 |
| 温度传感器 | DHT11 (PA0) | 温湿度采集 |
| 光照传感器 | 光敏电阻 (PA1 ADC) | 光照强度采集 |
| 风扇 | 直流电机 (PB6 PWM) | TIM4 CH1 PWM 调速 |
| LED 灯 | PA3 (低电平点亮) | 光照自动控制 |

## 📊 物模型属性

| 属性 | 标识符 | 类型 | 范围 | 读写 | 说明 |
|------|--------|------|------|------|------|
| 温度 | `temp` | float | 0~50 | 只读 | °C |
| 光照 | `light` | int32 | 0~5000 | 只读 | Lux |
| 风扇 | `fan_speed` | int32 | 0~1000 | 读写 | r/min，步长10 |
| LED | `led` | int32 | 0~1 | 只读 | 0=灭，1=亮 |

## 🚀 快速开始

### 1. 下位机（STM32）

1. 用 Keil MDK 打开 `Subordinate Device/MDK-ARM/YS.uvprojx`
2. 修改 `Core/Inc/main.h` 中的 WiFi 配置：
   ```c
   #define WIFI_SSID       "你的WiFi名"
   #define WIFI_PASSWORD   "你的WiFi密码"
   ```
3. 编译并烧录到 STM32 开发板

### 2. 上位机（Django）

```bash
# 进入上位机目录
cd "Host Computer"

# 安装依赖
pip install -r requirements.txt

# 配置 OneNET Token
# 编辑 smart_project/settings.py 中的 ONENET_TOKEN
# 或使用 Token 生成工具：
python generate_token.py

# 初始化数据库
python manage.py migrate

# 启动服务
python manage.py runserver
```

浏览器访问 `http://127.0.0.1:8000/`

## 🎛️ 功能说明

### 实时监控
- 温度、光照、风扇转速、LED 状态实时显示
- 3 秒自动轮询 OneNET 平台数据
- 温度趋势折线图

### 手动控制
- **风扇**：关闭 / 1/4 / 2/4 / 3/4 / 100% 五档按钮
- **LED**：光照 < 1000 Lux 自动点亮，≥ 1000 Lux 自动熄灭

### 🤖 温度预测（机器学习）
- 算法：`RandomForestRegressor`（scikit-learn）
- 特征：滑动窗口温度 + 时间特征
- 功能：预测下一时刻温度，建议风扇转速
- 自动模式：根据预测温度自动调节风扇

### 预测温度 → 风扇映射

| 预测温度 | 风扇转速 |
|---------|---------|
| ≥ 32°C | 1000 (全速) |
| 28~32°C | 400~1000 |
| 25~28°C | 0~400 |
| < 25°C | 0 (关闭) |

## 🔌 API 接口

| 接口 | 方法 | 说明 |
|------|------|------|
| `/api/sensor-data/` | GET | 轮询传感器数据 + LED 自动控制 |
| `/api/control/` | POST | 下发控制指令 `{"property":"fan_speed","value":500}` |
| `/api/history/` | GET | 历史数据 `?limit=50` |
| `/api/predict/` | GET | 温度预测 |
| `/api/train/` | POST | 重新训练模型 |
| `/api/auto-control/` | POST | 自动控制开关 `{"enabled":true}` |
| `/api/auto-control/status/` | GET | 自动控制状态 |

## ⚙️ OneNET 配置

| 配置项 | 值 |
|--------|-----|
| 产品ID | `SSX0I7L3I2` |
| 设备名 | `ESP8266` |
| MQTT 服务器 | `mqtts.heclouds.com:1883` |
| API 域名 | `iot-api.heclouds.com` |

Token 生成工具：`python generate_token.py`

## 📦 依赖

**上位机：**
- Python 3.10+
- Django 4.2+
- requests
- scikit-learn
- numpy

**下位机：**
- STM32CubeMX (`.ioc` 工程)
- Keil MDK-ARM
- STM32F1xx HAL 库

## 📝 开发日志

- ✅ OneNET MQTT 连接 + 属性上报
- ✅ OneNET HTTP API 轮询
- ✅ 风扇 PWM 调速控制
- ✅ LED 光照自动控制
- ✅ 温度预测 (RandomForest)
- ✅ 自动风扇控制模式
- ✅ 前端实时数据展示
- ✅ 控制冷却期防抖

## 📄 License

MIT License
