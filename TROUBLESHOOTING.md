# 🔧 问题排查与解决方案

本文档记录了开发过程中遇到的所有问题及对应的解决方案。

---

## 问题 1：OneNET HTTP API Token 认证失败

**现象**：Django 调用 OneNET API 返回 `authentication failed`

**原因**：
- 下位机使用的是 **MQTT Token**（`res=products/.../devices/...`）
- HTTP API 需要 **产品级 Token**（`res=products/...`）
- 两者的 `res` 资源标识不同，不能混用

**解决**：
```python
# 错误 - 使用设备级 res
res = "products/SSX0I7L3I2/devices/ESP8266"

# 正确 - 使用产品级 res
res = "products/SSX0I7L3I2"
```

Token 生成算法：
```python
string_for_sign = f"{et}\n{method}\n{res}\n{version}"
sign = base64(hmac_sha1(string_for_sign, api_key))
token = f"version={version}&res={url_encode(res)}&et={et}&method={method}&sign={url_encode(sign)}"
```

---

## 问题 2：Token 过期时间太长

**现象**：`authentication failed:token expire is too long,max expire limit 10 years`

**原因**：`et=2147483647`（2038年）超过了平台限制的 10 年

**解决**：将过期时间设置为 9 年内
```python
et = str(int(time.time()) + 9 * 365 * 24 * 3600)
```

---

## 问题 3：OneNET API 返回 code 不一致

**现象**：代码判断 `code == 200` 但实际返回 `code == 0`

**原因**：OneNET 新版 API 成功返回的 code 是 `0`，不是 `200`

**解决**：
```python
# 错误
if data.get("code") == 200:

# 正确
if data.get("code") == 0:
```

---

## 问题 4：OneNET 设置属性 params 格式错误

**现象**：`set-device-property` 返回 `invalid request parameter`

**原因**：`params` 应该是**对象**，不是数组

**解决**：
```python
# 错误 - 数组格式
"params": [{"identifier": "fan_speed", "value": 500}]

# 正确 - 对象格式
"params": {"fan_speed": 500}
```

---

## 问题 5：设备响应超时 (code=10411)

**现象**：OneNET 返回 `code=10411, msg=设备不响应超时`

**原因**：OneNET 等待设备 `set_reply` 超时。设备收到了指令并执行，但回复的 `id` 不匹配

**解决**（下位机 main.c）：
```c
// 错误 - 使用 HAL_GetTick() 作为 id
sprintf(reply, "{\"id\":\"%lu\",\"code\":200}", HAL_GetTick());

// 正确 - 从原始消息中提取 id
char msg_id[32] = "0";
const char *id_ptr = strstr(resp, "\"id\"");
// ... 解析原始 id ...
sprintf(reply, "{\"id\":\"%s\",\"code\":200}", msg_id);
```

---

## 问题 6：MQTT 控制指令被丢弃

**现象**：设备在线，传感器数据正常上报，但收不到控制指令

**原因**：`MQTT_Publish` 函数在发送前**清空接收缓冲区**，导致收到的控制指令被丢弃

**解决**：
```c
// 发送前先处理缓冲区中已有的数据
if (usart3_rx_done) {
    CheckResponse((const char*)usart3_rx_buffer);
    usart3_rx_index = 0;
    usart3_rx_done = 0;
}
// 发送后也检查一次
HAL_Delay(100);
if (usart3_rx_done) {
    CheckResponse((const char*)usart3_rx_buffer);
    usart3_rx_index = 0;
    usart3_rx_done = 0;
}
```

---

## 问题 7：MQTT 订阅丢失

**现象**：设备连接正常，但一直收不到 `set` 消息

**原因**：MQTT 订阅可能在连接异常后丢失

**解决**：每 30 秒自动重新订阅
```c
#define RESUB_INTERVAL 30000

if (HAL_GetTick() - last_resub_time >= RESUB_INTERVAL) {
    last_resub_time = HAL_GetTick();
    char sub_cmd[200];
    sprintf(sub_cmd, "AT+MQTTSUB=0,\"$sys/%s/%s/thing/property/set\",0\r\n",
            PRODUCT_ID, DEVICE_NAME);
    SendCmd(sub_cmd);
}
```

---

## 问题 8：OneNET 下发 JSON 格式与上报不同

**现象**：`ParseSetCommand` 解析不到属性值

**原因**：OneNET 上报和下发的 JSON 格式不同

| 方向 | 格式 | 示例 |
|------|------|------|
| 设备上报 (post) | `"key":{"value": val}` | `"fan_speed":{"value":500}` |
| 云台下发 (set) | `"key": val` | `"fan_speed":500` |

**解决**：
```c
// 错误 - 搜索上报格式
strstr(resp, "\"fan_speed\":{\"value\":");
ptr += 22;

// 正确 - 搜索 set 格式
strstr(resp, "\"fan_speed\":");
ptr += 12;
```

---

## 问题 9：控制指令响应延迟 10+ 秒

**现象**：点击控制按钮后 10+ 秒才生效

**原因**：`MQTT_Publish` 中有两次 `HAL_Delay(500)`，阻塞主循环

**解决**：缩短延时 + 定期重新订阅
```c
HAL_Delay(100);  // 原来是 500
// ... 发送数据 ...
HAL_Delay(100);  // 原来是 500
```

---

## 问题 10：HTTP 请求超时设置过短

**现象**：Django 返回 `控制失败`，但设备实际收到了指令

**原因**：`onenet.py` 超时 5 秒，但 OneNET 返回 10411 需要 6-7 秒

**解决**：
```python
self.timeout = 5       # 查询超时
self.set_timeout = 30  # 设置超时（等待设备响应）
```

---

## 问题 11：风扇关闭后仍在转

**现象**：`fan_speed=0` 已确认下发，但风扇不停

**原因**：PWM 0% 占空比时 PB6 可能仍有微弱输出

**解决**：
```c
if (speed == 0) {
    HAL_TIM_PWM_Stop(&htim4, TIM_CHANNEL_1);        // 关闭 PWM
    HAL_GPIO_WritePin(IN1_GPIO_Port, IN1_Pin, GPIO_PIN_RESET);  // 强制拉低
} else {
    HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_1);
    __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_1, pulse);
}
```

---

## 问题 12：LED 亮灭逻辑反转

**现象**：网页显示「亮」时 LED 实际灭了

**原因**：LED 电路是**低电平点亮**（共阳极/NPN 驱动），代码逻辑写反了

**解决**：
```c
// 错误
light < 1000 → GPIO_PIN_SET (高电平) → LED 灭

// 正确
light < 1000 → GPIO_PIN_RESET (低电平) → LED 亮
```

---

## 问题 13：Toast 消息遮挡按钮点击

**现象**：点击风扇档位按钮无反应

**原因**：Toast 使用 `position: fixed` 定位在顶部，`z-index: 1000`，遮挡了下方按钮

**解决**：
```css
.toast {
    pointer-events: none;  /* 点击穿透 */
    user-select: none;     /* 防止选中文字 */
}
```

---

## 问题 14：按钮高亮被轮询覆盖

**现象**：点击 3/4 档后，按钮高亮自动跳回 2/4

**原因**：轮询每 3 秒从 OneNET 拉取数据，OneNET 数据有延迟，覆盖了手动设置的高亮

**解决**：添加冷却期机制
```javascript
let _fanCooldown = 0;

function setFan(speed) {
    highlightFanBtn(speed);
    _fanCooldown = Date.now();  // 记录操作时间
    // ...
}

// 轮询时检查冷却期
if (d.fan_speed !== undefined && Date.now() - _fanCooldown > 10000) {
    highlightFanBtn(d.fan_speed);
}
```

---

## 问题 15：快速点击按钮异步竞争

**现象**：快速切换档位后，UI 状态混乱

**原因**：多个异步请求交错完成，先完成的请求恢复了旧状态

**解决**：请求序号机制
```javascript
let _lastFanReq = 0;

function setFan(speed) {
    const reqId = ++_lastFanReq;
    fetch(...)
    .then(json => {
        // 只有最新请求才处理响应
        if (reqId === _lastFanReq && !json.success) pollSensorData();
    });
}
```

---

## 问题 16：api_control 冗余网络请求

**现象**：控制响应慢，快速操作时被 OneNET 限流

**原因**：控制前先调用 `get_latest_data()` 获取旧值用于日志，多了一次网络请求

**解决**：移除冗余调用，日志旧值记为 `?`
```python
# 直接下发，不查询旧值
result = client.set_device_property({prop: value})
DeviceControlLog.objects.create(
    old_value="?",  # 不再查询
    new_value=str(value),
)
```

---

## 经验总结

| 类别 | 经验 |
|------|------|
| OneNET API | `code=0` 表示成功，`params` 用对象格式 |
| MQTT Token | 设备级和产品级 Token 不通用 |
| ESP8266 | 发送数据时会阻塞接收，需前后都检查缓冲区 |
| PWM 控制 | 关闭时需 Stop PWM + 强制拉低 GPIO |
| LED 电路 | 确认是高电平还是低电平点亮 |
| 前端轮询 | 控制操作后加冷却期，防止延迟数据覆盖 |
| 异步请求 | 用请求序号防止竞争，只处理最新响应 |
