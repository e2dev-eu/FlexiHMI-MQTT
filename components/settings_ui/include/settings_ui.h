#pragma once

#include "lvgl.h"
#include <string>
#include <vector>

// WiFi AP structure for scan results
struct WifiAP {
    std::string ssid;
    int rssi;
    bool requires_password;
};

class SettingsUI {
public:
    static SettingsUI& getInstance();
    
    // Initialize settings UI (creates gear icon in bottom right)
    void init(lv_obj_t* parent_screen);
    
    // Show/hide settings screen
    void show();
    void hide();
    
    // Check if settings screen is visible
    bool isVisible() const { return m_visible; }
    
    // Bring gear icon to foreground (call after creating widgets)
    void bringToFront();
    
    // Load settings from NVS
    bool loadSettings();
    
    // Save settings to NVS
    bool saveSettings();
    
    // Load network configuration from NVS
    bool loadNetworkConfig();
    
    // Save network configuration to NVS
    bool saveNetworkConfig();
    
    // Update WiFi scan results
    void updateWifiScanResults(const std::vector<WifiAP>& aps);
    
    // Update network status displays
    void updateEthStatus(bool connected, const std::string& ip);
    void updateWifiStatus(bool connected, const std::string& ssid, const std::string& ip);
    
    // Network manager callbacks
    void onLanStatusChanged(const std::string& status, const std::string& ip, const std::string& netmask, const std::string& gateway);
    void onWifiStatusChanged(const std::string& status, const std::string& ssid, const std::string& ip, const std::string& netmask, const std::string& gateway);
    
    // Getters for settings
    const std::string& getBrokerUri() const { return m_broker_uri; }
    const std::string& getUsername() const { return m_username; }
    const std::string& getPassword() const { return m_password; }
    const std::string& getClientId() const { return m_client_id; }
    const std::string& getConfigTopic() const { return m_config_topic; }
    
private:
    SettingsUI();
    ~SettingsUI();
    SettingsUI(const SettingsUI&) = delete;
    SettingsUI& operator=(const SettingsUI&) = delete;
    
    void createGearIcon(lv_obj_t* parent);
    void createSettingsScreen();
    void destroySettingsScreen();
    
    // Tab creation functions
    void createMqttTab(lv_obj_t* tab);
    void createLanTab(lv_obj_t* tab);
    void createWifiNetworkTab(lv_obj_t* tab);
    void createWifiRadioTab(lv_obj_t* tab);
    void createAboutTab(lv_obj_t* tab);
    
    // Callbacks
    static void gear_clicked_cb(lv_event_t* e);
    static void close_clicked_cb(lv_event_t* e);
    static void tab_changed_cb(lv_event_t* e);
    static void mqtt_save_clicked_cb(lv_event_t* e);
    static void textarea_focused_cb(lv_event_t* e);
    static void textarea_defocused_cb(lv_event_t* e);
    static void keyboard_ready_cb(lv_event_t* e);
    
    // LAN callbacks
    static void lan_dhcp_switch_cb(lv_event_t* e);
    static void lan_save_clicked_cb(lv_event_t* e);
    
    // WiFi callbacks
    static void wifi_scan_clicked_cb(lv_event_t* e);
    static void wifi_ap_clicked_cb(lv_event_t* e);
    static void wifi_connect_clicked_cb(lv_event_t* e);
    static void wifi_dhcp_switch_cb(lv_event_t* e);
    static void wifi_save_clicked_cb(lv_event_t* e);
    
    void createKeyboard();
    void destroyKeyboard();
    void performWifiScan();
    
    // Helper methods for network config
    void loadLanConfigToUI();
    void loadWifiConfigToUI();
    
    // UI objects
    lv_obj_t* m_gear_icon;
    lv_obj_t* m_settings_screen;
    lv_obj_t* m_tabview;
    lv_obj_t* m_keyboard;
    
    // MQTT tab objects
    lv_obj_t* m_broker_input;
    lv_obj_t* m_username_input;
    lv_obj_t* m_password_input;
    lv_obj_t* m_client_id_input;
    lv_obj_t* m_config_topic_input;
    
    // LAN tab objects
    lv_obj_t* m_lan_dhcp_switch;
    lv_obj_t* m_lan_ip_input;
    lv_obj_t* m_lan_netmask_input;
    lv_obj_t* m_lan_gateway_input;
    lv_obj_t* m_lan_status_label;
    lv_obj_t* m_lan_current_status_label;
    lv_obj_t* m_lan_current_ip_label;
    lv_obj_t* m_lan_current_netmask_label;
    lv_obj_t* m_lan_current_gateway_label;
    
    // WiFi tab objects
    lv_obj_t* m_wifi_list;
    lv_obj_t* m_wifi_ssid_input;
    lv_obj_t* m_wifi_password_input;
    lv_obj_t* m_wifi_status_label;
    lv_obj_t* m_wifi_dhcp_switch;
    lv_obj_t* m_wifi_ip_input;
    lv_obj_t* m_wifi_netmask_input;
    lv_obj_t* m_wifi_gateway_input;
    lv_obj_t* m_wifi_current_status_label;
    lv_obj_t* m_wifi_current_ssid_label;
    lv_obj_t* m_wifi_current_ip_label;
    lv_obj_t* m_wifi_current_netmask_label;
    lv_obj_t* m_wifi_current_gateway_label;
    
    // WiFi Radio tab status labels
    lv_obj_t* m_wifi_radio_conn_state_label;
    lv_obj_t* m_wifi_radio_ssid_label;
    lv_obj_t* m_wifi_radio_signal_label;
    lv_obj_t* m_wifi_radio_channel_label;
    
    std::string m_selected_ssid;
    
    bool m_visible;
    
    // Settings values
    std::string m_broker_uri;
    std::string m_username;
    std::string m_password;
    std::string m_client_id;
    std::string m_config_topic;
    
    // Network configuration
    struct NetworkConfig {
        bool lan_dhcp;
        std::string lan_ip;
        std::string lan_netmask;
        std::string lan_gateway;
        bool wifi_dhcp;
        std::string wifi_ip;
        std::string wifi_netmask;
        std::string wifi_gateway;
    } m_network_config;
};
