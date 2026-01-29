#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "lvgl.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "mqtt_manager.h"
#include "config_manager.h"
#include "settings_ui.h"
#include "status_info_ui.h"

static const char *TAG = "app_main_cpp";

// MQTT Task - handles MQTT events and configuration updates
static void mqtt_task(void *pvParameters)
{
    ESP_LOGI(TAG, "MQTT Task started");
    
    // Wait for Ethernet to be ready
    vTaskDelay(pdMS_TO_TICKS(5000));
    
    // Load settings and connect to MQTT
    SettingsUI& settings = SettingsUI::getInstance();
    settings.loadSettings();
    
    // Initialize MQTT with loaded settings
    MQTTManager& mqtt = MQTTManager::getInstance();
    if (!settings.getUsername().empty()) {
        ESP_LOGI(TAG, "Connecting to MQTT with authentication");
        mqtt.init(settings.getBrokerUri(), settings.getUsername(), 
                  settings.getPassword(), settings.getClientId());
    } else {
        ESP_LOGI(TAG, "Connecting to MQTT: %s", settings.getBrokerUri().c_str());
        mqtt.init(settings.getBrokerUri(), settings.getClientId());
    }
    
    // Wait for MQTT connection
    int retry_count = 0;
    while (!mqtt.isConnected() && retry_count < 30) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        retry_count++;
    }
    
    if (mqtt.isConnected()) {
        std::string config_topic = settings.getConfigTopic();
        ESP_LOGI(TAG, "MQTT connected, subscribing to config topic: %s", config_topic.c_str());
        
        // Update status info
        StatusInfoUI::getInstance().updateMqttStatus(true, settings.getBrokerUri());
        
        // Subscribe to configuration topic
        mqtt.subscribe(config_topic, 0, [](const std::string& topic, const std::string& payload) {
            ESP_LOGI(TAG, "Received config on %s, size: %d bytes", topic.c_str(), payload.size());
            ConfigManager::getInstance().queueConfig(payload);
        });
    } else {
        ESP_LOGW(TAG, "MQTT connection failed, loading cached config");
        StatusInfoUI::getInstance().updateMqttStatus(false, settings.getBrokerUri());
        ConfigManager::getInstance().loadCachedConfig();
    }
    
    // Keep task running
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// HMI Task - runs LVGL UI updates in separate task
static void hmi_task(void *pvParameters)
{
    ESP_LOGI(TAG, "HMI Task started");
    
    while (1) {
        // Lock display and perform UI operations
        bsp_display_lock(0);
        
        // Process any pending configuration from MQTT
        ConfigManager::getInstance().processPendingConfig();
        
        // Update system info periodically
        static int update_counter = 0;
        if (++update_counter >= 10) {  // Update every second
            update_counter = 0;
            StatusInfoUI::getInstance().updateSystemInfo(
                esp_get_free_heap_size(),
                esp_get_minimum_free_heap_size()
            );
        }
        
        // lv_timer_handler is called automatically by the LVGL port
        
        bsp_display_unlock();
        
        // Sleep for a bit
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// Initialize base UI with settings gear icon
static void init_base_ui(void)
{
    // Lock display while creating UI
    bsp_display_lock(0);
    
    // Setup screen background
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x1E1E1E), LV_PART_MAIN);
    
    // Initialize settings UI (creates gear icon)
    SettingsUI::getInstance().init(scr);
    
    // Initialize status info UI (creates info icon)
    StatusInfoUI::getInstance().init(scr);
    
    bsp_display_unlock();
    
    ESP_LOGI(TAG, "Base UI initialized");
}

extern "C" void app_main_cpp(void)
{
    ESP_LOGI(TAG, "Initializing ESP32-P4 MQTT Panel...");
    
    // Initialize base UI (gear icon + placeholder)
    init_base_ui();
    
    // Create HMI task for UI updates
    xTaskCreate(hmi_task, "hmi_task", 8192, NULL, 4, NULL);
    
    // Create MQTT task for configuration and message handling
    xTaskCreate(mqtt_task, "mqtt_task", 8192, NULL, 5, NULL);
    
    ESP_LOGI(TAG, "MQTT Panel initialization complete");
}
