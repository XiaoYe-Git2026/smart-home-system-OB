"""
温度预测模块
使用 RandomForestRegressor 基于历史温度数据进行预测
"""

import logging
import numpy as np
from sklearn.ensemble import RandomForestRegressor
from django.db.models import F

logger = logging.getLogger(__name__)

# 滑动窗口大小（用前 N 次温度作为特征）
WINDOW_SIZE = 5

# 全局模型缓存
_model = None
_model_ready = False


def _build_features(temps):
    """
    从温度序列构建特征矩阵
    每条样本: [t-N, t-N+1, ..., t-1, hour, minute]
    """
    X, y = [], []
    for i in range(WINDOW_SIZE, len(temps)):
        # 滑动窗口温度
        window = temps[i - WINDOW_SIZE:i]
        # 当前记录的小时和分钟（从索引推算，或直接用 0）
        # 这里简化：用索引位置模拟时间周期
        idx = i % 60
        features = window + [idx // 6, idx % 6 * 10]
        X.append(features)
        y.append(temps[i])
    return np.array(X), np.array(y)


def train_model():
    """
    从数据库读取历史温度数据，训练模型
    Returns: (success: bool, message: str)
    """
    global _model, _model_ready

    from .models import SensorData

    # 至少需要 WINDOW_SIZE + 10 条数据才训练
    records = list(
        SensorData.objects.order_by("created_at").values_list("temp", flat=True)
    )

    if len(records) < WINDOW_SIZE + 10:
        _model_ready = False
        return False, f"数据不足：当前 {len(records)} 条，至少需要 {WINDOW_SIZE + 10} 条"

    X, y = _build_features(records)

    if len(X) < 5:
        _model_ready = False
        return False, "特征样本不足"

    # 训练随机森林
    _model = RandomForestRegressor(
        n_estimators=100,
        max_depth=10,
        random_state=42,
    )
    _model.fit(X, y)
    _model_ready = True

    score = _model.score(X, y)
    logger.info("模型训练完成，R²=%.4f，样本数=%d", score, len(X))
    return True, f"训练完成，R²={score:.4f}，样本数={len(X)}"


def predict_next_temp(current_temps=None):
    """
    预测下一次温度

    Args:
        current_temps: 最近 WINDOW_SIZE 个温度值列表
                       如果为 None，从数据库读取

    Returns:
        dict: {
            'predicted_temp': float,
            'current_temp': float,
            'fan_speed': int,       # 建议风扇转速
            'model_ready': bool,
            'message': str,
        }
    """
    global _model, _model_ready

    from .models import SensorData

    # 如果模型未训练，先训练
    if not _model_ready:
        success, msg = train_model()
        if not success:
            return {
                "predicted_temp": None,
                "current_temp": None,
                "fan_speed": 0,
                "model_ready": False,
                "message": msg,
            }

    # 获取最近的温度序列
    if current_temps is None:
        recent = list(
            SensorData.objects.order_by("-created_at")
            .values_list("temp", flat=True)[:WINDOW_SIZE + 5]
        )
        current_temps = list(reversed(recent))

    if len(current_temps) < WINDOW_SIZE:
        return {
            "predicted_temp": None,
            "current_temp": current_temps[-1] if current_temps else 0,
            "fan_speed": 0,
            "model_ready": _model_ready,
            "message": f"温度序列不足，需要 {WINDOW_SIZE} 个",
        }

    # 构建预测特征
    window = current_temps[-WINDOW_SIZE:]
    from datetime import datetime
    now = datetime.now()
    features = np.array([window + [now.hour, now.minute]])

    # 预测
    predicted = float(_model.predict(features)[0])
    predicted = round(predicted, 1)

    current_temp = current_temps[-1]

    # 根据预测温度计算建议风扇转速
    fan_speed = _calc_fan_speed(predicted)

    return {
        "predicted_temp": predicted,
        "current_temp": current_temp,
        "fan_speed": fan_speed,
        "model_ready": True,
        "message": "预测成功",
    }


def _calc_fan_speed(temp):
    """
    根据预测温度计算风扇转速
    - >= 32°C  → 1000 (全速)
    - 28~32°C  → 线性映射 400~1000
    - 25~28°C  → 线性映射 0~400
    - < 25°C   → 0 (关闭)
    """
    if temp >= 32:
        return 1000
    elif temp >= 28:
        speed = int(400 + (temp - 28) / 4 * 600)
        return (speed // 10) * 10  # 对齐步长
    elif temp >= 25:
        speed = int((temp - 25) / 3 * 400)
        return (speed // 10) * 10
    else:
        return 0


def get_model_info():
    """获取模型状态信息"""
    from .models import SensorData
    total = SensorData.objects.count()
    return {
        "model_ready": _model_ready,
        "total_samples": total,
        "window_size": WINDOW_SIZE,
    }
