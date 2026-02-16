set(BSP_COMPONENT "")
if(DEFINED CONFIG_BSP_BOARD_JC1060WP470C_I_W_Y AND CONFIG_BSP_BOARD_JC1060WP470C_I_W_Y STREQUAL "y")
    set(BSP_COMPONENT jc1060wp470c_i_w_y_bsp)
elseif(DEFINED CONFIG_BSP_BOARD_P4_FUNCTION_EV AND CONFIG_BSP_BOARD_P4_FUNCTION_EV STREQUAL "y")
    set(BSP_COMPONENT espressif__esp32_p4_function_ev_board)
else()
    message(WARNING "BSP board not selected yet; defaulting to JC1060WP470C_I_W_Y for configuration. Choose the board in menuconfig.")
    set(BSP_COMPONENT jc1060wp470c_i_w_y_bsp)
endif()

idf_component_register(
    SRCS "backlight_manager.cpp"
    INCLUDE_DIRS "include"
    REQUIRES ${BSP_COMPONENT} esp_timer freertos
)
