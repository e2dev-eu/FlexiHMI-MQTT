#include <stdio.h>
#include <dirent.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "lvgl.h"
#include "esp_lv_decoder.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "mqtt_manager.h"
#include "config_manager.h"
#include "settings_ui.h"
#include "wireless_manager.h"
#include "lan_manager.h"
#include "backlight_manager.h"
#include "esp_hosted.h" // ESP-Hosted for ESP32-C6 wireless co-processor

static const char *TAG = "app_main_cpp";

// Global touch event filter for backlight activity reset
static void touch_event_cb(lv_event_t *e)
{
    BacklightManager::getInstance().resetTimer();
}

// MQTT Task - handles MQTT events and configuration updates
static void mqtt_task(void *pvParameters)
{
    ESP_LOGI(TAG, "MQTT Task started");

    // Wait for Ethernet to be ready
    vTaskDelay(pdMS_TO_TICKS(5000));

    // Load settings and connect to MQTT
    SettingsUI &settings = SettingsUI::getInstance();
    settings.loadSettings();

    // Initialize MQTT with loaded settings
    MQTTManager &mqtt = MQTTManager::getInstance();

    // Register status callback to update SettingsUI
    mqtt.setStatusCallback([](bool connected, uint32_t messages_received, uint32_t messages_sent)
                           { SettingsUI::getInstance().onMqttStatusChanged(connected, messages_received, messages_sent); });

    if (!settings.getUsername().empty())
    {
        ESP_LOGI(TAG, "Connecting to MQTT with authentication");
        mqtt.init(settings.getBrokerUri(), settings.getUsername(),
                  settings.getPassword(), settings.getClientId());
    }
    else
    {
        ESP_LOGI(TAG, "Connecting to MQTT: %s", settings.getBrokerUri().c_str());
        mqtt.init(settings.getBrokerUri(), settings.getClientId());
    }

    // Wait for MQTT connection
    int retry_count = 0;
    while (!mqtt.isConnected() && retry_count < 30)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
        retry_count++;
    }

    if (mqtt.isConnected())
    {
        std::string config_topic = settings.getConfigTopic();
        ESP_LOGI(TAG, "MQTT connected, subscribing to config topic: %s", config_topic.c_str());

        // Subscribe to configuration topic
        mqtt.subscribe(config_topic, 0, [](const std::string &topic, const std::string &payload)
                       {
            ESP_LOGI(TAG, "Received config on %s, size: %d bytes", topic.c_str(), payload.size());
            ConfigManager::getInstance().queueConfig(payload); });
    }
    else
    {
        ESP_LOGE(TAG, "MQTT connection failed - no configuration available");
    }

    // Keep task running
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// HMI Task - runs LVGL UI updates in separate task
static void hmi_task(void *pvParameters)
{
    ESP_LOGI(TAG, "HMI Task started");

    while (1)
    {
        // Lock display and perform UI operations
        bsp_display_lock(0);

        // Process any pending configuration from MQTT
        ConfigManager::getInstance().processPendingConfig();

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

    // Apply LVGL dark theme
    lv_theme_t *theme = lv_theme_default_init(NULL, lv_palette_main(LV_PALETTE_BLUE),
                                              lv_palette_main(LV_PALETTE_RED),
                                              true, LV_FONT_DEFAULT);
    lv_disp_set_theme(lv_disp_get_default(), theme);

    // Setup screen background
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x1E1E1E), LV_PART_MAIN);

    // Add global event to catch all touch events for backlight activity
    lv_indev_t *indev_touch = bsp_display_get_input_dev();
    if (indev_touch == NULL)
    {
        ESP_LOGW(TAG, "No active input device found for touch event handling");
    }
    else
    {
        ESP_LOGI(TAG, "Active input device found for touch event handling");
        lv_indev_add_event_cb(indev_touch, touch_event_cb, LV_EVENT_ALL, NULL);
    }

    // Initialize settings UI (creates gear icon in bottom-right)
    SettingsUI::getInstance().init(scr);

    bsp_display_unlock();

    // Initialize backlight manager (10 seconds timeout, dim to 5%, 1 second fade)
    BacklightManager::getInstance().init(30, 5, 1000);

    ESP_LOGI(TAG, "Base UI initialized");
}

// Initialize network managers (Ethernet and Wi-Fi)
static void init_network_managers(void)
{
    ESP_LOGI(TAG, "Initializing network managers...");

    // Initialize ESP-Hosted (ESP32-C6 wireless co-processor over SDIO)
    ESP_LOGI(TAG, "Initializing ESP-Hosted for ESP32-C6 co-processor...");
    esp_err_t ret = esp_hosted_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "ESP-Hosted initialization failed: %s", esp_err_to_name(ret));
        ESP_LOGE(TAG, "Check that ESP32-C6 is flashed with esp-hosted slave firmware");
    }
    else
    {
        ESP_LOGI(TAG, "ESP-Hosted initialized successfully");

        // Get co-processor app descriptor
        esp_hosted_app_desc_t desc = {};
        if (esp_hosted_get_coprocessor_app_desc(&desc) == ESP_OK)
        {
            ESP_LOGI(TAG, "ESP32-C6 Firmware: %s, Version: %s", desc.project_name, desc.version);
        }
    }

    // Initialize LAN Manager
    LanManager &lan = LanManager::getInstance();

    if (lan.init() != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize LAN Manager");
    }
    else
    {
        ESP_LOGI(TAG, "LAN Manager initialized (MAC: %s)", lan.getMacAddress().c_str());
    }

    // Initialize Wireless Manager
    WirelessManager &wifi = WirelessManager::getInstance();
    if (wifi.init() != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize Wireless Manager");
    }
    else
    {
        ESP_LOGI(TAG, "Wireless Manager initialized");
    }
}

extern "C" void app_main_cpp(void)
{
    ESP_LOGI(TAG, "Initializing...");

    static esp_lv_decoder_handle_t s_decoder_handle = NULL;
    esp_err_t decoder_ret = esp_lv_decoder_init(&s_decoder_handle);
    if(decoder_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init esp_lv_decoder: %s", esp_err_to_name(decoder_ret));
    } else {
        ESP_LOGI(TAG, "esp_lv_decoder initialized");
    }

    // Initialize base UI first (gear icon + placeholder) - shows immediately
    init_base_ui();

    // Initialize SD card for image storage
    ESP_LOGI(TAG, "Mounting SD card...");
    esp_err_t ret = bsp_sdcard_mount();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to mount SD card: %s", esp_err_to_name(ret));
        ESP_LOGE(TAG, "Image widget will not work without SD card");
    }
    else
    {
        ESP_LOGI(TAG, "SD card mounted successfully at %s", BSP_SD_MOUNT_POINT);

        // Print SD card information
        sdmmc_card_t *sdcard = bsp_sdcard_get_handle();
        if (sdcard != NULL)
        {
            ESP_LOGI(TAG, "SD Card: %s, Size: %llu MB",
                     sdcard->cid.name,
                     ((uint64_t)sdcard->csd.capacity) * sdcard->csd.sector_size / (1024 * 1024));
        }
    }

    // Initialize network managers (Ethernet and Wi-Fi) - can take time
    init_network_managers();

    // Create HMI task for UI updates
    xTaskCreate(hmi_task, "hmi_task", 8192, NULL, 4, NULL);

    // Create MQTT task for configuration and message handling
    xTaskCreate(mqtt_task, "mqtt_task", 8192, NULL, 5, NULL);

    ESP_LOGI(TAG, "MQTT Panel initialization complete");
}
