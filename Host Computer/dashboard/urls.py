from django.urls import path
from . import views

app_name = "dashboard"

urlpatterns = [
    path("", views.index, name="index"),
    path("api/sensor-data/", views.api_sensor_data, name="api_sensor_data"),
    path("api/control/", views.api_control, name="api_control"),
    path("api/history/", views.api_history, name="api_history"),
    path("api/predict/", views.api_predict, name="api_predict"),
    path("api/train/", views.api_train, name="api_train"),
    path("api/auto-control/", views.api_auto_control, name="api_auto_control"),
    path("api/auto-control/status/", views.api_auto_control_status, name="api_auto_control_status"),
]
