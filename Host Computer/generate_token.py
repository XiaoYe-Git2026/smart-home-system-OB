"""
OneNET HTTP API Token 生成工具

Token 签名算法:
  string_for_sign = et + "\n" + method + "\n" + res + "\n" + version
  sign = base64( md5(string_for_sign, api_key) )
  token = version=2018-10-31&res=<res>&et=<et>&method=md5&sign=<sign>

使用方式:
  python generate_token.py
  或导入调用: from generate_token import generate_onenet_token
"""

import hashlib
import base64
import time
from urllib.parse import quote


def generate_onenet_token(api_key, product_id, device_name, expire_hours=24):
    """
    生成 OneNET HTTP API Token

    Args:
        api_key:      OneNET 平台 APIkey (在 权限管理 -> APIkey 中获取)
        product_id:   产品 ID
        device_name:  设备名称
        expire_hours: 过期时间 (小时), 默认 24 小时

    Returns:
        str: 完整的 Token 字符串
    """
    version = "2018-10-31"
    method = "md5"

    # 过期时间戳 (秒)
    et = str(int(time.time()) + expire_hours * 3600)

    # 资源标识 (HTTP API 使用 products/{id}/devices/{name})
    res = f"products/{product_id}/devices/{device_name}"

    # 待签名字符串
    string_for_sign = f"{et}\n{method}\n{res}\n{version}"

    # 计算签名: base64( hmac_md5(string_for_sign, api_key) )
    # OneNET 使用 HMAC-MD5，key 为 api_key
    sign = base64.b64encode(
        hmac_md5(string_for_sign.encode("utf-8"), api_key.encode("utf-8"))
    ).decode("utf-8")

    # URL 编码
    res_encoded = quote(res, safe="")
    sign_encoded = quote(sign, safe="")

    token = (
        f"version={version}"
        f"&res={res_encoded}"
        f"&et={et}"
        f"&method={method}"
        f"&sign={sign_encoded}"
    )

    return token


def hmac_md5(key, message):
    """HMAC-MD5 签名"""
    block_size = 64

    # 如果 key 长度大于 64 字节，先做 MD5
    if len(key) > block_size:
        key = hashlib.md5(key).digest()

    # 补齐到 64 字节
    key = key + b'\x00' * (block_size - len(key))

    # HMAC 计算
    o_key_pad = bytes([b ^ 0x5c for b in key])
    i_key_pad = bytes([b ^ 0x36 for b in key])

    return hashlib.md5(o_key_pad + hashlib.md5(i_key_pad + message).digest()).digest()


if __name__ == "__main__":
    print("=" * 60)
    print("  OneNET HTTP API Token 生成工具")
    print("=" * 60)
    print()

    # 从下位机 onenet.h 读取的默认值
    default_product_id = "SSX0I7L3I2"
    default_device_name = "ESP8266"

    api_key = input(f"请输入 OneNET APIkey: ").strip()
    if not api_key:
        print("❌ APIkey 不能为空！")
        print("   请在 OneNET 平台 -> 权限管理 -> APIkey 中获取")
        exit(1)

    product_id = input(f"产品ID [{default_product_id}]: ").strip() or default_product_id
    device_name = input(f"设备名称 [{default_device_name}]: ").strip() or default_device_name
    hours = input("有效期(小时) [24]: ").strip()
    expire_hours = int(hours) if hours else 24

    token = generate_onenet_token(api_key, product_id, device_name, expire_hours)

    print()
    print("─" * 60)
    print("✅ 生成的 Token:")
    print()
    print(token)
    print()
    print("─" * 60)
    print(f"📌 请将此 Token 粘贴到 smart_project/settings.py 的 ONENET_TOKEN 中")
    print(f"⏰ 有效期: {expire_hours} 小时")
    print("─" * 60)
