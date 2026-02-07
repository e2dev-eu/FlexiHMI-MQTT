#pragma once

#include <string>
#include <vector>
#include <memory>
#include "cJSON.h"
#include "hmi_widget.h"
#include "mqtt_manager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

class ConfigManager {
public:
    static ConfigManager& getInstance();
    
    // Queue config for application (thread-safe, called from MQTT task)
    void queueConfig(const std::string& json_config);
    
    // Apply pending config if available (must be called from HMI/LVGL task)
    void processPendingConfig();
    
    // Parse JSON configuration and create/update widgets (LVGL thread only!)
    bool parseAndApply(const std::string& json_config);
    
    // Get current configuration version
    int getCurrentVersion() const { return m_current_version; }
    

    // Destroy all active widgets
    void destroyAllWidgets();
    
private:
    ConfigManager();
    ~ConfigManager();
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;
    
    bool parseWidgets(cJSON* widgets_array, lv_obj_t* parent = nullptr);
    bool createWidget(cJSON* widget_json, lv_obj_t* parent = nullptr);
    HMIWidget* createWidgetByType(const std::string& type, const std::string& id, int x, int y, int w, int h, cJSON* properties, lv_obj_t* parent);
    
    int m_current_version;
    std::vector<HMIWidget*> m_active_widgets;
    std::vector<MQTTManager::SubscriptionHandle> m_config_subscriptions;
    
    // Pending config for deferred application
    std::string m_pending_config;
    bool m_has_pending_config;
    SemaphoreHandle_t m_config_mutex;
};
