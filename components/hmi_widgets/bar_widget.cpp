#include "bar_widget.h"
#include "mqtt_manager.h"
#include <esp_log.h>
#include <cstdlib>

static const char *TAG = "BarWidget";

BarWidget::BarWidget(const std::string& id, int x, int y, int w, int h, cJSON* properties, lv_obj_t* parent) {
    m_id = id;
    m_min = 0;
    m_max = 100;
    m_value = 0;
    
    // Extract properties
    if (properties) {
        cJSON* min_item = cJSON_GetObjectItem(properties, "min");
        if (min_item && cJSON_IsNumber(min_item)) {
            m_min = min_item->valueint;
        }
        
        cJSON* max_item = cJSON_GetObjectItem(properties, "max");
        if (max_item && cJSON_IsNumber(max_item)) {
            m_max = max_item->valueint;
        }
        
        cJSON* value_item = cJSON_GetObjectItem(properties, "value");
        if (value_item && cJSON_IsNumber(value_item)) {
            m_value = value_item->valueint;
        }
        
        cJSON* mqtt_topic = cJSON_GetObjectItem(properties, "mqtt_topic");
        if (mqtt_topic && cJSON_IsString(mqtt_topic)) {
            m_mqtt_topic = mqtt_topic->valuestring;
        }
        
        cJSON* color_item = cJSON_GetObjectItem(properties, "color");
        if (color_item && cJSON_IsString(color_item)) {
            const char* color_str = color_item->valuestring;
            if (color_str[0] == '#') {
                uint32_t color = strtol(color_str + 1, NULL, 16);
                m_color = lv_color_hex(color);
                m_has_color = true;
            }
        }
    }
    
    // Create bar object
    lv_obj_t* parent_obj = parent ? parent : lv_screen_active();
    m_lvgl_obj = lv_bar_create(parent_obj);
    if (!m_lvgl_obj) {
        ESP_LOGE(TAG, "Failed to create bar widget: %s", id.c_str());
        return;
    }
    
    lv_obj_set_pos(m_lvgl_obj, x, y);
    lv_obj_set_size(m_lvgl_obj, w, h);
    
    lv_bar_set_range(m_lvgl_obj, m_min, m_max);
    lv_bar_set_value(m_lvgl_obj, m_value, LV_ANIM_OFF);
    
    // Apply custom color if specified
    if (m_has_color) {
        lv_obj_set_style_bg_color(m_lvgl_obj, m_color, LV_PART_INDICATOR);
    }
    
    // Subscribe to mqtt_topic to receive updates
    if (!m_mqtt_topic.empty()) {
        m_subscription_handle = MQTTManager::getInstance().subscribe(m_mqtt_topic, 0,
            [this](const std::string& topic, const std::string& payload) {
                this->onMqttMessage(topic, payload);
            });
        ESP_LOGI(TAG, "Bar %s subscribed to %s for updates", id.c_str(), m_mqtt_topic.c_str());
    }
    
    ESP_LOGI(TAG, "Created bar widget: %s at (%d,%d) size (%dx%d)", 
             id.c_str(), x, y, w, h);
}

BarWidget::~BarWidget() {
    if (m_subscription_handle != 0) {
        MQTTManager::getInstance().unsubscribe(m_subscription_handle);
        m_subscription_handle = 0;
    }
    if (m_lvgl_obj) {
        lv_obj_delete(m_lvgl_obj);
        m_lvgl_obj = nullptr;
        ESP_LOGI(TAG, "Destroyed bar widget: %s", m_id.c_str());
    }
}

void BarWidget::onMqttMessage(const std::string& topic, const std::string& payload) {
    int new_value = atoi(payload.c_str());
    
    // Clamp to range
    if (new_value < m_min) new_value = m_min;
    if (new_value > m_max) new_value = m_max;
    
    // Schedule async update
    AsyncUpdateData* data = new AsyncUpdateData{this, new_value};
    lv_async_call(async_update_cb, data);
}

void BarWidget::async_update_cb(void* user_data) {
    AsyncUpdateData* data = static_cast<AsyncUpdateData*>(user_data);
    if (data && data->widget) {
        data->widget->updateValue(data->value);
    }
    delete data;
}

void BarWidget::updateValue(int value) {
    if (m_lvgl_obj && lv_obj_is_valid(m_lvgl_obj)) {
        m_value = value;
        lv_bar_set_value(m_lvgl_obj, m_value, LV_ANIM_ON);
        ESP_LOGD(TAG, "Updated bar %s to value: %d", m_id.c_str(), m_value);
    }
}
