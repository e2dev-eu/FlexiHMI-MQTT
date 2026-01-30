idf_component_register(
    SRCS "settings_ui.cpp"
    INCLUDE_DIRS "include"
    REQUIRES lvgl nvs_flash mqtt_manager wireless_manager lan_manager esp_app_format
)
