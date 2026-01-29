#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "ethernet_init.h"

static const char *TAG = "main";

// External C++ main function
extern void app_main_cpp(void);

void app_main(void)
{
    ESP_LOGI(TAG, "ESP32-P4 LVGL Panel Starting...");
    
    // Initialize Ethernet
    ESP_ERROR_CHECK(ethernet_init());
    
    // Initialize BSP display with default configuration
    bsp_display_start();
    bsp_display_backlight_on();
    
    ESP_LOGI(TAG, "Display initialized");
    
    // Call C++ main
    app_main_cpp();
}
