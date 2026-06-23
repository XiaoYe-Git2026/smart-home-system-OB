"""
OneNET 平台 HTTP API 客户端
用于查询和设置设备物模型属性
"""

import requests
import logging
from django.conf import settings

logger = logging.getLogger(__name__)


class OneNETClient:
    """OneNET 物模型 HTTP API 客户端"""

    BASE_URL = "https://iot-api.heclouds.com"

    def __init__(self):
        self.product_id = settings.ONENET_PRODUCT_ID
        self.device_name = settings.ONENET_DEVICE_NAME
        self.token = settings.ONENET_TOKEN
        self.session = requests.Session()
        self.session.headers.update({
            "Authorization": self.token,
            "Content-Type": "application/json",
        })
        self.timeout = 5       # 查询超时(秒)
        self.set_timeout = 30  # 设置超时(秒)，OneNET 等待设备响应需更长时间

    def query_device_property(self, identifiers=None):
        """
        查询设备属性值

        Args:
            identifiers: 属性标识符列表，如 ['temp', 'light']，None 表示查询全部

        Returns:
            dict: API 响应数据，失败返回 None
        """
        url = f"{self.BASE_URL}/thingmodel/query-device-property"
        params = {
            "product_id": self.product_id,
            "device_name": self.device_name,
        }
        if identifiers:
            params["identifier"] = ",".join(identifiers)

        try:
            resp = self.session.get(url, params=params, timeout=self.timeout)
            resp.raise_for_status()
            data = resp.json()
            if data.get("code") == 0:
                return data.get("data", [])
            else:
                logger.warning("OneNET 查询失败: %s", data.get("msg"))
                return None
        except requests.RequestException as e:
            logger.error("OneNET 请求异常: %s", e)
            return None

    def set_device_property(self, params_dict):
        """
        设置设备属性值

        Args:
            params_dict: 属性字典，如 {'fan_speed': 500, 'curtain_step': 60}

        Returns:
            dict: API 响应数据，失败返回 None
        """
        url = f"{self.BASE_URL}/thingmodel/set-device-property"
        body = {
            "product_id": self.product_id,
            "device_name": self.device_name,
            "params": params_dict,
        }

        try:
            resp = self.session.post(url, json=body, timeout=self.set_timeout)
            resp.raise_for_status()
            data = resp.json()
            code = data.get("code")
            if code == 0:
                return data
            elif code == 10411:
                # 10411 = 设备响应超时，但指令通常已送达
                logger.info("OneNET 设置超时(10411)，指令可能已送达: %s", data.get("msg"))
                return data
            else:
                logger.warning("OneNET 设置失败: %s", data.get("msg"))
                return None
        except requests.RequestException as e:
            logger.error("OneNET 请求异常: %s", e)
            return None

    def get_latest_data(self):
        """
        获取最新传感器数据，返回格式化字典

        Returns:
            dict: {
                'temp': float, 'light': int,
                'fan_speed': int, 'led': int,
                'temp_time': str, 'light_time': str, ...
            }
            失败返回 None
        """
        properties = self.query_device_property()
        if properties is None:
            return None

        result = {}
        for prop in properties:
            identifier = prop.get("identifier", "")
            value = prop.get("value", "")
            time_ms = prop.get("time", 0)

            # 根据属性类型转换值
            if identifier == "temp":
                result["temp"] = float(value) if value else 0.0
            elif identifier == "light":
                result["light"] = int(float(value)) if value else 0
            elif identifier == "fan_speed":
                result["fan_speed"] = int(float(value)) if value else 0
            elif identifier == "led":
                result["led"] = int(float(value)) if value else 0

            # 记录时间戳
            if time_ms:
                from datetime import datetime
                result[f"{identifier}_time"] = datetime.fromtimestamp(
                    time_ms / 1000
                ).strftime("%Y-%m-%d %H:%M:%S")

        return result


# 全局单例
_client = None


def get_client():
    """获取 OneNET 客户端单例"""
    global _client
    if _client is None:
        _client = OneNETClient()
    return _client
