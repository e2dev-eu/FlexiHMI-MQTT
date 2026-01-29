#include "slider_widget.h"
#include "mqtt_manager.h"
#include <esp_log.h>
#include <cstdio>

static const char *TAG = "SliderWidget";

bool SliderWidget::create(const std::string& id, int x, int y, int w, int h, cJSON* properties, lv_obj_t* parent) {
    m_id = id;
    m_min = 0;
    m_max = 100;
    m_value = 50;
    
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
        
        cJSON* label_item = cJSON_GetObjectItem(properties, "label");
        if (label_item && cJSON_IsString(label_item)) {
            m_label = label_item->valuestring;
        }
        
        cJSON* mqtt_topic = cJSON_GetObjectItem(properties, "mqtt_topic");
        if (mqtt_topic && cJSON_IsString(mqtt_topic)) {
            m_mqtt_topic = mqtt_topic->valuestring;
        }
    }
    
    // Create label if specified
    lv_obj_t* parent_obj = parent ? parent : lv_screen_active();
    if (!m_label.empty()) {
        m_label_obj = lv_label_create(parent_obj);
        lv_label_set_text(m_label_obj, m_label.c_str());
        lv_obj_set_pos(m_label_obj, x, y - 25);
    }
    
    // Create slider object
    m_lvgl_obj = lv_slider_create(parent_obj);
    if (!m_lvgl_obj) {
        ESP_LOGE(TAG, "Failed to create slider widget: %s", id.c_str());
        return false;
    }
    
    lv_obj_set_pos(m_lvgl_obj, x, y);
    lv_obj_set_size(m_lvgl_obj, w, h);
    
    lv_slider_set_range(m_lvgl_obj, m_min, m_max);
    lv_slider_set_value(m_lvgl_obj, m_value, LV_ANIM_OFF);
    
    // Add event callback
    lv_obj_add_event_cb(m_lvgl_obj, slider_event_cb, LV_EVENT_VALUE_CHANGED, this);
    
    // Create value label below slider
    m_value_label = lv_label_create(lv_screen_active());
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", m_value);
    lv_label_set_text(m_value_label, buf);
    lv_obj_set_pos(m_value_label, x + w / 2 - 10, y + h + 5);
    
    ESP_LOGI(TAG, "Created slider widget: %s at (%d,%d) range [%d,%d]", 
             id.c_str(), x, y, m_min, m_max);
    
    return true;
}

void SliderWidget::destroy() {
    if (m_label_obj) {
        lv_obj_delete(m_label_obj);
        m_label_obj = nullptr;
    }
    if (m_value_label) {
        lv_obj_delete(m_value_label);
        m_value_label = nullptr;
    }
    if (m_lvgl_obj) {
        lv_obj_delete(m_lvgl_obj);
        m_lvgl_obj = nullptr;
        ESP_LOGI(TAG, "Destroyed slider widget: %s", m_id.c_str());
    }
}

void SliderWidget::onMqttMessage(const std::string& topic, const std::string& payload) {
    if (!m_lvgl_obj) {
        return;
    }
    
    int value = std::atoi(payload.c_str());
    if (value >= m_min && value <= m_max) {
        m_value = value;
        lv_slider_set_value(m_lvgl_obj, value, LV_ANIM_ON);
        
        // Update value label
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", value);
        lv_label_set_text(m_value_label, buf);
        
        ESP_LOGD(TAG, "Updated slider %s: %d", m_id.c_str(), value);
    }
}

void SliderWidget::slider_event_cb(lv_event_t* e) {
    SliderWidget* widget = static_cast<SliderWidget*>(lv_event_get_user_data(e));
    lv_obj_t* obj = static_cast<lv_obj_t*>(lv_event_get_target(e));
    
    int value = lv_slider_get_value(obj);
    widget->m_value = value;
    
    // Update value label
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", value);
    lv_label_set_text(widget->m_value_label, buf);
    
    // Publish to MQTT if topic is configured
    if (!widget->m_mqtt_topic.empty()) {
        char payload[16];
        snprintf(payload, sizeof(payload), "%d", value);
        MQTTManager::getInstance().publish(widget->m_mqtt_topic, payload, 0, false);
        ESP_LOGI(TAG, "Slider %s changed to %d, published to %s", 
                 widget->m_id.c_str(), value, widget->m_mqtt_topic.c_str());
    }
}
