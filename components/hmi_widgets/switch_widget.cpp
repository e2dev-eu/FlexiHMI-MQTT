#include "switch_widget.h"
#include "mqtt_manager.h"
#include <esp_log.h>

static const char *TAG = "SwitchWidget";

bool SwitchWidget::create(const std::string& id, int x, int y, int w, int h, cJSON* properties, lv_obj_t* parent) {
    m_id = id;
    m_state = false;
    m_retained = true;  // Default to retained messages
    
    // Extract properties
    if (properties) {
        cJSON* state_item = cJSON_GetObjectItem(properties, "state");
        if (state_item && cJSON_IsBool(state_item)) {
            m_state = cJSON_IsTrue(state_item);
        }
        
        cJSON* mqtt_topic = cJSON_GetObjectItem(properties, "mqtt_topic");
        if (mqtt_topic && cJSON_IsString(mqtt_topic)) {
            m_mqtt_topic = mqtt_topic->valuestring;
        }
        
        cJSON* retained_item = cJSON_GetObjectItem(properties, "retained");
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
    
    // Create switch object
    lv_obj_t* parent_obj = parent ? parent : lv_screen_active();
    m_lvgl_obj = lv_switch_create(parent_obj);
    if (!m_lvgl_obj) {
        ESP_LOGE(TAG, "Failed to create switch widget: %s", id.c_str());
        return false;
    }
    
    lv_obj_set_pos(m_lvgl_obj, x, y);
    if (w > 0 && h > 0) {
        lv_obj_set_size(m_lvgl_obj, w, h);
    }
    
    // Set initial state
    if (m_state) {
        lv_obj_add_state(m_lvgl_obj, LV_STATE_CHECKED);
    }
    
    // Apply custom color if specified
    if (m_has_color) {
        lv_obj_set_style_bg_color(m_lvgl_obj, m_color, LV_PART_INDICATOR);
    }
    
    // Add event callback
    lv_obj_add_event_cb(m_lvgl_obj, switch_event_cb, LV_EVENT_VALUE_CHANGED, this);
    
    // Subscribe to mqtt_topic to receive external updates
    if (!m_mqtt_topic.empty()) {
        m_subscription_handle = MQTTManager::getInstance().subscribe(m_mqtt_topic, 0,
            [this](const std::string& topic, const std::string& payload) {
                this->onMqttMessage(topic, payload);
            });
        ESP_LOGI(TAG, "Switch %s subscribed to %s for external updates", id.c_str(), m_mqtt_topic.c_str());
    }
    
    ESP_LOGI(TAG, "Created switch widget: %s at (%d,%d)", id.c_str(), x, y);
    
    return true;
}

void SwitchWidget::destroy() {
    if (m_subscription_handle != 0) {
        MQTTManager::getInstance().unsubscribe(m_subscription_handle);
        m_subscription_handle = 0;
    }
    if (m_lvgl_obj) {
        lv_obj_delete(m_lvgl_obj);
        m_lvgl_obj = nullptr;
        ESP_LOGI(TAG, "Destroyed switch widget: %s", m_id.c_str());
    }
}

void SwitchWidget::onMqttMessage(const std::string& topic, const std::string& payload) {
    if (!m_lvgl_obj) {
        return;
    }
    
    // Parse new state
    bool new_state = (payload == "ON" || payload == "1" || payload == "true");
    
    // Only update if state actually changed
    if (new_state == m_state) {
        return;
    }
    
    // Allocate data for async call (will be freed in callback)
    AsyncUpdateData* data = new AsyncUpdateData{this, new_state};
    
    // Schedule update on LVGL task
    lv_async_call(async_update_cb, data);
    
    ESP_LOGD(TAG, "Scheduled async update for switch %s: %s", m_id.c_str(), new_state ? "ON" : "OFF");
}

void SwitchWidget::async_update_cb(void* user_data) {
    AsyncUpdateData* data = static_cast<AsyncUpdateData*>(user_data);
    if (data && data->widget) {
        data->widget->updateState(data->state);
    }
    delete data;
}

void SwitchWidget::updateState(bool new_state) {
    if (!m_lvgl_obj || !lv_obj_is_valid(m_lvgl_obj)) {
        return;
    }
    
    m_state = new_state;
    
    // Set flag to prevent event callback from publishing
    m_updating_from_mqtt = true;
    
    if (new_state) {
        lv_obj_add_state(m_lvgl_obj, LV_STATE_CHECKED);
    } else {
        lv_obj_clear_state(m_lvgl_obj, LV_STATE_CHECKED);
    }
    
    m_updating_from_mqtt = false;
    
    ESP_LOGD(TAG, "Updated switch %s: %s", m_id.c_str(), new_state ? "ON" : "OFF");
}

void SwitchWidget::switch_event_cb(lv_event_t* e) {
    SwitchWidget* widget = static_cast<SwitchWidget*>(lv_event_get_user_data(e));
    
    // Don't publish if we're updating from MQTT (prevent feedback loop)
    if (widget->m_updating_from_mqtt) {
        return;
    }
    
    lv_obj_t* obj = static_cast<lv_obj_t*>(lv_event_get_target(e));
    
    bool state = lv_obj_has_state(obj, LV_STATE_CHECKED);
    widget->m_state = state;
    
    // Publish to MQTT if topic is configured
    if (!widget->m_mqtt_topic.empty()) {
        std::string payload = state ? "ON" : "OFF";
        MQTTManager::getInstance().publish(widget->m_mqtt_topic, payload, 0, widget->m_retained);
        ESP_LOGI(TAG, "Switch %s changed to %s, published to %s (retained=%d)", 
                 widget->m_id.c_str(), payload.c_str(), widget->m_mqtt_topic.c_str(), widget->m_retained);
    }
}
