from django.db import models


class SensorData(models.Model):
    """传感器数据记录"""
    temp = models.FloatField("温度(°C)", default=0)
    light = models.IntegerField("光照(Lux)", default=0)
    fan_speed = models.IntegerField("风扇转速(r/min)", default=0)
    led = models.IntegerField("LED(0=灭/1=亮)", default=0)
    created_at = models.DateTimeField("记录时间", auto_now_add=True)

    class Meta:
        verbose_name = "传感器数据"
        verbose_name_plural = "传感器数据"
        ordering = ["-created_at"]

    def __str__(self):
        return f"[{self.created_at}] T={self.temp}°C L={self.light}Lux F={self.fan_speed} LED={self.led}"


class DeviceControlLog(models.Model):
    """设备控制日志"""
    property_name = models.CharField("属性名称", max_length=50)
    old_value = models.CharField("原值", max_length=20)
    new_value = models.CharField("新值", max_length=20)
    success = models.BooleanField("是否成功", default=True)
    message = models.CharField("返回信息", max_length=200, blank=True)
    created_at = models.DateTimeField("控制时间", auto_now_add=True)

    class Meta:
        verbose_name = "控制日志"
        verbose_name_plural = "控制日志"
        ordering = ["-created_at"]

    def __str__(self):
        status = "✓" if self.success else "✗"
        return f"{status} [{self.created_at}] {self.property_name}: {self.old_value} -> {self.new_value}"
