#include "gauge_widget.h"
#include "mqtt_manager.h"
#include <esp_log.h>
#include <cstring>
#include <cstdlib>

static const char *TAG = "GaugeWidget";

bool GaugeWidget::create(const std::string& id, int x, int y, int w, int h, cJSON* properties, lv_obj_t* parent) {
    m_id = id;
    m_value = 0;
    m_min_value = 0;
    m_max_value = 100;
    
    // Extract properties
    if (properties) {
        cJSON* min_value = cJSON_GetObjectItem(properties, "min_value");
        if (min_value && cJSON_IsNumber(min_value)) {
            m_min_value = min_value->valueint;
        }
        
        cJSON* max_value = cJSON_GetObjectItem(properties, "max_value");
        if (max_value && cJSON_IsNumber(max_value)) {
            m_max_value = max_value->valueint;
        }
        
        cJSON* value = cJSON_GetObjectItem(properties, "value");
        if (value && cJSON_IsNumber(value)) {
            m_value = value->valueint;
        }
        
        cJSON* mqtt_topic = cJSON_GetObjectItem(properties, "mqtt_topic");
        if (mqtt_topic && cJSON_IsString(mqtt_topic)) {
            m_mqtt_topic = mqtt_topic->valuestring;
        }
    }
    
    // Clamp initial value
    if (m_value < m_min_value) m_value = m_min_value;
    if (m_value > m_max_value) m_value = m_max_value;
    
    // Create scale object (this becomes the main widget)
    lv_obj_t* parent_obj = parent ? parent : lv_screen_active();
    m_lvgl_obj = lv_scale_create(parent_obj);
    if (!m_lvgl_obj) {
        ESP_LOGE(TAG, "Failed to create gauge (scale): %s", id.c_str());
        return false;
    }
    
    lv_obj_set_pos(m_lvgl_obj, x, y);
    
    // Use the smaller of width/height for the scale size (circular gauge)
    int32_t scale_size = (w < h) ? w : h;
    if (scale_size <= 0) {
        scale_size = 200; // Default size
    }
    
    lv_obj_set_size(m_lvgl_obj, scale_size, scale_size);
    
    // Configure scale for round inner mode
    lv_scale_set_mode(m_lvgl_obj, LV_SCALE_MODE_ROUND_INNER);
    lv_scale_set_label_show(m_lvgl_obj, true);
    
    // Set range
    lv_scale_set_range(m_lvgl_obj, m_min_value, m_max_value);
    
    // Configure tick counts
    lv_scale_set_total_tick_count(m_lvgl_obj, 41);   // Total ticks
    lv_scale_set_major_tick_every(m_lvgl_obj, 5);    // Major tick every 5
    
    // Set tick lengths
    lv_obj_set_style_length(m_lvgl_obj, 5, LV_PART_ITEMS);       // Minor tick length
    lv_obj_set_style_length(m_lvgl_obj, 10, LV_PART_INDICATOR);  // Major tick length
    
    // Set angle range and rotation (240 degrees starting at 6 o'clock)
    lv_scale_set_angle_range(m_lvgl_obj, 240);
    lv_scale_set_rotation(m_lvgl_obj, 150);  // Start at 6 o'clock position
    
    // Style the scale arc
    lv_obj_set_style_arc_color(m_lvgl_obj, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
    lv_obj_set_style_arc_width(m_lvgl_obj, 4, LV_PART_MAIN);
    
    // Create needle (line)
    m_needle = lv_line_create(m_lvgl_obj);
    lv_obj_set_style_line_width(m_needle, 3, LV_PART_MAIN);
    lv_obj_set_style_line_color(m_needle, lv_palette_main(LV_PALETTE_RED), LV_PART_MAIN);
    lv_obj_set_style_line_rounded(m_needle, true, LV_PART_MAIN);
    
    // Set initial needle position
    int32_t needle_length = scale_size / 2 - 15;  // Slightly shorter than radius
    lv_scale_set_line_needle_value(m_lvgl_obj, m_needle, needle_length, m_value);
    
    // Subscribe to MQTT topic if specified
    if (!m_mqtt_topic.empty()) {
        m_subscription_handle = MQTTManager::getInstance().subscribe(m_mqtt_topic, 0,
            [this](const std::string& topic, const std::string& payload) {
                this->onMqttMessage(topic, payload);
            });
        
        if (m_subscription_handle != 0) {
            ESP_LOGI(TAG, "Gauge %s subscribed to %s for external updates", id.c_str(), m_mqtt_topic.c_str());
        }
    }
    
    ESP_LOGI(TAG, "Created gauge widget: %s at (%d,%d) size %dx%d range [%d,%d]", 
             id.c_str(), x, y, scale_size, scale_size, m_min_value, m_max_value);
    return true;
}

void GaugeWidget::destroy() {
    if (m_subscription_handle != 0) {
        MQTTManager::getInstance().unsubscribe(m_subscription_handle);
        m_subscription_handle = 0;
    }
    
    if (m_lvgl_obj && lv_obj_is_valid(m_lvgl_obj)) {
        // Needle is a child of scale, so it will be deleted automatically
        lv_obj_del(m_lvgl_obj);
        m_lvgl_obj = nullptr;
        m_needle = nullptr;
    }
}

void GaugeWidget::onMqttMessage(const std::string& topic, const std::string& payload) {
    int new_value = atoi(payload.c_str());
    
    // Clamp value to range
    if (new_value < m_min_value) new_value = m_min_value;
    if (new_value > m_max_value) new_value = m_max_value;
    
    AsyncUpdateData* data = new AsyncUpdateData{this, new_value};
    lv_async_call(async_update_cb, data);
}

void GaugeWidget::async_update_cb(void* user_data) {
    AsyncUpdateData* data = static_cast<AsyncUpdateData*>(user_data);
    if (data && data->widget) {
        data->widget->updateValue(data->value);
    }
    delete data;
}

void GaugeWidget::updateValue(int value) {
    if (!m_lvgl_obj || !lv_obj_is_valid(m_lvgl_obj) || !m_needle) {
        return;
    }
    
    m_value = value;
    
    // Calculate needle length based on scale size
    int32_t scale_size = lv_obj_get_width(m_lvgl_obj);
    int32_t needle_length = scale_size / 2 - 15;
    
    // Update needle position using LVGL scale function
    lv_scale_set_line_needle_value(m_lvgl_obj, m_needle, needle_length, m_value);
    
    ESP_LOGD(TAG, "Updated gauge %s to value: %d", m_id.c_str(), m_value);
}
