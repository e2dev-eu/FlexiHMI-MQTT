#include "tabview_widget.h"
#include "mqtt_manager.h"
#include <esp_log.h>

static const char *TAG = "TabviewWidget";

bool TabviewWidget::create(const std::string& id, int x, int y, int w, int h, cJSON* properties, lv_obj_t* parent) {
    m_id = id;
    m_updating_from_mqtt = false;
    m_retained = true;
    m_active_tab = 0;
    
    // Get tab names from properties
    if (!properties) {
        ESP_LOGE(TAG, "Missing properties for tabview widget: %s", id.c_str());
        return false;
    }
    
    cJSON* tabs = cJSON_GetObjectItem(properties, "tabs");
    if (!tabs || !cJSON_IsArray(tabs)) {
        ESP_LOGE(TAG, "Missing or invalid 'tabs' array for tabview widget: %s", id.c_str());
        return false;
    }
    
    int tab_count = cJSON_GetArraySize(tabs);
    if (tab_count == 0) {
        ESP_LOGE(TAG, "Empty tabs array for tabview widget: %s", id.c_str());
        return false;
    }
    
    // Store tab names
    for (int i = 0; i < tab_count; i++) {
        cJSON* tab_name = cJSON_GetArrayItem(tabs, i);
        if (tab_name && cJSON_IsString(tab_name)) {
            m_tab_names.push_back(tab_name->valuestring);
        }
    }
    
    // Create tabview object
    lv_obj_t* parent_obj = parent ? parent : lv_screen_active();
    m_lvgl_obj = lv_tabview_create(parent_obj);
    if (!m_lvgl_obj) {
        ESP_LOGE(TAG, "Failed to create tabview widget: %s", id.c_str());
        return false;
    }
    
    lv_obj_set_pos(m_lvgl_obj, x, y);
    lv_obj_set_size(m_lvgl_obj, w, h);
    
    // Add tabs and store references
    for (const auto& tab_name : m_tab_names) {
        lv_obj_t* tab = lv_tabview_add_tab(m_lvgl_obj, tab_name.c_str());
        m_tab_objects[tab_name] = tab;
        ESP_LOGI(TAG, "Added tab: %s", tab_name.c_str());
    }
    
    // Get initial active tab
    cJSON* active_tab = cJSON_GetObjectItem(properties, "active_tab");
    if (active_tab && cJSON_IsNumber(active_tab)) {
        m_active_tab = active_tab->valueint;
        if (m_active_tab < m_tab_names.size()) {
            lv_tabview_set_active(m_lvgl_obj, m_active_tab, LV_ANIM_OFF);
        }
    }
    
    // Apply color styling
    cJSON* bg_color = cJSON_GetObjectItem(properties, "bg_color");
    if (bg_color && cJSON_IsString(bg_color)) {
        uint32_t color = strtol(bg_color->valuestring + 1, NULL, 16); // Skip '#'
        lv_obj_set_style_bg_color(m_lvgl_obj, lv_color_hex(color), 0);
    }
    
    cJSON* tab_bg_color = cJSON_GetObjectItem(properties, "tab_bg_color");
    if (tab_bg_color && cJSON_IsString(tab_bg_color)) {
        uint32_t color = strtol(tab_bg_color->valuestring + 1, NULL, 16);
        lv_obj_t* tab_bar = lv_tabview_get_tab_bar(m_lvgl_obj);
        if (tab_bar) {
            lv_obj_set_style_bg_color(tab_bar, lv_color_hex(color), 0);
        }
    }
    
    cJSON* active_color = cJSON_GetObjectItem(properties, "active_tab_color");
    if (active_color && cJSON_IsString(active_color)) {
        uint32_t color = strtol(active_color->valuestring + 1, NULL, 16);
        lv_obj_t* tab_bar = lv_tabview_get_tab_bar(m_lvgl_obj);
        if (tab_bar) {
            // Style the active tab indicator
            lv_obj_set_style_bg_color(tab_bar, lv_color_hex(color), LV_PART_ITEMS | LV_STATE_CHECKED);
        }
    }
    
    cJSON* tab_text_color = cJSON_GetObjectItem(properties, "tab_text_color");
    if (tab_text_color && cJSON_IsString(tab_text_color)) {
        uint32_t color = strtol(tab_text_color->valuestring + 1, NULL, 16);
        lv_obj_t* tab_bar = lv_tabview_get_tab_bar(m_lvgl_obj);
        if (tab_bar) {
            lv_obj_set_style_text_color(tab_bar, lv_color_hex(color), LV_PART_ITEMS);
        }
    }
    
    // Setup MQTT
    cJSON* mqtt_topic = cJSON_GetObjectItem(properties, "mqtt_topic");
    if (mqtt_topic && cJSON_IsString(mqtt_topic)) {
        m_mqtt_topic = mqtt_topic->valuestring;
        
        cJSON* retained = cJSON_GetObjectItem(properties, "retained");
        if (retained && cJSON_IsBool(retained)) {
            m_retained = cJSON_IsTrue(retained);
        }
        
        // Subscribe for external updates
        MQTTManager::getInstance().subscribe(m_mqtt_topic, 0, 
            [this](const std::string& topic, const std::string& payload) {
                this->onMqttMessage(topic, payload);
            });
    }
    
    // Register event callback for tab changes
    lv_obj_add_event_cb(m_lvgl_obj, tab_changed_event_cb, LV_EVENT_VALUE_CHANGED, this);
    
    ESP_LOGI(TAG, "Created tabview widget: %s with %d tabs at (%d,%d) size (%dx%d)", 
             id.c_str(), m_tab_names.size(), x, y, w, h);
    
    return true;
}

void TabviewWidget::destroy() {
    if (m_lvgl_obj) {
        lv_obj_delete(m_lvgl_obj);
        m_lvgl_obj = nullptr;
        m_tab_objects.clear();
        ESP_LOGI(TAG, "Destroyed tabview widget: %s", m_id.c_str());
    }
}

void TabviewWidget::tab_changed_event_cb(lv_event_t* e) {
    TabviewWidget* widget = static_cast<TabviewWidget*>(lv_event_get_user_data(e));
    lv_obj_t* tabview = static_cast<lv_obj_t*>(lv_event_get_target(e));
    
    uint32_t tab_index = lv_tabview_get_tab_active(tabview);
    widget->onTabChanged(tab_index);
}

void TabviewWidget::onTabChanged(uint32_t tab_index) {
    if (m_updating_from_mqtt) {
        return; // Prevent feedback loop
    }
    
    m_active_tab = tab_index;
    
    // Publish tab change via MQTT
    if (!m_mqtt_topic.empty() && tab_index < m_tab_names.size()) {
        std::string payload = m_tab_names[tab_index];
        MQTTManager::getInstance().publish(m_mqtt_topic, payload, 0, m_retained);
        ESP_LOGI(TAG, "Tab changed to: %s (index %d)", payload.c_str(), tab_index);
    }
}

void TabviewWidget::onMqttMessage(const std::string& topic, const std::string& payload) {
    if (!m_lvgl_obj || !lv_obj_is_valid(m_lvgl_obj)) {
        return;
    }
    
    // Parse payload - can be tab name or index
    uint32_t new_tab_index = m_active_tab;
    
    // Try to find by name first
    bool found = false;
    for (size_t i = 0; i < m_tab_names.size(); i++) {
        if (m_tab_names[i] == payload) {
            new_tab_index = i;
            found = true;
            break;
        }
    }
    
    // If not found by name, try parsing as index
    if (!found) {
        char* endptr;
        long index = strtol(payload.c_str(), &endptr, 10);
        if (*endptr == '\0' && index >= 0 && index < static_cast<long>(m_tab_names.size())) {
            new_tab_index = static_cast<uint32_t>(index);
            found = true;
        }
    }
    
    if (!found) {
        ESP_LOGW(TAG, "Invalid tab identifier: %s", payload.c_str());
        return;
    }
    
    if (new_tab_index == m_active_tab) {
        return; // Already on this tab
    }
    
    // Update tab via LVGL async call
    auto update_fn = [](void* user_data) {
        TabviewWidget* widget = static_cast<TabviewWidget*>(user_data);
        if (widget->m_lvgl_obj && lv_obj_is_valid(widget->m_lvgl_obj)) {
            widget->m_updating_from_mqtt = true;
            lv_tabview_set_active(widget->m_lvgl_obj, widget->m_active_tab, LV_ANIM_ON);
            widget->m_updating_from_mqtt = false;
            ESP_LOGI(TAG, "Tabview '%s' changed to tab %d via MQTT", widget->m_id.c_str(), widget->m_active_tab);
        }
    };
    
    m_active_tab = new_tab_index;
    lv_async_call(update_fn, this);
}

lv_obj_t* TabviewWidget::getTabByName(const std::string& tab_name) {
    auto it = m_tab_objects.find(tab_name);
    if (it != m_tab_objects.end()) {
        return it->second;
    }
    return nullptr;
}
