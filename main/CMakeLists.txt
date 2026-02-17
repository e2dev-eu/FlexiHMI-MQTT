idf_component_register(
    SRCS 
        "main.c"
        "main.cpp"
    INCLUDE_DIRS 
        "."
    REQUIRES
        ethernet
        esp_timer
        freertos
        jc1060wp470c_i_w_y_bsp
        espressif__esp32_p4_function_ev_board
        espressif__esp_lvgl_adapter
        hmi_widgets
        mqtt_manager
        config_manager
        settings_ui
        status_info_ui
        wireless_manager
        lan_manager
        backlight_manager
        mqtt
        http_server
)

if(CONFIG_BSP_BOARD_JC1060WP470C_I_W_Y)
    target_include_directories(${COMPONENT_LIB} BEFORE PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/../components/jc1060wp470c_i_w_y_bsp/include
    )
else()
    target_include_directories(${COMPONENT_LIB} BEFORE PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/../managed_components/espressif__esp32_p4_function_ev_board/include
    )
endif()
