#include "container_widget.h"
#include <esp_log.h>

static const char *TAG = "ContainerWidget";

bool ContainerWidget::create(const std::string& id, int x, int y, int w, int h, cJSON* properties, lv_obj_t* parent) {
    m_id = id;
    
    // Create container object
    lv_obj_t* parent_obj = parent ? parent : lv_screen_active();
    m_lvgl_obj = lv_obj_create(parent_obj);
    if (!m_lvgl_obj) {
        ESP_LOGE(TAG, "Failed to create container widget: %s", id.c_str());
        return false;
    }
    
    lv_obj_set_pos(m_lvgl_obj, x, y);
    lv_obj_set_size(m_lvgl_obj, w, h);
    
    // Apply styling
    if (properties) {
        cJSON* bg_color = cJSON_GetObjectItem(properties, "bg_color");
        if (bg_color && cJSON_IsString(bg_color)) {
            uint32_t color = strtol(bg_color->valuestring + 1, NULL, 16); // Skip '#'
            lv_obj_set_style_bg_color(m_lvgl_obj, lv_color_hex(color), 0);
        }
        
        cJSON* border_width = cJSON_GetObjectItem(properties, "border_width");
        if (border_width && cJSON_IsNumber(border_width)) {
            lv_obj_set_style_border_width(m_lvgl_obj, border_width->valueint, 0);
        }
        
        cJSON* pad = cJSON_GetObjectItem(properties, "padding");
        if (pad && cJSON_IsNumber(pad)) {
            lv_obj_set_style_pad_all(m_lvgl_obj, pad->valueint, 0);
        }
    }
    
    ESP_LOGI(TAG, "Created container widget: %s at (%d,%d) size (%dx%d)", 
             id.c_str(), x, y, w, h);
    
    return true;
}

void ContainerWidget::destroy() {
    if (m_lvgl_obj) {
        lv_obj_delete(m_lvgl_obj);
        m_lvgl_obj = nullptr;
        ESP_LOGI(TAG, "Destroyed container widget: %s", m_id.c_str());
    }
}

void ContainerWidget::onMqttMessage(const std::string& topic, const std::string& payload) {
    // Containers don't react to MQTT messages directly
}
