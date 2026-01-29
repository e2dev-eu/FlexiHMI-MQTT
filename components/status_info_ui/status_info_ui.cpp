#include "status_info_ui.h"
#include "esp_log.h"
#include <cstdio>

static const char* TAG = "StatusInfoUI";

// C wrapper functions implementation
extern "C" {
    void status_info_update_network(const char* ip, const char* mask, const char* gateway) {
        StatusInfoUI::getInstance().updateNetworkStatus(
            std::string(ip), std::string(mask), std::string(gateway));
    }
    
    void status_info_update_mqtt(bool connected, const char* broker) {
        StatusInfoUI::getInstance().updateMqttStatus(connected, std::string(broker));
    }
}

StatusInfoUI& StatusInfoUI::getInstance() {
    static StatusInfoUI instance;
    return instance;
}

StatusInfoUI::StatusInfoUI() 
    : m_info_icon(nullptr)
    , m_status_screen(nullptr)
    , m_ip_label(nullptr)
    , m_mask_label(nullptr)
    , m_gateway_label(nullptr)
    , m_mqtt_status_label(nullptr)
    , m_mqtt_broker_label(nullptr)
    , m_heap_label(nullptr)
    , m_min_heap_label(nullptr)
    , m_visible(false)
    , m_mqtt_connected(false)
    , m_free_heap(0)
    , m_min_free_heap(0) {
}

StatusInfoUI::~StatusInfoUI() {
}

void StatusInfoUI::init(lv_obj_t* parent_screen) {
    createInfoIcon(parent_screen);
}

void StatusInfoUI::createInfoIcon(lv_obj_t* parent) {
    // Create info button in top-right corner, next to gear icon
    m_info_icon = lv_button_create(parent);
    lv_obj_set_size(m_info_icon, 50, 50);
    lv_obj_align(m_info_icon, LV_ALIGN_TOP_RIGHT, -70, 10);  // 70px from right (60px for gear + 10px spacing)
    
    // Add info symbol
    lv_obj_t* label = lv_label_create(m_info_icon);
    lv_label_set_text(label, LV_SYMBOL_LIST);  // Using list icon, can also use LV_SYMBOL_HOME
    lv_obj_center(label);
    
    lv_obj_add_event_cb(m_info_icon, info_clicked_cb, LV_EVENT_CLICKED, this);
    
    ESP_LOGI(TAG, "Info icon created");
}

void StatusInfoUI::createStatusScreen() {
    // Create modal overlay
    m_status_screen = lv_obj_create(lv_screen_active());
    lv_obj_set_size(m_status_screen, LV_PCT(60), LV_PCT(70));
    lv_obj_center(m_status_screen);
    lv_obj_set_style_bg_color(m_status_screen, lv_color_hex(0x34495E), 0);
    lv_obj_set_style_border_color(m_status_screen, lv_color_hex(0x1ABC9C), 0);
    lv_obj_set_style_border_width(m_status_screen, 3, 0);
    
    // Title
    lv_obj_t* title = lv_label_create(m_status_screen);
    lv_label_set_text(title, LV_SYMBOL_LIST " System Status");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 15);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    
    int y_pos = 70;
    int line_height = 35;
    
    // Network section
    lv_obj_t* net_title = lv_label_create(m_status_screen);
    lv_label_set_text(net_title, "Network:");
    lv_obj_align(net_title, LV_ALIGN_TOP_LEFT, 20, y_pos);
    lv_obj_set_style_text_font(net_title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(net_title, lv_color_hex(0x3498DB), 0);
    y_pos += line_height;
    
    m_ip_label = lv_label_create(m_status_screen);
    lv_label_set_text(m_ip_label, "  IP: --");
    lv_obj_align(m_ip_label, LV_ALIGN_TOP_LEFT, 30, y_pos);
    y_pos += line_height;
    
    m_mask_label = lv_label_create(m_status_screen);
    lv_label_set_text(m_mask_label, "  Mask: --");
    lv_obj_align(m_mask_label, LV_ALIGN_TOP_LEFT, 30, y_pos);
    y_pos += line_height;
    
    m_gateway_label = lv_label_create(m_status_screen);
    lv_label_set_text(m_gateway_label, "  Gateway: --");
    lv_obj_align(m_gateway_label, LV_ALIGN_TOP_LEFT, 30, y_pos);
    y_pos += line_height + 10;
    
    // MQTT section
    lv_obj_t* mqtt_title = lv_label_create(m_status_screen);
    lv_label_set_text(mqtt_title, "MQTT:");
    lv_obj_align(mqtt_title, LV_ALIGN_TOP_LEFT, 20, y_pos);
    lv_obj_set_style_text_font(mqtt_title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(mqtt_title, lv_color_hex(0xE74C3C), 0);
    y_pos += line_height;
    
    m_mqtt_status_label = lv_label_create(m_status_screen);
    lv_label_set_text(m_mqtt_status_label, "  Status: Disconnected");
    lv_obj_align(m_mqtt_status_label, LV_ALIGN_TOP_LEFT, 30, y_pos);
    y_pos += line_height;
    
    m_mqtt_broker_label = lv_label_create(m_status_screen);
    lv_label_set_text(m_mqtt_broker_label, "  Broker: --");
    lv_obj_align(m_mqtt_broker_label, LV_ALIGN_TOP_LEFT, 30, y_pos);
    y_pos += line_height + 10;
    
    // System section
    lv_obj_t* sys_title = lv_label_create(m_status_screen);
    lv_label_set_text(sys_title, "Memory:");
    lv_obj_align(sys_title, LV_ALIGN_TOP_LEFT, 20, y_pos);
    lv_obj_set_style_text_font(sys_title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(sys_title, lv_color_hex(0x9B59B6), 0);
    y_pos += line_height;
    
    m_heap_label = lv_label_create(m_status_screen);
    lv_label_set_text(m_heap_label, "  Free Heap: -- KB");
    lv_obj_align(m_heap_label, LV_ALIGN_TOP_LEFT, 30, y_pos);
    y_pos += line_height;
    
    m_min_heap_label = lv_label_create(m_status_screen);
    lv_label_set_text(m_min_heap_label, "  Min Free: -- KB");
    lv_obj_align(m_min_heap_label, LV_ALIGN_TOP_LEFT, 30, y_pos);
    
    // Close button
    lv_obj_t* close_btn = lv_button_create(m_status_screen);
    lv_obj_set_size(close_btn, 120, 50);
    lv_obj_align(close_btn, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_add_event_cb(close_btn, close_clicked_cb, LV_EVENT_CLICKED, this);
    
    lv_obj_t* close_label = lv_label_create(close_btn);
    lv_label_set_text(close_label, "Close");
    lv_obj_center(close_label);
    
    // Update labels with current values
    if (!m_ip_address.empty()) {
        updateNetworkStatus(m_ip_address, m_netmask, m_gateway);
    }
    if (!m_mqtt_broker.empty()) {
        updateMqttStatus(m_mqtt_connected, m_mqtt_broker);
    }
    if (m_free_heap > 0) {
        updateSystemInfo(m_free_heap, m_min_free_heap);
    }
}

void StatusInfoUI::destroyStatusScreen() {
    if (m_status_screen) {
        lv_obj_delete(m_status_screen);
        m_status_screen = nullptr;
        m_ip_label = nullptr;
        m_mask_label = nullptr;
        m_gateway_label = nullptr;
        m_mqtt_status_label = nullptr;
        m_mqtt_broker_label = nullptr;
        m_heap_label = nullptr;
        m_min_heap_label = nullptr;
    }
}

void StatusInfoUI::show() {
    if (!m_visible) {
        createStatusScreen();
        m_visible = true;
    }
}

void StatusInfoUI::hide() {
    if (m_visible) {
        destroyStatusScreen();
        m_visible = false;
    }
}

void StatusInfoUI::updateNetworkStatus(const std::string& ip, const std::string& mask, const std::string& gateway) {
    m_ip_address = ip;
    m_netmask = mask;
    m_gateway = gateway;
    
    if (m_ip_label && lv_obj_is_valid(m_ip_label)) {
        char buf[128];
        snprintf(buf, sizeof(buf), "  IP: %s", ip.c_str());
        lv_label_set_text(m_ip_label, buf);
        
        snprintf(buf, sizeof(buf), "  Mask: %s", mask.c_str());
        lv_label_set_text(m_mask_label, buf);
        
        snprintf(buf, sizeof(buf), "  Gateway: %s", gateway.c_str());
        lv_label_set_text(m_gateway_label, buf);
    }
}

void StatusInfoUI::updateMqttStatus(bool connected, const std::string& broker) {
    m_mqtt_connected = connected;
    m_mqtt_broker = broker;
    
    if (m_mqtt_status_label && lv_obj_is_valid(m_mqtt_status_label)) {
        lv_label_set_text(m_mqtt_status_label, 
            connected ? "  Status: Connected" : "  Status: Disconnected");
        
        char buf[256];
        snprintf(buf, sizeof(buf), "  Broker: %s", broker.c_str());
        lv_label_set_text(m_mqtt_broker_label, buf);
    }
}

void StatusInfoUI::updateSystemInfo(uint32_t free_heap, uint32_t min_free_heap) {
    m_free_heap = free_heap;
    m_min_free_heap = min_free_heap;
    
    if (m_heap_label && lv_obj_is_valid(m_heap_label)) {
        char buf[128];
        snprintf(buf, sizeof(buf), "  Free Heap: %lu KB", free_heap / 1024);
        lv_label_set_text(m_heap_label, buf);
        
        snprintf(buf, sizeof(buf), "  Min Free: %lu KB", min_free_heap / 1024);
        lv_label_set_text(m_min_heap_label, buf);
    }
}

void StatusInfoUI::info_clicked_cb(lv_event_t* e) {
    StatusInfoUI* ui = static_cast<StatusInfoUI*>(lv_event_get_user_data(e));
    if (ui->m_visible) {
        ui->hide();
    } else {
        ui->show();
    }
}

void StatusInfoUI::close_clicked_cb(lv_event_t* e) {
    StatusInfoUI* ui = static_cast<StatusInfoUI*>(lv_event_get_user_data(e));
    ui->hide();
}
