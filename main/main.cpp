#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "lvgl.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"

static const char *TAG = "app_main_cpp";

// HMI Task - runs LVGL UI updates in separate task
static void hmi_task(void *pvParameters)
{
    ESP_LOGI(TAG, "HMI Task started");
    
    while (1) {
        // Lock display and perform UI operations
        bsp_display_lock(0);
        
        // Add your UI updates here
        // lv_timer_handler is called automatically by the LVGL port
        
        bsp_display_unlock();
        
        // Sleep for a bit
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// Create demo UI
static void create_demo_ui(void)
{
    // Lock display while creating UI
    bsp_display_lock(0);
    
    // Create a simple demo screen
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x003a57), LV_PART_MAIN);
    
    // Create a label
    lv_obj_t *label = lv_label_create(scr);
    lv_label_set_text(label, "ESP32-P4 LVGL9 Panel\\nTouch the screen!");
    lv_obj_set_style_text_color(label, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, -50);
    
    // Create a button
    lv_obj_t *btn = lv_button_create(scr);
    lv_obj_set_size(btn, 200, 80);
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, 50);
    
    lv_obj_t *btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, "Click Me!");
    lv_obj_center(btn_label);
    
    bsp_display_unlock();
}

extern "C" void app_main_cpp(void)
{
    ESP_LOGI(TAG, "Initializing ESP32-P4 Function EV Board UI...");
    
    // Create demo UI
    create_demo_ui();
    
    // Create HMI task for UI updates
    xTaskCreate(hmi_task, "hmi_task", 8192, NULL, 4, NULL);
    
    ESP_LOGI(TAG, "HMI initialization complete");
}
