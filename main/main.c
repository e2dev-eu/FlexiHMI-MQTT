#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "bsp/touch.h"
#include "ethernet_init.h"
#include "esp_lv_adapter.h"

static const char *TAG = "main";

static lv_indev_t *s_touch_indev = NULL;

lv_indev_t *app_get_touch_indev(void)
{
    return s_touch_indev;
}

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
    
    // Initialize display panel using BSP, then register it with esp_lvgl_adapter
    bsp_display_config_t cfg = {0};
#ifdef BSP_BOARD_P4_FUNCTION_EV
    cfg.hdmi_resolution = BSP_HDMI_RES_NONE;
    cfg.dsi_bus.phy_clk_src = 0;  // Let driver choose default clock source
    cfg.dsi_bus.lane_bit_rate_mbps = BSP_LCD_MIPI_DSI_LANE_BITRATE_MBPS;
#endif

    esp_lv_adapter_tear_avoid_mode_t tear_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_DEFAULT_MIPI_DSI;
    esp_lv_adapter_rotation_t rotation = ESP_LV_ADAPTER_ROTATE_0;
    uint8_t required_fbs = esp_lv_adapter_get_required_frame_buffer_count(tear_mode, rotation);
    if (CONFIG_BSP_LCD_DPI_BUFFER_NUMS != required_fbs) {
        ESP_LOGW(TAG, "CONFIG_BSP_LCD_DPI_BUFFER_NUMS=%d, adapter requires %d for tearing mode", CONFIG_BSP_LCD_DPI_BUFFER_NUMS, required_fbs);
    }

    bsp_lcd_handles_t handles = {0};
    ESP_ERROR_CHECK(bsp_display_new_with_handles(&cfg, &handles));
    esp_err_t disp_ret = esp_lcd_panel_disp_on_off(handles.panel, true);
    if (disp_ret != ESP_OK && disp_ret != ESP_ERR_NOT_SUPPORTED) {
        ESP_ERROR_CHECK(disp_ret);
    }
    bsp_display_backlight_on();

    esp_lv_adapter_config_t lv_cfg = ESP_LV_ADAPTER_DEFAULT_CONFIG();
    ESP_ERROR_CHECK(esp_lv_adapter_init(&lv_cfg));

    esp_lv_adapter_display_config_t disp_cfg = ESP_LV_ADAPTER_DISPLAY_MIPI_DEFAULT_CONFIG(
        handles.panel,
        handles.io,
        BSP_LCD_H_RES,
        BSP_LCD_V_RES,
        rotation
    );
    disp_cfg.tear_avoid_mode = tear_mode;
    disp_cfg.profile.enable_ppa_accel = true;
    lv_display_t *disp = esp_lv_adapter_register_display(&disp_cfg);
    if (disp == NULL) {
        ESP_LOGE(TAG, "Failed to register LVGL display");
    }

    esp_lcd_touch_handle_t touch_handle = NULL;
    bsp_touch_config_t touch_cfg = {0};
    if (bsp_touch_new(&touch_cfg, &touch_handle) == ESP_OK) {
        esp_lv_adapter_touch_config_t lv_touch_cfg = ESP_LV_ADAPTER_TOUCH_DEFAULT_CONFIG(disp, touch_handle);
        s_touch_indev = esp_lv_adapter_register_touch(&lv_touch_cfg);
        if (s_touch_indev == NULL) {
            ESP_LOGW(TAG, "Failed to register LVGL touch device");
        }
    } else {
        ESP_LOGW(TAG, "Failed to initialize touch controller");
    }

    ESP_ERROR_CHECK(esp_lv_adapter_start());
    
    ESP_LOGI(TAG, "Display initialized (esp_lvgl_adapter)");
    
    // Call C++ main
    app_main_cpp();
}
