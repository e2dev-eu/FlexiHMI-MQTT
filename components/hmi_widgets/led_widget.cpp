#include "led_widget.h"
#include "mqtt_manager.h"
#include <esp_log.h>
#include <cstring>
#include <cstdlib>

static const char *TAG = "LEDWidget";

bool LEDWidget::create(const std::string& id, int x, int y, int w, int h, cJSON* properties, lv_obj_t* parent) {
    m_id = id;
    m_brightness = 255;
    m_color_on = lv_color_hex(0x00FF00);   // Green
    m_color_off = lv_color_hex(0x808080);  // Gray
    
    // Extract properties
    if (properties) {
        cJSON* brightness_item = cJSON_GetObjectItem(properties, "brightness");
        if (brightness_item && cJSON_IsNumber(brightness_item)) {
            m_brightness = brightness_item->valueint;
        }
        
        cJSON* mqtt_topic = cJSON_GetObjectItem(properties, "mqtt_topic");
        if (mqtt_topic && cJSON_IsString(mqtt_topic)) {
            m_mqtt_topic = mqtt_topic->valuestring;
        }
        
        cJSON* color_on_item = cJSON_GetObjectItem(properties, "color_on");
        if (color_on_item && cJSON_IsString(color_on_item)) {
            const char* color_str = color_on_item->valuestring;
            if (color_str[0] == '#') {
                uint32_t color = strtol(color_str + 1, NULL, 16);
                m_color_on = lv_color_hex(color);
            }
        }
        
        cJSON* color_off_item = cJSON_GetObjectItem(properties, "color_off");
        if (color_off_item && cJSON_IsString(color_off_item)) {
            const char* color_str = color_off_item->valuestring;
            if (color_str[0] == '#') {
                uint32_t color = strtol(color_str + 1, NULL, 16);
                m_color_off = lv_color_hex(color);
            }
        }
    }
    
    // Create LED object
    lv_obj_t* parent_obj = parent ? parent : lv_screen_active();
    m_lvgl_obj = lv_led_create(parent_obj);
    if (!m_lvgl_obj) {
        ESP_LOGE(TAG, "Failed to create LED widget: %s", id.c_str());
        return false;
    }
    
    lv_obj_set_pos(m_lvgl_obj, x, y);
    if (w > 0 && h > 0) {
        lv_obj_set_size(m_lvgl_obj, w, h);
    }
    
    lv_led_set_color(m_lvgl_obj, m_color_on);
    lv_led_set_brightness(m_lvgl_obj, m_brightness);
    
    // Subscribe to mqtt_topic to receive updates
    if (!m_mqtt_topic.empty()) {
        m_subscription_handle = MQTTManager::getInstance().subscribe(m_mqtt_topic, 0,
            [this](const std::string& topic, const std::string& payload) {
                this->onMqttMessage(topic, payload);
            });
        ESP_LOGI(TAG, "LED %s subscribed to %s for external updates", id.c_str(), m_mqtt_topic.c_str());
    }
    
    ESP_LOGI(TAG, "Created LED widget: %s at (%d,%d)", id.c_str(), x, y);
    
    return true;
}

void LEDWidget::destroy() {
    if (m_subscription_handle != 0) {
        MQTTManager::getInstance().unsubscribe(m_subscription_handle);
        m_subscription_handle = 0;
    }
    if (m_lvgl_obj) {
        lv_obj_delete(m_lvgl_obj);
        m_lvgl_obj = nullptr;
        ESP_LOGI(TAG, "Destroyed LED widget: %s", m_id.c_str());
    }
}

void LEDWidget::onMqttMessage(const std::string& topic, const std::string& payload) {
    uint8_t new_brightness = 0;
    
    // Parse ON/OFF or numeric brightness
    if (strcasecmp(payload.c_str(), "ON") == 0 || strcasecmp(payload.c_str(), "1") == 0) {
        new_brightness = 255;
    } else if (strcasecmp(payload.c_str(), "OFF") == 0 || strcasecmp(payload.c_str(), "0") == 0) {
        new_brightness = 0;
    } else {
        new_brightness = atoi(payload.c_str());
        // uint8_t is automatically clamped to 0-255
    }
    
    AsyncUpdateData* data = new AsyncUpdateData{this, new_brightness};
    lv_async_call(async_update_cb, data);
}

void LEDWidget::async_update_cb(void* user_data) {
    AsyncUpdateData* data = static_cast<AsyncUpdateData*>(user_data);
    if (data && data->widget) {
        data->widget->updateBrightness(data->brightness);
    }
    delete data;
}

void LEDWidget::updateBrightness(uint8_t brightness) {
    if (m_lvgl_obj && lv_obj_is_valid(m_lvgl_obj)) {
        m_brightness = brightness;
        
        if (brightness == 0) {
            lv_led_off(m_lvgl_obj);
            lv_led_set_color(m_lvgl_obj, m_color_off);
        } else {
            lv_led_on(m_lvgl_obj);
            lv_led_set_color(m_lvgl_obj, m_color_on);
            lv_led_set_brightness(m_lvgl_obj, brightness);
        }
        
        ESP_LOGI(TAG, "Updated LED %s to brightness: %d", m_id.c_str(), m_brightness);
    }
}
