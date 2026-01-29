#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "ethernet_init.h"

static const char *TAG = "main";

// External C++ main function
extern void app_main_cpp(void);

void app_main(void)
{
    ESP_LOGI(TAG, "ESP32-P4 LVGL Panel Starting...");
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "NVS initialized");
    
    // Initialize Ethernet
    ESP_ERROR_CHECK(ethernet_init());
    
    // Initialize BSP display with custom configuration (disable SW rotation to save memory)
    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size = BSP_LCD_DRAW_BUFF_SIZE,
        .double_buffer = BSP_LCD_DRAW_BUFF_DOUBLE,
        .hw_cfg = {
            .hdmi_resolution = BSP_HDMI_RES_NONE,
            .dsi_bus = {
                .phy_clk_src = 0,  // Let driver choose default clock source
                .lane_bit_rate_mbps = BSP_LCD_MIPI_DSI_LANE_BITRATE_MBPS,
            }
        },
        .flags = {
            .buff_dma = true,
            .buff_spiram = false,
            .sw_rotate = false,  // Disable SW rotation to avoid large rotation buffer allocation
        }
    };
    bsp_display_start_with_config(&cfg);
    bsp_display_backlight_on();
    
    ESP_LOGI(TAG, "Display initialized");
    
    // Call C++ main
    app_main_cpp();
}
