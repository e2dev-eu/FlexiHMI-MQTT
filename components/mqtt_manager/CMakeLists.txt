idf_component_register(
    SRCS "mqtt_manager.cpp"
    INCLUDE_DIRS "include"
    REQUIRES mqtt esp_event nvs_flash
)
