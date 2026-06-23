from django.contrib import admin
from .models import SensorData, DeviceControlLog


@admin.register(SensorData)
class SensorDataAdmin(admin.ModelAdmin):
    list_display = ("created_at", "temp", "light", "fan_speed", "led")
    list_filter = ("created_at",)
    readonly_fields = ("created_at",)


@admin.register(DeviceControlLog)
class DeviceControlLogAdmin(admin.ModelAdmin):
    list_display = ("created_at", "property_name", "old_value", "new_value", "success", "message")
    list_filter = ("property_name", "success", "created_at")
    readonly_fields = ("created_at",)
