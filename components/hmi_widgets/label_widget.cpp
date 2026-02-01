#include "label_widget.h"
#include "mqtt_manager.h"
#include <esp_log.h>
#include <cstdio>

static const char *TAG = "LabelWidget";

bool LabelWidget::create(const std::string& id, int x, int y, int w, int h, cJSON* properties, lv_obj_t* parent) {
    m_id = id;
    
    int font_size = 0;
    const char* align_str = nullptr;
    
    // Extract properties
    if (properties) {
        cJSON* text_item = cJSON_GetObjectItem(properties, "text");
        if (text_item && cJSON_IsString(text_item)) {
            m_text = text_item->valuestring;
        }
        
        cJSON* format_item = cJSON_GetObjectItem(properties, "format");
        if (format_item && cJSON_IsString(format_item)) {
            m_format = format_item->valuestring;
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
        
        cJSON* font_size_item = cJSON_GetObjectItem(properties, "font_size");
        if (font_size_item && cJSON_IsNumber(font_size_item)) {
            font_size = font_size_item->valueint;
        }
        
        cJSON* align_item = cJSON_GetObjectItem(properties, "align");
        if (align_item && cJSON_IsString(align_item)) {
            align_str = align_item->valuestring;
        }
    }
    
    // Create label object
    lv_obj_t* parent_obj = parent ? parent : lv_screen_active();
    m_lvgl_obj = lv_label_create(parent_obj);
    if (!m_lvgl_obj) {
        ESP_LOGE(TAG, "Failed to create label widget: %s", id.c_str());
        return false;
    }
    
    lv_obj_set_pos(m_lvgl_obj, x, y);
    lv_obj_set_size(m_lvgl_obj, w, h);
    lv_label_set_text(m_lvgl_obj, m_text.empty() ? "Label" : m_text.c_str());
    lv_label_set_long_mode(m_lvgl_obj, LV_LABEL_LONG_WRAP);
    
    // Apply text alignment
    if (align_str) {
        if (strcmp(align_str, "center") == 0) {
            lv_obj_set_style_text_align(m_lvgl_obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        } else if (strcmp(align_str, "right") == 0) {
            lv_obj_set_style_text_align(m_lvgl_obj, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
        } else if (strcmp(align_str, "left") == 0) {
            lv_obj_set_style_text_align(m_lvgl_obj, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
        }
    }
    
    // Apply font size
    if (font_size > 0) {
        // LVGL built-in fonts: 10, 12, 14, 16, 18, 20, 24, 28, 32, 36, 48
        const lv_font_t* font = nullptr;
        if (font_size <= 10) font = &lv_font_montserrat_10;
        else if (font_size <= 12) font = &lv_font_montserrat_12;
        else if (font_size <= 14) font = &lv_font_montserrat_14;
        else if (font_size <= 16) font = &lv_font_montserrat_16;
        else if (font_size <= 18) font = &lv_font_montserrat_18;
        else if (font_size <= 20) font = &lv_font_montserrat_20;
        else if (font_size <= 24) font = &lv_font_montserrat_24;
        else if (font_size <= 28) font = &lv_font_montserrat_28;
        else if (font_size <= 32) font = &lv_font_montserrat_32;
        else if (font_size <= 36) font = &lv_font_montserrat_36;
        else font = &lv_font_montserrat_48;
        
        if (font) {
            lv_obj_set_style_text_font(m_lvgl_obj, font, LV_PART_MAIN);
        }
    }
    
    // Apply custom color if specified
    if (m_has_color) {
        lv_obj_set_style_text_color(m_lvgl_obj, m_color, LV_PART_MAIN);
    }
    
    // Subscribe to MQTT topic if specified
    if (!m_mqtt_topic.empty()) {
        m_subscription_handle = MQTTManager::getInstance().subscribe(m_mqtt_topic, 0,
            [this](const std::string& topic, const std::string& payload) {
                this->onMqttMessage(topic, payload);
            });
        
        if (m_subscription_handle != 0) {
            ESP_LOGI(TAG, "Label %s subscribed to %s for updates", id.c_str(), m_mqtt_topic.c_str());
        }
    }
    
    ESP_LOGI(TAG, "Created label widget: %s at (%d,%d) size (%dx%d)", 
             id.c_str(), x, y, w, h);
    
    return true;
}

void LabelWidget::destroy() {
    if (m_subscription_handle != 0) {
        MQTTManager::getInstance().unsubscribe(m_subscription_handle);
        m_subscription_handle = 0;
    }
    
    if (m_lvgl_obj) {
        lv_obj_delete(m_lvgl_obj);
        m_lvgl_obj = nullptr;
        ESP_LOGI(TAG, "Destroyed label widget: %s", m_id.c_str());
    }
}

void LabelWidget::onMqttMessage(const std::string& topic, const std::string& payload) {
    // Format the text if needed
    std::string new_text;
    if (!m_format.empty()) {
        char formatted[256];
        snprintf(formatted, sizeof(formatted), m_format.c_str(), payload.c_str());
        new_text = formatted;
    } else {
        new_text = payload;
    }
    
    // Use async call to update LVGL widget from LVGL thread
    AsyncUpdateData* data = new AsyncUpdateData{this, new_text};
    lv_async_call(async_update_cb, data);
}

void LabelWidget::async_update_cb(void* user_data) {
    AsyncUpdateData* data = static_cast<AsyncUpdateData*>(user_data);
    if (data && data->widget) {
        data->widget->updateText(data->text);
    }
    delete data;
}

void LabelWidget::updateText(const std::string& text) {
    if (!m_lvgl_obj || !lv_obj_is_valid(m_lvgl_obj)) {
        return;
    }
    
    m_text = text;
    lv_label_set_text(m_lvgl_obj, m_text.c_str());
    ESP_LOGD(TAG, "Updated label %s: %s", m_id.c_str(), m_text.c_str());
}

// Register this widget type
