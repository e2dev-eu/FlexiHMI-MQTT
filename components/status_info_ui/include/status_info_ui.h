#pragma once

#ifdef __cplusplus
#include "lvgl.h"
#include <string>
#endif

#ifdef __cplusplus
extern "C" {
#endif

// C wrapper functions
void status_info_update_network(const char* ip, const char* mask, const char* gateway);
void status_info_update_mqtt(bool connected, const char* broker);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

class StatusInfoUI {
public:
    static StatusInfoUI& getInstance();
    
    // Initialize status info UI (creates info icon)
    void init(lv_obj_t* parent_screen);
    
    // Show/hide status info screen
    void show();
    void hide();
    
    // Check if status screen is visible
    bool isVisible() const { return m_visible; }
    
    // Bring info icon to foreground (call after creating widgets)
    void bringToFront();
    
    // Update status information
    void updateNetworkStatus(const std::string& ip, const std::string& mask, const std::string& gateway);
    void updateMqttStatus(bool connected, const std::string& broker);
    void updateSystemInfo(uint32_t free_heap, uint32_t min_free_heap);
    
private:
    StatusInfoUI();
    ~StatusInfoUI();
    StatusInfoUI(const StatusInfoUI&) = delete;
    StatusInfoUI& operator=(const StatusInfoUI&) = delete;
    
    void createInfoIcon(lv_obj_t* parent);
    void createStatusScreen();
    void destroyStatusScreen();
    
    static void info_clicked_cb(lv_event_t* e);
    static void close_clicked_cb(lv_event_t* e);
    
    lv_obj_t* m_info_icon;
    lv_obj_t* m_status_screen;
    lv_obj_t* m_ip_label;
    lv_obj_t* m_mask_label;
    lv_obj_t* m_gateway_label;
    lv_obj_t* m_mqtt_status_label;
    lv_obj_t* m_mqtt_broker_label;
    lv_obj_t* m_heap_label;
    lv_obj_t* m_min_heap_label;
    
    bool m_visible;
    
    // Status values
    std::string m_ip_address;
    std::string m_netmask;
    std::string m_gateway;
    std::string m_mqtt_broker;
    bool m_mqtt_connected;
    uint32_t m_free_heap;
    uint32_t m_min_free_heap;
};

#endif  // __cplusplus
