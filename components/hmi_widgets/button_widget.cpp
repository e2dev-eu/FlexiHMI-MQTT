#include "button_widget.h"
#include "mqtt_manager.h"
#include <esp_log.h>

static const char *TAG = "ButtonWidget";

ButtonWidget::ButtonWidget(const std::string& id, int x, int y, int w, int h, cJSON* properties, lv_obj_t* parent) {
    m_id = id;
    m_label = nullptr;
    
    // Extract properties
    if (properties) {
        cJSON* text_item = cJSON_GetObjectItem(properties, "text");
        if (text_item && cJSON_IsString(text_item)) {
            m_button_text = text_item->valuestring;
        }
        
        cJSON* mqtt_topic_item = cJSON_GetObjectItem(properties, "mqtt_topic");
        if (mqtt_topic_item && cJSON_IsString(mqtt_topic_item)) {
            m_mqtt_topic = mqtt_topic_item->valuestring;
        }
        
        cJSON* mqtt_payload_item = cJSON_GetObjectItem(properties, "mqtt_payload");
        if (mqtt_payload_item && cJSON_IsString(mqtt_payload_item)) {
            m_mqtt_payload = mqtt_payload_item->valuestring;
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
    
    // Create button object
    lv_obj_t* parent_obj = parent ? parent : lv_screen_active();
    m_lvgl_obj = lv_button_create(parent_obj);
    if (!m_lvgl_obj) {
        ESP_LOGE(TAG, "Failed to create button widget: %s", id.c_str());
        return;
    }
    
    lv_obj_set_pos(m_lvgl_obj, x, y);
    lv_obj_set_size(m_lvgl_obj, w, h);
    
    // Apply custom color if specified
    if (m_has_color) {
        lv_obj_set_style_bg_color(m_lvgl_obj, m_color, LV_PART_MAIN);
    }
    
    // Create label on button
    m_label = lv_label_create(m_lvgl_obj);
    lv_label_set_text(m_label, m_button_text.empty() ? "Button" : m_button_text.c_str());
    lv_obj_center(m_label);
    
    // Set user data and event callback
    lv_obj_set_user_data(m_lvgl_obj, this);
    lv_obj_add_event_cb(m_lvgl_obj, button_event_cb, LV_EVENT_CLICKED, nullptr);
    
    ESP_LOGI(TAG, "Created button widget: %s at (%d,%d) size (%dx%d)", 
             id.c_str(), x, y, w, h);
}

ButtonWidget::~ButtonWidget() {
    if (m_lvgl_obj) {
        lv_obj_delete(m_lvgl_obj);
        m_lvgl_obj = nullptr;
        m_label = nullptr;
        ESP_LOGI(TAG, "Destroyed button widget: %s", m_id.c_str());
    }
}

void ButtonWidget::onMqttMessage(const std::string& topic, const std::string& payload) {
    // Buttons could receive MQTT messages to change text or state
    ESP_LOGD(TAG, "Button %s received message: %s", m_id.c_str(), payload.c_str());
}

void ButtonWidget::button_event_cb(lv_event_t* e) {
    lv_obj_t* obj = (lv_obj_t*)lv_event_get_target(e);
    ButtonWidget* widget = static_cast<ButtonWidget*>(lv_obj_get_user_data(obj));
    
    if (widget && !widget->m_mqtt_topic.empty()) {
        std::string payload = widget->m_mqtt_payload.empty() ? "clicked" : widget->m_mqtt_payload;
        MQTTManager::getInstance().publish(widget->m_mqtt_topic, payload, 0, widget->m_retained);
        ESP_LOGI(TAG, "Button %s clicked, published to %s: %s (retained=%d)", 
                 widget->m_id.c_str(), widget->m_mqtt_topic.c_str(), payload.c_str(), widget->m_retained);
    }
}

// Register this widget type
