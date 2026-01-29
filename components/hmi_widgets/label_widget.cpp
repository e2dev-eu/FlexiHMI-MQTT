#include "label_widget.h"
#include <esp_log.h>
#include <cstdio>

static const char *TAG = "LabelWidget";

bool LabelWidget::create(const std::string& id, int x, int y, int w, int h, cJSON* properties, lv_obj_t* parent) {
    m_id = id;
    
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
    
    // Apply custom color if specified
    if (m_has_color) {
        lv_obj_set_style_text_color(m_lvgl_obj, m_color, LV_PART_MAIN);
    }
    
    ESP_LOGI(TAG, "Created label widget: %s at (%d,%d) size (%dx%d)", 
             id.c_str(), x, y, w, h);
    
    return true;
}

void LabelWidget::destroy() {
    if (m_lvgl_obj) {
        lv_obj_delete(m_lvgl_obj);
        m_lvgl_obj = nullptr;
        ESP_LOGI(TAG, "Destroyed label widget: %s", m_id.c_str());
    }
}

void LabelWidget::onMqttMessage(const std::string& topic, const std::string& payload) {
    if (!m_lvgl_obj) {
        ESP_LOGW(TAG, "Widget not created yet: %s", m_id.c_str());
        return;
    }

    // If format string is provided, use it
    if (!m_format.empty()) {
        char formatted[128];
        float value = std::atof(payload.c_str());
        snprintf(formatted, sizeof(formatted), m_format.c_str(), value);
        m_text = formatted;
    } else {
        m_text = payload;
    }

    lv_label_set_text(m_lvgl_obj, m_text.c_str());
    ESP_LOGD(TAG, "Updated label %s: %s", m_id.c_str(), m_text.c_str());
}

// Register this widget type
