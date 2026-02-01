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
    m_retained = true;  // Default to retained messages
    m_last_published_value = -1;  // Invalid value, no echo to ignore yet
    
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
        
        cJSON* retained_item = cJSON_GetObjectItem(properties, "mqtt_retained");
        if (retained_item && cJSON_IsBool(retained_item)) {
            m_retained = cJSON_IsTrue(retained_item);
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
    
    // Apply custom color if specified
    if (m_has_color) {
        lv_obj_set_style_bg_color(m_lvgl_obj, m_color, LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(m_lvgl_obj, m_color, LV_PART_KNOB);
    }
    
    // Add event callback
    lv_obj_add_event_cb(m_lvgl_obj, slider_event_cb, LV_EVENT_VALUE_CHANGED, this);
    
    // Create value label below slider
    m_value_label = lv_label_create(parent_obj);
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", m_value);
    lv_label_set_text(m_value_label, buf);
    lv_obj_set_pos(m_value_label, x + w / 2, y + h / 2 - 5);
    
    // Subscribe to mqtt_topic to receive external updates
    if (!m_mqtt_topic.empty()) {
        m_subscription_handle = MQTTManager::getInstance().subscribe(m_mqtt_topic, 0,
            [this](const std::string& topic, const std::string& payload) {
                this->onMqttMessage(topic, payload);
            });
        ESP_LOGI(TAG, "Slider %s subscribed to %s for external updates", id.c_str(), m_mqtt_topic.c_str());
    }
    
    ESP_LOGI(TAG, "Created slider widget: %s at (%d,%d) range [%d,%d]", 
             id.c_str(), x, y, m_min, m_max);
    
    return true;
}

void SliderWidget::destroy() {
    if (m_subscription_handle != 0) {
        MQTTManager::getInstance().unsubscribe(m_subscription_handle);
        m_subscription_handle = 0;
    }
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
        // Ignore if this is the exact value we just published (our own echo)
        if (value == m_last_published_value && m_last_published_value != -1) {
            ESP_LOGD(TAG, "Slider %s ignoring own published value: %d", m_id.c_str(), value);
            m_last_published_value = -1;  // Clear flag after first ignore
            return;
        }
        
        // Only update if the new value is different from current
        if (value == m_value) {
            return;
        }
        
        // Allocate data for async call (will be freed in callback)
        AsyncUpdateData* data = new AsyncUpdateData{this, value};
        
        // Schedule update on LVGL task
        lv_async_call(async_update_cb, data);
        
        ESP_LOGD(TAG, "Scheduled async update for slider %s: %d", m_id.c_str(), value);
    }
}

void SliderWidget::async_update_cb(void* user_data) {
    AsyncUpdateData* data = static_cast<AsyncUpdateData*>(user_data);
    if (data && data->widget) {
        data->widget->updateValue(data->value);
    }
    delete data;
}

void SliderWidget::updateValue(int value) {
    if (!m_lvgl_obj || !lv_obj_is_valid(m_lvgl_obj)) {
        return;
    }
    
    m_value = value;
    
    // Set flag to prevent event callback from publishing
    m_updating_from_mqtt = true;
    
    lv_slider_set_value(m_lvgl_obj, value, LV_ANIM_ON);
    
    // Update value label
    if (m_value_label && lv_obj_is_valid(m_value_label)) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", value);
        lv_label_set_text(m_value_label, buf);
    }
    
    // Keep flag set briefly to ensure event is processed with flag still true
    lv_timer_handler();
    m_updating_from_mqtt = false;
    
    ESP_LOGD(TAG, "Updated slider %s: %d", m_id.c_str(), value);
}

void SliderWidget::slider_event_cb(lv_event_t* e) {
    SliderWidget* widget = static_cast<SliderWidget*>(lv_event_get_user_data(e));
    
    // Don't publish if we're updating from MQTT (prevent feedback loop)
    if (widget->m_updating_from_mqtt) {
        return;
    }
    
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
        widget->m_last_published_value = value;  // Track published value to ignore echo
        MQTTManager::getInstance().publish(widget->m_mqtt_topic, payload, 0, widget->m_retained);
        ESP_LOGD(TAG, "Slider %s changed to %d, published to %s (retained=%d)", 
                 widget->m_id.c_str(), value, widget->m_mqtt_topic.c_str(), widget->m_retained);
    }
}
