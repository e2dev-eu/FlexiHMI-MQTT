#include "dropdown_widget.h"
#include "mqtt_manager.h"
#include <esp_log.h>
#include <cstdlib>

static const char *TAG = "DropdownWidget";

DropdownWidget::DropdownWidget(const std::string& id, int x, int y, int w, int h, cJSON* properties, lv_obj_t* parent) {
    m_id = id;
    m_selected = 0;
    m_pending_selected = m_selected;
    m_retained = true;
    m_last_published_payload = "";  // No echo to ignore yet
    
    // Extract properties
    if (properties) {
        cJSON* options_item = cJSON_GetObjectItem(properties, "options");
        if (options_item && cJSON_IsArray(options_item)) {
            int count = cJSON_GetArraySize(options_item);
            for (int i = 0; i < count; i++) {
                cJSON* opt = cJSON_GetArrayItem(options_item, i);
                if (opt && cJSON_IsString(opt)) {
                    m_options.push_back(opt->valuestring);
                }
            }
        }
        
        cJSON* selected_item = cJSON_GetObjectItem(properties, "selected");
        if (selected_item && cJSON_IsNumber(selected_item)) {
            m_selected = selected_item->valueint;
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
    
    // Create dropdown object
    lv_obj_t* parent_obj = parent ? parent : lv_screen_active();
    m_lvgl_obj = lv_dropdown_create(parent_obj);
    if (!m_lvgl_obj) {
        ESP_LOGE(TAG, "Failed to create dropdown widget: %s", id.c_str());
        return;
    }
    
    lv_obj_set_pos(m_lvgl_obj, x, y);
    lv_obj_set_width(m_lvgl_obj, w);
    
    // Build options string (newline-separated)
    std::string options_str;
    for (size_t i = 0; i < m_options.size(); i++) {
        if (i > 0) options_str += "\n";
        options_str += m_options[i];
    }
    
    if (!options_str.empty()) {
        lv_dropdown_set_options(m_lvgl_obj, options_str.c_str());
    }
    
    lv_dropdown_set_selected(m_lvgl_obj, m_selected);
    
    // Apply custom color if specified
    if (m_has_color) {
        lv_obj_set_style_bg_color(m_lvgl_obj, m_color, LV_PART_MAIN);
    }
    
    // Add event callback
    lv_obj_add_event_cb(m_lvgl_obj, dropdown_event_cb, LV_EVENT_VALUE_CHANGED, this);
    
    // Subscribe to mqtt_topic to receive external updates
    if (!m_mqtt_topic.empty()) {
        m_subscription_handle = MQTTManager::getInstance().subscribe(m_mqtt_topic, 0,
            [this](const std::string& topic, const std::string& payload) {
                this->onMqttMessage(topic, payload);
            });
        ESP_LOGI(TAG, "Dropdown %s subscribed to %s for external updates", id.c_str(), m_mqtt_topic.c_str());
    }
    
    ESP_LOGI(TAG, "Created dropdown widget: %s at (%d,%d) with %d options", 
             id.c_str(), x, y, m_options.size());
}

DropdownWidget::~DropdownWidget() {
    cancelAsync(async_update_cb, this);
    if (m_subscription_handle != 0) {
        MQTTManager::getInstance().unsubscribe(m_subscription_handle);
        m_subscription_handle = 0;
    }
    if (m_lvgl_obj) {
        lv_obj_delete(m_lvgl_obj);
        m_lvgl_obj = nullptr;
        ESP_LOGI(TAG, "Destroyed dropdown widget: %s", m_id.c_str());
    }
}

void DropdownWidget::dropdown_event_cb(lv_event_t* e) {
    DropdownWidget* widget = static_cast<DropdownWidget*>(lv_event_get_user_data(e));
    if (!widget || widget->m_updating_from_mqtt) {
        return;
    }
    
    lv_obj_t* dd = static_cast<lv_obj_t*>(lv_event_get_target(e));
    uint16_t new_selected = lv_dropdown_get_selected(dd);
    
    if (new_selected != widget->m_selected) {
        widget->m_selected = new_selected;
        
        if (!widget->m_mqtt_topic.empty() && new_selected < widget->m_options.size()) {
            const char* payload = widget->m_options[new_selected].c_str();
            widget->m_last_published_payload = payload;  // Track published payload to ignore echo
            MQTTManager::getInstance().publish(widget->m_mqtt_topic, payload, 0, widget->m_retained);
            ESP_LOGI(TAG, "Dropdown %s changed to %s, published to %s (retained=%d)", 
                     widget->m_id.c_str(), payload, widget->m_mqtt_topic.c_str(), widget->m_retained);
        }
    }
}

void DropdownWidget::onMqttMessage(const std::string& topic, const std::string& payload) {
    // Check if payload matches what we just published (ignore own echo)
    if (m_last_published_payload == payload && !m_last_published_payload.empty()) {
        ESP_LOGD(TAG, "Dropdown %s ignoring own published value: %s", m_id.c_str(), payload.c_str());
        m_last_published_payload.clear();  // Clear after first ignore
        return;
    }
    
    uint16_t new_selected = m_selected;
    
    // Check if payload is a pure number (index)
    bool is_numeric = !payload.empty() && (isdigit(payload[0]) || payload[0] == '-');
    if (is_numeric) {
        for (size_t i = 1; i < payload.length(); i++) {
            if (!isdigit(payload[i])) {
                is_numeric = false;
                break;
            }
        }
    }
    
    if (is_numeric) {
        // Parse as index
        int index = atoi(payload.c_str());
        if (index >= 0 && index < (int)m_options.size()) {
            new_selected = index;
        }
    } else {
        // Find matching option string
        for (size_t i = 0; i < m_options.size(); i++) {
            if (m_options[i] == payload) {
                new_selected = i;
                break;
            }
        }
    }
    
    if (new_selected != m_selected) {
        m_pending_selected = new_selected;
        scheduleAsync(async_update_cb, this);
    }
}

void DropdownWidget::async_update_cb(void* user_data) {
    DropdownWidget* widget = static_cast<DropdownWidget*>(user_data);
    if (!widget) {
        return;
    }
    widget->markAsyncComplete();
    widget->updateSelection(widget->m_pending_selected);
}

void DropdownWidget::updateSelection(uint16_t selected) {
    if (m_lvgl_obj && lv_obj_is_valid(m_lvgl_obj)) {
        m_selected = selected;
        m_updating_from_mqtt = true;
        lv_dropdown_set_selected(m_lvgl_obj, m_selected);
        // Keep flag set for a brief moment to ensure event callback sees it
        lv_timer_handler();  // Process any pending events
        m_updating_from_mqtt = false;
        ESP_LOGD(TAG, "Updated dropdown %s to index: %d", m_id.c_str(), m_selected);
    }
}
