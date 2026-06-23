import json
import logging
from django.shortcuts import render
from django.http import JsonResponse
from django.views.decorators.csrf import csrf_exempt
from django.views.decorators.http import require_http_methods
from django.conf import settings

from .onenet import get_client
from .models import SensorData, DeviceControlLog
from . import ml_predict

logger = logging.getLogger(__name__)


def index(request):
    """首页 — 展示实时数据和控制面板"""
    return render(request, "dashboard/index.html", {
        "poll_interval": settings.POLL_INTERVAL,
    })


def api_sensor_data(request):
    """
    AJAX 接口 — 从 OneNET 轮询最新传感器数据
    GET /api/sensor-data/
    """
    client = get_client()
    data = client.get_latest_data()

    if data is None:
        return JsonResponse({
            "success": False,
            "message": "无法获取 OneNET 数据，请检查网络或 Token 配置",
        })

    # 保存到数据库
    SensorData.objects.create(
        temp=data.get("temp", 0),
        light=data.get("light", 0),
        fan_speed=data.get("fan_speed", 0),
        led=data.get("led", 0),
    )

    # 只保留最近 500 条记录
    count = SensorData.objects.count()
    if count > 500:
        ids = SensorData.objects.order_by("-created_at").values_list("id", flat=True)[:500]
        SensorData.objects.exclude(id__in=list(ids)).delete()

    # ========== LED 自动控制：光照 < 1000 亮灯 ==========
    light = data.get("light")
    current_led = data.get("led")
    if light is not None:
        target_led = 1 if light < 1000 else 0
        if current_led != target_led:
            led_result = client.set_device_property({"led": target_led})
            if led_result is not None:
                data["led"] = target_led
                DeviceControlLog.objects.create(
                    property_name="led",
                    old_value=str(current_led),
                    new_value=str(target_led),
                    success=True,
                    message=f"光照自动控制: light={light}",
                )

    return JsonResponse({
        "success": True,
        "data": data,
    })


@csrf_exempt
@require_http_methods(["POST"])
def api_control(request):
    """
    控制接口 — 下发设备属性
    POST /api/control/
    Body: {"property": "fan_speed", "value": 500}
    """
    try:
        body = json.loads(request.body)
    except json.JSONDecodeError:
        return JsonResponse({"success": False, "message": "无效的 JSON 数据"}, status=400)

    prop = body.get("property")
    value = body.get("value")

    # 验证属性名
    valid_props = {"fan_speed", "led"}
    if prop not in valid_props:
        return JsonResponse({"success": False, "message": f"不支持的属性: {prop}"}, status=400)

    # 验证值范围
    try:
        value = int(value)
    except (TypeError, ValueError):
        return JsonResponse({"success": False, "message": "值必须为整数"}, status=400)

    if prop == "fan_speed":
        if not (0 <= value <= 1000):
            return JsonResponse({"success": False, "message": "风扇转速范围: 0-1000"}, status=400)
        value = (value // 10) * 10  # 对齐步长
    elif prop == "led":
        value = 1 if value else 0  # 只有 0 和 1

    # 下发控制指令
    client = get_client()
    result = client.set_device_property({prop: value})

    success = result is not None
    message = "控制成功" if success else "控制失败，请检查网络或设备状态"

    # 记录控制日志
    DeviceControlLog.objects.create(
        property_name=prop,
        old_value="?",
        new_value=str(value),
        success=success,
        message=message,
    )

    return JsonResponse({
        "success": success,
        "message": message,
    })


def api_history(request):
    """
    历史数据接口 — 返回最近 N 条记录
    GET /api/history/?limit=50
    """
    limit = int(request.GET.get("limit", 50))
    limit = min(limit, 200)

    records = SensorData.objects.order_by("-created_at")[:limit]
    data = [
        {
            "temp": r.temp,
            "light": r.light,
            "fan_speed": r.fan_speed,
            "led": r.led,
            "time": r.created_at.strftime("%H:%M:%S"),
        }
        for r in reversed(records)
    ]

    return JsonResponse({"success": True, "data": data})


def api_predict(request):
    """
    温度预测接口
    GET /api/predict/
    返回预测温度和建议风扇转速
    """
    result = ml_predict.predict_next_temp()
    return JsonResponse({"success": True, "data": result})


@csrf_exempt
@require_http_methods(["POST"])
def api_train(request):
    """
    重新训练模型
    POST /api/train/
    """
    success, message = ml_predict.train_model()
    return JsonResponse({"success": success, "message": message})


# 自动控制状态（内存级）
_auto_control_enabled = False


@csrf_exempt
@require_http_methods(["POST"])
def api_auto_control(request):
    """
    自动控制开关
    POST /api/auto-control/
    Body: {"enabled": true/false}
    """
    global _auto_control_enabled
    try:
        body = json.loads(request.body)
    except json.JSONDecodeError:
        return JsonResponse({"success": False, "message": "无效的 JSON"}, status=400)

    _auto_control_enabled = bool(body.get("enabled", False))
    status = "开启" if _auto_control_enabled else "关闭"
    return JsonResponse({"success": True, "enabled": _auto_control_enabled, "message": f"自动控制已{status}"})


def api_auto_control_status(request):
    """
    查询自动控制状态 + 最新预测
    GET /api/auto-control/status/
    """
    global _auto_control_enabled

    result = None
    if _auto_control_enabled:
        # 执行预测
        result = ml_predict.predict_next_temp()

        # 如果预测成功，自动下发风扇控制
        if result and result.get("model_ready") and result.get("fan_speed") is not None:
            client = get_client()
            client.set_device_property({"fan_speed": result["fan_speed"]})

    return JsonResponse({
        "success": True,
        "enabled": _auto_control_enabled,
        "prediction": result,
    })
