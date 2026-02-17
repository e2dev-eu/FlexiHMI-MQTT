idf_component_register(
    SRCS "backlight_manager.cpp"
    INCLUDE_DIRS "include"
    REQUIRES jc1060wp470c_i_w_y_bsp espressif__esp32_p4_function_ev_board esp_timer freertos
)
