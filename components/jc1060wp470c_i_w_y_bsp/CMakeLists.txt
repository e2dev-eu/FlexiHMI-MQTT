if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER_EQUAL "5.3")
    set(REQ esp_driver_gpio esp_driver_i2c esp_driver_ledc)
else()
    set(REQ driver)
endif()

idf_component_register(
    SRCS "src/jc1060wp470c_i_w_y_bsp.c"
    INCLUDE_DIRS "include"
    REQUIRES ${REQ} esp_lcd espressif__esp_lcd_touch espressif__esp_lcd_touch_gt911 espressif__esp_lcd_jd9165
)
