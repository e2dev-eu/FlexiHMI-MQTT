#include "spinner_widget.h"
#include "mqtt_manager.h"
#include <esp_log.h>
#include <cstring>

static const char *TAG = "SpinnerWidget";

bool SpinnerWidget::create(const std::string& id, int x, int y, int w, int h, cJSON* properties, lv_obj_t* parent) {
    m_id = id;
    
    // Extract properties
    if (properties) {
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
    
    // Create spinner object
    lv_obj_t* parent_obj = parent ? parent : lv_screen_active();
    m_lvgl_obj = lv_spinner_create(parent_obj);
    if (!m_lvgl_obj) {
        ESP_LOGE(TAG, "Failed to create spinner widget: %s", id.c_str());
        return false;
    }
    
    lv_obj_set_pos(m_lvgl_obj, x, y);
    lv_obj_set_size(m_lvgl_obj, w, h);
    
    // Apply custom color if specified
    if (m_has_color) {
        lv_obj_set_style_arc_color(m_lvgl_obj, m_color, LV_PART_INDICATOR);
    }
    
    // Subscribe to mqtt_topic to control visibility
    if (!m_mqtt_topic.empty()) {
        m_subscription_handle = MQTTManager::getInstance().subscribe(m_mqtt_topic, 0,
            [this](const std::string& topic, const std::string& payload) {
                this->onMqttMessage(topic, payload);
            });
        ESP_LOGI(TAG, "Spinner %s subscribed to %s for external updates", id.c_str(), m_mqtt_topic.c_str());
    }
    
    ESP_LOGI(TAG, "Created spinner widget: %s at (%d,%d)", id.c_str(), x, y);
    
    return true;
}

void SpinnerWidget::destroy() {
    if (m_subscription_handle != 0) {
        MQTTManager::getInstance().unsubscribe(m_subscription_handle);
        m_subscription_handle = 0;
    }
    if (m_lvgl_obj) {
        lv_obj_delete(m_lvgl_obj);
        m_lvgl_obj = nullptr;
        ESP_LOGI(TAG, "Destroyed spinner widget: %s", m_id.c_str());
    }
}

void SpinnerWidget::onMqttMessage(const std::string& topic, const std::string& payload) {
    bool visible = false;
    
    if (strcasecmp(payload.c_str(), "show") == 0 || 
        strcasecmp(payload.c_str(), "true") == 0 ||
        strcasecmp(payload.c_str(), "1") == 0) {
        visible = true;
    }
    
    AsyncUpdateData* data = new AsyncUpdateData{this, visible};
    lv_async_call(async_update_cb, data);
}

void SpinnerWidget::async_update_cb(void* user_data) {
    AsyncUpdateData* data = static_cast<AsyncUpdateData*>(user_data);
    if (data && data->widget) {
        data->widget->updateVisibility(data->visible);
    }
    delete data;
}

void SpinnerWidget::updateVisibility(bool visible) {
    if (m_lvgl_obj && lv_obj_is_valid(m_lvgl_obj)) {
        if (visible) {
            lv_obj_clear_flag(m_lvgl_obj, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(m_lvgl_obj, LV_OBJ_FLAG_HIDDEN);
        }
        ESP_LOGD(TAG, "Updated spinner %s visibility: %s", m_id.c_str(), visible ? "visible" : "hidden");
    }
}
