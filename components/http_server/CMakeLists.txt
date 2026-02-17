idf_component_register(
    SRCS "http_server.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_server espressif__esp_lvgl_adapter lvgl__lvgl
)
