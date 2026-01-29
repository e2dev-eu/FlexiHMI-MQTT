#include "switch_widget.h"
#include "mqtt_manager.h"
#include <esp_log.h>

static const char *TAG = "SwitchWidget";

bool SwitchWidget::create(const std::string& id, int x, int y, int w, int h, cJSON* properties, lv_obj_t* parent) {
    m_id = id;
    m_state = false;
    
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
    
    // Add event callback
    lv_obj_add_event_cb(m_lvgl_obj, switch_event_cb, LV_EVENT_VALUE_CHANGED, this);
    
    ESP_LOGI(TAG, "Created switch widget: %s at (%d,%d)", id.c_str(), x, y);
    
    return true;
}

void SwitchWidget::destroy() {
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
    
    // Update switch state based on payload
    bool new_state = (payload == "ON" || payload == "1" || payload == "true");
    m_state = new_state;
    
    if (new_state) {
        lv_obj_add_state(m_lvgl_obj, LV_STATE_CHECKED);
    } else {
        lv_obj_clear_state(m_lvgl_obj, LV_STATE_CHECKED);
    }
    
    ESP_LOGD(TAG, "Updated switch %s: %s", m_id.c_str(), new_state ? "ON" : "OFF");
}

void SwitchWidget::switch_event_cb(lv_event_t* e) {
    SwitchWidget* widget = static_cast<SwitchWidget*>(lv_event_get_user_data(e));
    lv_obj_t* obj = static_cast<lv_obj_t*>(lv_event_get_target(e));
    
    bool state = lv_obj_has_state(obj, LV_STATE_CHECKED);
    widget->m_state = state;
    
    // Publish to MQTT if topic is configured
    if (!widget->m_mqtt_topic.empty()) {
        std::string payload = state ? "ON" : "OFF";
        MQTTManager::getInstance().publish(widget->m_mqtt_topic, payload, 0, false);
        ESP_LOGI(TAG, "Switch %s changed to %s, published to %s", 
                 widget->m_id.c_str(), payload.c_str(), widget->m_mqtt_topic.c_str());
    }
}
