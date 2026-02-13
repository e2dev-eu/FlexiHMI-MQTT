#include "checkbox_widget.h"
#include "mqtt_manager.h"
#include <esp_log.h>
#include <cstring>

static const char *TAG = "CheckboxWidget";

CheckboxWidget::CheckboxWidget(const std::string& id, int x, int y, int w, int h, cJSON* properties, lv_obj_t* parent) {
    m_id = id;
    m_checked = false;
    m_pending_checked = m_checked;
    m_retained = true;
    m_text = "Checkbox";
    
    // Extract properties
    if (properties) {
        cJSON* checked_item = cJSON_GetObjectItem(properties, "checked");
        if (checked_item && cJSON_IsBool(checked_item)) {
            m_checked = cJSON_IsTrue(checked_item);
        }
        
        cJSON* text_item = cJSON_GetObjectItem(properties, "text");
        if (text_item && cJSON_IsString(text_item)) {
            m_text = text_item->valuestring;
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
    
    // Create checkbox object
    lv_obj_t* parent_obj = parent ? parent : lv_screen_active();
    m_lvgl_obj = lv_checkbox_create(parent_obj);
    if (!m_lvgl_obj) {
        ESP_LOGE(TAG, "Failed to create checkbox widget: %s", id.c_str());
        return;
    }
    
    lv_obj_set_pos(m_lvgl_obj, x, y);
    lv_checkbox_set_text(m_lvgl_obj, m_text.c_str());
    
    // Set initial state
    if (m_checked) {
        lv_obj_add_state(m_lvgl_obj, LV_STATE_CHECKED);
    }
    
    // Apply custom color if specified
    if (m_has_color) {
        lv_obj_set_style_bg_color(m_lvgl_obj, m_color, LV_PART_INDICATOR);
    }
    
    // Add event callback
    lv_obj_add_event_cb(m_lvgl_obj, checkbox_event_cb, LV_EVENT_VALUE_CHANGED, this);
    
    // Subscribe to mqtt_topic to receive external updates
    if (!m_mqtt_topic.empty()) {
        m_subscription_handle = MQTTManager::getInstance().subscribe(m_mqtt_topic, 0,
            [this](const std::string& topic, const std::string& payload) {
                this->onMqttMessage(topic, payload);
            });
        ESP_LOGI(TAG, "Checkbox %s subscribed to %s for external updates", id.c_str(), m_mqtt_topic.c_str());
    }
    
    ESP_LOGI(TAG, "Created checkbox widget: %s at (%d,%d)", id.c_str(), x, y);
}

CheckboxWidget::~CheckboxWidget() {
    cancelAsync(async_update_cb, this);
    if (m_subscription_handle != 0) {
        MQTTManager::getInstance().unsubscribe(m_subscription_handle);
        m_subscription_handle = 0;
    }
    if (m_lvgl_obj) {
        lv_obj_delete(m_lvgl_obj);
        m_lvgl_obj = nullptr;
        ESP_LOGI(TAG, "Destroyed checkbox widget: %s", m_id.c_str());
    }
}

void CheckboxWidget::checkbox_event_cb(lv_event_t* e) {
    CheckboxWidget* widget = static_cast<CheckboxWidget*>(lv_event_get_user_data(e));
    if (!widget || widget->m_updating_from_mqtt) {
        return;
    }
    
    lv_obj_t* cb = static_cast<lv_obj_t*>(lv_event_get_target(e));
    bool new_state = lv_obj_has_state(cb, LV_STATE_CHECKED);
    
    if (new_state != widget->m_checked) {
        widget->m_checked = new_state;
        
        if (!widget->m_mqtt_topic.empty()) {
            const char* payload = new_state ? "true" : "false";
            MQTTManager::getInstance().publish(widget->m_mqtt_topic, payload, 0, widget->m_retained);
            ESP_LOGI(TAG, "Checkbox %s changed to %s, published to %s (retained=%d)", 
                     widget->m_id.c_str(), payload, widget->m_mqtt_topic.c_str(), widget->m_retained);
        }
    }
}

void CheckboxWidget::onMqttMessage(const std::string& topic, const std::string& payload) {
    bool new_state = false;
    
    // Parse various true/false formats
    if (strcasecmp(payload.c_str(), "true") == 0 || 
        strcasecmp(payload.c_str(), "1") == 0 ||
        strcasecmp(payload.c_str(), "checked") == 0) {
        new_state = true;
    }
    
    if (new_state != m_checked) {
        m_pending_checked = new_state;
        scheduleAsync(async_update_cb, this);
    }
}

void CheckboxWidget::async_update_cb(void* user_data) {
    CheckboxWidget* widget = static_cast<CheckboxWidget*>(user_data);
    if (!widget) {
        return;
    }
    widget->markAsyncComplete();
    widget->updateState(widget->m_pending_checked);
}

void CheckboxWidget::updateState(bool checked) {
    if (m_lvgl_obj && lv_obj_is_valid(m_lvgl_obj)) {
        m_updating_from_mqtt = true;
        m_checked = checked;
        if (checked) {
            lv_obj_add_state(m_lvgl_obj, LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(m_lvgl_obj, LV_STATE_CHECKED);
        }
        m_updating_from_mqtt = false;
        ESP_LOGD(TAG, "Updated checkbox %s to: %s", m_id.c_str(), checked ? "checked" : "unchecked");
    }
}
