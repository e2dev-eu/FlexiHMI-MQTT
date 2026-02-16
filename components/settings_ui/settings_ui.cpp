#include "settings_ui.h"
#include "mqtt_manager.h"
#include "wireless_manager.h"
#include "lan_manager.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_app_desc.h"
#include "esp_idf_version.h"
#include <tuple>

static const char* TAG = "SettingsUI";

// C wrapper for bringing icon to front
extern "C" void settings_ui_bring_to_front() {
    SettingsUI::getInstance().bringToFront();
}

static const char* NVS_NAMESPACE = "mqtt_settings";
static const char* NVS_KEY_BROKER = "broker_uri";
static const char* NVS_KEY_USERNAME = "username";
static const char* NVS_KEY_PASSWORD = "password";
static const char* NVS_KEY_CLIENT_ID = "client_id";
static const char* NVS_KEY_CONFIG_TOPIC = "config_topic";

SettingsUI& SettingsUI::getInstance() {
    static SettingsUI instance;
    return instance;
}

SettingsUI::SettingsUI() 
    : m_settings_layer(nullptr)
    , m_gear_icon(nullptr)
    , m_settings_screen(nullptr)
    , m_tabview(nullptr)
    , m_keyboard(nullptr)
    , m_broker_input(nullptr)
    , m_username_input(nullptr)
    , m_password_input(nullptr)
    , m_client_id_input(nullptr)
    , m_config_topic_input(nullptr)
    , m_lan_dhcp_switch(nullptr)
    , m_lan_ip_input(nullptr)
    , m_lan_netmask_input(nullptr)
    , m_lan_gateway_input(nullptr)
    , m_lan_current_status_label(nullptr)
    , m_lan_current_ip_label(nullptr)
    , m_lan_current_netmask_label(nullptr)
    , m_lan_current_gateway_label(nullptr)
    , m_wifi_list(nullptr)
    , m_wifi_ssid_input(nullptr)
    , m_wifi_password_input(nullptr)
    , m_wifi_dhcp_switch(nullptr)
    , m_wifi_ip_input(nullptr)
    , m_wifi_netmask_input(nullptr)
    , m_wifi_gateway_input(nullptr)
    , m_wifi_current_status_label(nullptr)
    , m_wifi_current_ssid_label(nullptr)
    , m_wifi_current_ip_label(nullptr)
    , m_wifi_current_netmask_label(nullptr)
    , m_wifi_current_gateway_label(nullptr)
    , m_wifi_radio_conn_state_label(nullptr)
    , m_wifi_radio_ssid_label(nullptr)
    , m_wifi_radio_signal_label(nullptr)
    , m_wifi_radio_channel_label(nullptr)
    , m_visible(false)
    , m_broker_uri("mqtt://")
    , m_config_topic("hmi/config") {
    // Initialize network config with defaults
    m_network_config.lan_dhcp = true;
    m_network_config.lan_netmask = "255.255.255.0";
    m_network_config.wifi_dhcp = true;
    m_network_config.wifi_netmask = "255.255.255.0";
}

SettingsUI::~SettingsUI() {
}

void SettingsUI::init(lv_obj_t* parent_screen) {
    // Create a persistent layer for settings UI that stays on top
    m_settings_layer = lv_obj_create(parent_screen);
    lv_obj_set_size(m_settings_layer, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(m_settings_layer, LV_OPA_TRANSP, 0);  // Transparent background
    lv_obj_set_style_border_width(m_settings_layer, 0, 0);  // No border
    lv_obj_clear_flag(m_settings_layer, LV_OBJ_FLAG_SCROLLABLE);  // Not scrollable
    lv_obj_clear_flag(m_settings_layer, LV_OBJ_FLAG_CLICKABLE);  // Don't block clicks - let events pass through
    lv_obj_add_flag(m_settings_layer, LV_OBJ_FLAG_FLOATING);  // Always on top
    lv_obj_add_flag(m_settings_layer, LV_OBJ_FLAG_EVENT_BUBBLE);  // Pass events to widgets below
    lv_obj_move_foreground(m_settings_layer);
    
    createGearIcon(m_settings_layer);
    loadSettings();
    // Note: Network config is loaded by managers on their init()
    // We'll load from managers when creating the UI tabs
    
    // Register callbacks with network managers to receive status updates
    LanManager& lan = LanManager::getInstance();
    lan.setStatusCallback([this, &lan](EthConnectionStatus status, const std::string& info) {
        std::string status_str;
        switch (status) {
            case EthConnectionStatus::DISCONNECTED:
                status_str = "Disconnected";
                break;
            case EthConnectionStatus::LINK_DOWN:
                status_str = "Cable unplugged";
                break;
            case EthConnectionStatus::LINK_UP:
                status_str = "Link up";
                break;
            case EthConnectionStatus::CONNECTED:
                status_str = "Connected";
                break;
        }
        
        // Capture values to pass to LVGL task
        std::string captured_status = status_str;
        std::string captured_ip, captured_netmask, captured_gateway;
        
        // Get current IP info if connected
        if (status == EthConnectionStatus::CONNECTED) {
            captured_ip = lan.getIpAddress();
            captured_netmask = lan.getNetmask();
            captured_gateway = lan.getGateway();
        }
        
        // Use lv_async_call to safely update UI from LVGL task
        lv_async_call([](void* user_data) {
            auto* data = static_cast<std::tuple<SettingsUI*, std::string, std::string, std::string, std::string>*>(user_data);
            auto& [ui, status, ip, netmask, gateway] = *data;
            ui->onLanStatusChanged(status, ip, netmask, gateway);
            delete data;
        }, new std::tuple<SettingsUI*, std::string, std::string, std::string, std::string>(this, captured_status, captured_ip, captured_netmask, captured_gateway));
    });
    
    lan.setIpCallback([this](const std::string& ip, const std::string& netmask, const std::string& gateway) {
        // Capture values to pass to LVGL task
        std::string captured_ip = ip;
        std::string captured_netmask = netmask;
        std::string captured_gateway = gateway;
        
        // Use lv_async_call to safely update UI from LVGL task
        lv_async_call([](void* user_data) {
            auto* data = static_cast<std::tuple<SettingsUI*, std::string, std::string, std::string, std::string>*>(user_data);
            auto& [ui, status, ip, netmask, gateway] = *data;
            ui->onLanStatusChanged(status, ip, netmask, gateway);
            delete data;
        }, new std::tuple<SettingsUI*, std::string, std::string, std::string, std::string>(this, std::string("Connected"), captured_ip, captured_netmask, captured_gateway));
    });
    
    // Register WiFi callbacks
    WirelessManager& wifi = WirelessManager::getInstance();
    wifi.setStatusCallback([this, &wifi](WifiConnectionStatus status, const std::string& info) {
        std::string status_str;
        switch (status) {
            case WifiConnectionStatus::DISCONNECTED:
                status_str = "Disconnected";
                break;
            case WifiConnectionStatus::CONNECTING:
                status_str = "Connecting...";
                break;
            case WifiConnectionStatus::CONNECTED:
                status_str = "Connected";
                break;
            case WifiConnectionStatus::FAILED:
                status_str = "Connection failed";
                break;
        }
        
        // Capture values to pass to LVGL task
        std::string captured_status = status_str;
        std::string captured_ssid, captured_ip, captured_netmask, captured_gateway;
        
        // Get current info if connected
        if (status == WifiConnectionStatus::CONNECTED) {
            captured_ssid = wifi.getCurrentSsid();
            captured_ip = wifi.getIpAddress();
            captured_netmask = wifi.getNetmask();
            captured_gateway = wifi.getGateway();
        }
        
        // Use lv_async_call to safely update UI from LVGL task
        lv_async_call([](void* user_data) {
            auto* data = static_cast<std::tuple<SettingsUI*, std::string, std::string, std::string, std::string, std::string>*>(user_data);
            auto& [ui, status, ssid, ip, netmask, gateway] = *data;
            ui->onWifiStatusChanged(status, ssid, ip, netmask, gateway);
            delete data;
        }, new std::tuple<SettingsUI*, std::string, std::string, std::string, std::string, std::string>(this, captured_status, captured_ssid, captured_ip, captured_netmask, captured_gateway));
    });
    
    wifi.setIpCallback([this](const std::string& ip, const std::string& netmask, const std::string& gateway) {
        // Capture values to pass to LVGL task
        WirelessManager& wifi = WirelessManager::getInstance();
        std::string captured_ssid = wifi.getCurrentSsid();
        std::string captured_ip = ip;
        std::string captured_netmask = netmask;
        std::string captured_gateway = gateway;
        
        // Use lv_async_call to safely update UI from LVGL task
        lv_async_call([](void* user_data) {
            auto* data = static_cast<std::tuple<SettingsUI*, std::string, std::string, std::string, std::string, std::string>*>(user_data);
            auto& [ui, status, ssid, ip, netmask, gateway] = *data;
            ui->onWifiStatusChanged(status, ssid, ip, netmask, gateway);
            delete data;
        }, new std::tuple<SettingsUI*, std::string, std::string, std::string, std::string, std::string>(this, std::string("Connected"), captured_ssid, captured_ip, captured_netmask, captured_gateway));
    });
    
    ESP_LOGI(TAG, "Network manager callbacks registered");
    
    // Create settings screen (hidden initially)
    createSettingsScreen();
    lv_obj_add_flag(m_settings_screen, LV_OBJ_FLAG_HIDDEN);
    ESP_LOGI(TAG, "Settings screen created (hidden)");
}

void SettingsUI::createGearIcon(lv_obj_t* parent) {
    // Create gear button in bottom-right corner
    m_gear_icon = lv_button_create(parent);
    lv_obj_set_size(m_gear_icon, 60, 60);
    lv_obj_align(m_gear_icon, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
    
    // Add gear symbol
    lv_obj_t* label = lv_label_create(m_gear_icon);
    lv_label_set_text(label, LV_SYMBOL_SETTINGS);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_32, 0);
    lv_obj_center(label);
    
    lv_obj_add_event_cb(m_gear_icon, gear_clicked_cb, LV_EVENT_CLICKED, this);
    
    ESP_LOGI(TAG, "Gear icon created");
}

void SettingsUI::createSettingsScreen() {
    // Create tabview on settings layer
    m_tabview = lv_tabview_create(m_settings_layer);
    lv_obj_set_size(m_tabview, LV_PCT(100), LV_PCT(100));
    lv_obj_align(m_tabview, LV_ALIGN_CENTER, 0, 0);
    
    // Keep on top - move to foreground
    lv_obj_move_foreground(m_tabview);
    lv_obj_add_flag(m_tabview, LV_OBJ_FLAG_FLOATING);
    
    // Create tabs
    lv_obj_t* mqtt_tab = lv_tabview_add_tab(m_tabview, "MQTT");
    lv_obj_t* lan_tab = lv_tabview_add_tab(m_tabview, "LAN");
    lv_obj_t* wifi_net_tab = lv_tabview_add_tab(m_tabview, "WiFi Net");
    lv_obj_t* wifi_radio_tab = lv_tabview_add_tab(m_tabview, "WiFi Radio");
    lv_obj_t* about_tab = lv_tabview_add_tab(m_tabview, "About");
    lv_obj_t* close_tab = lv_tabview_add_tab(m_tabview, "Close");
    (void)close_tab;  // Intentionally empty tab
    
    // Populate tabs
    createMqttTab(mqtt_tab);
    createLanTab(lan_tab);
    createWifiNetworkTab(wifi_net_tab);
    createWifiRadioTab(wifi_radio_tab);
    createAboutTab(about_tab);

    // Close tab content (explicit button)
    lv_obj_t* close_container = lv_obj_create(close_tab);
    lv_obj_set_size(close_container, LV_PCT(100), LV_PCT(100));
    lv_obj_clear_flag(close_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(close_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(close_container, 0, 0);

    lv_obj_t* close_btn = lv_button_create(close_container);
    lv_obj_set_size(close_btn, 220, 60);
    lv_obj_center(close_btn);
    lv_obj_add_event_cb(close_btn, close_clicked_cb, LV_EVENT_CLICKED, this);

    lv_obj_t* close_label = lv_label_create(close_btn);
    lv_label_set_text(close_label, "Close Settings");
    lv_obj_center(close_label);
    
    // Add event callback to tabview for tab changes
    lv_obj_add_event_cb(m_tabview, tab_changed_cb, LV_EVENT_VALUE_CHANGED, this);
    lv_obj_t* tab_btns = lv_tabview_get_tab_btns(m_tabview);
    lv_obj_add_event_cb(tab_btns, tab_changed_cb, LV_EVENT_VALUE_CHANGED, this);
    lv_obj_add_event_cb(tab_btns, tab_changed_cb, LV_EVENT_CLICKED, this);
    
    // Store settings_screen as tabview for destroy operations
    m_settings_screen = m_tabview;
    
    // Create keyboard
    createKeyboard();
    
    ESP_LOGI(TAG, "Settings screen with tabview created");
}

void SettingsUI::destroySettingsScreen() {
    if (m_settings_screen) {
        destroyKeyboard();
        lv_obj_delete(m_settings_screen);
        m_settings_screen = nullptr;
        m_broker_input = nullptr;
        m_username_input = nullptr;
        m_password_input = nullptr;
        m_client_id_input = nullptr;
        m_config_topic_input = nullptr;
        m_mqtt_status_label = nullptr;
        m_mqtt_broker_label = nullptr;
        m_mqtt_messages_received_label = nullptr;
        m_mqtt_messages_sent_label = nullptr;
        m_lan_dhcp_switch = nullptr;
        m_lan_ip_input = nullptr;
        m_lan_netmask_input = nullptr;
        m_lan_gateway_input = nullptr;
        m_wifi_list = nullptr;
        m_wifi_ssid_input = nullptr;
        m_wifi_password_input = nullptr;
        m_wifi_dhcp_switch = nullptr;
        m_wifi_ip_input = nullptr;
        m_wifi_netmask_input = nullptr;
        m_wifi_gateway_input = nullptr;
        ESP_LOGI(TAG, "Settings screen destroyed");
    }
}

void SettingsUI::createMqttTab(lv_obj_t* tab) {
    int field_height = 40;
    int input_width = 280;
    
    // === LEFT CONTAINER (Configuration) ===
    lv_obj_t* left_container = lv_obj_create(tab);
    lv_obj_set_size(left_container, LV_PCT(48), LV_PCT(95));
    lv_obj_set_pos(left_container, 10, 10);
    lv_obj_set_style_bg_color(left_container, lv_color_hex(0x2a2a2a), 0);
    lv_obj_set_style_border_color(left_container, lv_color_hex(0x444444), 0);
    lv_obj_set_style_border_width(left_container, 1, 0);
    lv_obj_set_style_pad_all(left_container, 15, 0);
    lv_obj_set_scrollbar_mode(left_container, LV_SCROLLBAR_MODE_AUTO);
    
    int y_pos = 10;
    
    // Configuration header
    lv_obj_t* config_header = lv_label_create(left_container);
    lv_label_set_text(config_header, "MQTT Configuration");
    lv_obj_set_pos(config_header, 0, y_pos);
    lv_obj_set_style_text_font(config_header, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(config_header, lv_color_hex(0x00BFFF), 0);
    
    y_pos += 35;
    
    // Broker URI
    lv_obj_t* broker_label = lv_label_create(left_container);
    lv_label_set_text(broker_label, "Broker URI:");
    lv_obj_set_pos(broker_label, 0, y_pos);
    
    y_pos += 22;
    
    m_broker_input = lv_textarea_create(left_container);
    lv_obj_set_size(m_broker_input, input_width, field_height);
    lv_obj_set_pos(m_broker_input, 0, y_pos);
    lv_textarea_set_one_line(m_broker_input, true);
    lv_textarea_set_text(m_broker_input, m_broker_uri.c_str());
    lv_obj_add_event_cb(m_broker_input, textarea_focused_cb, LV_EVENT_FOCUSED, this);
    lv_obj_add_event_cb(m_broker_input, textarea_defocused_cb, LV_EVENT_DEFOCUSED, this);
    
    y_pos += field_height + 15;
    
    // Username
    lv_obj_t* username_label = lv_label_create(left_container);
    lv_label_set_text(username_label, "Username:");
    lv_obj_set_pos(username_label, 0, y_pos);
    
    y_pos += 22;
    
    m_username_input = lv_textarea_create(left_container);
    lv_obj_set_size(m_username_input, input_width, field_height);
    lv_obj_set_pos(m_username_input, 0, y_pos);
    lv_textarea_set_one_line(m_username_input, true);
    lv_textarea_set_text(m_username_input, m_username.c_str());
    lv_obj_add_event_cb(m_username_input, textarea_focused_cb, LV_EVENT_FOCUSED, this);
    lv_obj_add_event_cb(m_username_input, textarea_defocused_cb, LV_EVENT_DEFOCUSED, this);
    
    y_pos += field_height + 15;
    
    // Password
    lv_obj_t* password_label = lv_label_create(left_container);
    lv_label_set_text(password_label, "Password:");
    lv_obj_set_pos(password_label, 0, y_pos);
    
    y_pos += 22;
    
    m_password_input = lv_textarea_create(left_container);
    lv_obj_set_size(m_password_input, input_width, field_height);
    lv_obj_set_pos(m_password_input, 0, y_pos);
    lv_textarea_set_one_line(m_password_input, true);
    lv_textarea_set_password_mode(m_password_input, true);
    lv_textarea_set_text(m_password_input, m_password.c_str());
    lv_obj_add_event_cb(m_password_input, textarea_focused_cb, LV_EVENT_FOCUSED, this);
    lv_obj_add_event_cb(m_password_input, textarea_defocused_cb, LV_EVENT_DEFOCUSED, this);
    
    y_pos += field_height + 15;
    
    // Client ID
    lv_obj_t* client_id_label = lv_label_create(left_container);
    lv_label_set_text(client_id_label, "Client ID:");
    lv_obj_set_pos(client_id_label, 0, y_pos);
    
    y_pos += 22;
    
    m_client_id_input = lv_textarea_create(left_container);
    lv_obj_set_size(m_client_id_input, input_width, field_height);
    lv_obj_set_pos(m_client_id_input, 0, y_pos);
    lv_textarea_set_one_line(m_client_id_input, true);
    lv_textarea_set_text(m_client_id_input, m_client_id.c_str());
    lv_obj_add_event_cb(m_client_id_input, textarea_focused_cb, LV_EVENT_FOCUSED, this);
    lv_obj_add_event_cb(m_client_id_input, textarea_defocused_cb, LV_EVENT_DEFOCUSED, this);
    
    y_pos += field_height + 15;
    
    // Config Topic
    lv_obj_t* topic_label = lv_label_create(left_container);
    lv_label_set_text(topic_label, "Config Topic:");
    lv_obj_set_pos(topic_label, 0, y_pos);
    
    y_pos += 22;
    
    m_config_topic_input = lv_textarea_create(left_container);
    lv_obj_set_size(m_config_topic_input, input_width, field_height);
    lv_obj_set_pos(m_config_topic_input, 0, y_pos);
    lv_textarea_set_one_line(m_config_topic_input, true);
    lv_textarea_set_text(m_config_topic_input, m_config_topic.c_str());
    lv_obj_add_event_cb(m_config_topic_input, textarea_focused_cb, LV_EVENT_FOCUSED, this);
    lv_obj_add_event_cb(m_config_topic_input, textarea_defocused_cb, LV_EVENT_DEFOCUSED, this);
    
    y_pos += field_height + 25;
    
    // Save & Apply button
    lv_obj_t* save_btn = lv_button_create(left_container);
    lv_obj_set_size(save_btn, input_width, 45);
    lv_obj_set_pos(save_btn, 0, y_pos);
    lv_obj_add_event_cb(save_btn, mqtt_save_clicked_cb, LV_EVENT_CLICKED, this);
    
    lv_obj_t* save_label = lv_label_create(save_btn);
    lv_label_set_text(save_label, "Save & Apply");
    lv_obj_center(save_label);
    
    // === RIGHT CONTAINER (Status) ===
    lv_obj_t* right_container = lv_obj_create(tab);
    lv_obj_set_size(right_container, LV_PCT(48), LV_PCT(95));
    lv_obj_set_pos(right_container, LV_PCT(52), 10);
    lv_obj_set_style_bg_color(right_container, lv_color_hex(0x2a2a2a), 0);
    lv_obj_set_style_border_color(right_container, lv_color_hex(0x444444), 0);
    lv_obj_set_style_border_width(right_container, 1, 0);
    lv_obj_set_style_pad_all(right_container, 15, 0);
    lv_obj_clear_flag(right_container, LV_OBJ_FLAG_SCROLLABLE);
    
    y_pos = 10;
    
    // Status header
    lv_obj_t* status_header = lv_label_create(right_container);
    lv_label_set_text(status_header, "Current Status");
    lv_obj_set_pos(status_header, 0, y_pos);
    lv_obj_set_style_text_font(status_header, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(status_header, lv_color_hex(0x00BFFF), 0);
    
    y_pos += 40;
    
    // Connection status
    m_mqtt_status_label = lv_label_create(right_container);
    lv_label_set_text(m_mqtt_status_label, "Status: Disconnected");
    lv_obj_set_pos(m_mqtt_status_label, 0, y_pos);
    lv_obj_set_style_text_color(m_mqtt_status_label, lv_color_hex(0xFF6666), 0);
    
    y_pos += 30;
    
    // Broker
    m_mqtt_broker_label = lv_label_create(right_container);
    lv_label_set_text(m_mqtt_broker_label, "Broker: ---");
    lv_obj_set_pos(m_mqtt_broker_label, 0, y_pos);
    lv_obj_set_style_text_color(m_mqtt_broker_label, lv_color_hex(0xCCCCCC), 0);
    
    y_pos += 40;
    
    // Statistics header
    lv_obj_t* stats_header = lv_label_create(right_container);
    lv_label_set_text(stats_header, "Statistics");
    lv_obj_set_pos(stats_header, 0, y_pos);
    lv_obj_set_style_text_font(stats_header, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(stats_header, lv_color_hex(0x00BFFF), 0);
    
    y_pos += 30;
    
    // Messages received
    m_mqtt_messages_received_label = lv_label_create(right_container);
    lv_label_set_text(m_mqtt_messages_received_label, "Received: 0");
    lv_obj_set_pos(m_mqtt_messages_received_label, 0, y_pos);
    lv_obj_set_style_text_color(m_mqtt_messages_received_label, lv_color_hex(0xCCCCCC), 0);
    
    y_pos += 25;
    
    // Messages sent
    m_mqtt_messages_sent_label = lv_label_create(right_container);
    lv_label_set_text(m_mqtt_messages_sent_label, "Sent: 0");
    lv_obj_set_pos(m_mqtt_messages_sent_label, 0, y_pos);
    lv_obj_set_style_text_color(m_mqtt_messages_sent_label, lv_color_hex(0xCCCCCC), 0);
    
    // Update status from MQTTManager
    MQTTManager& mqtt = MQTTManager::getInstance();
    onMqttStatusChanged(mqtt.isConnected(), mqtt.getMessagesReceived(), mqtt.getMessagesSent());
    
    ESP_LOGI(TAG, "MQTT tab created");
}

void SettingsUI::createLanTab(lv_obj_t* tab) {
    int field_height = 40;
    int input_width = 280;
    
    // === LEFT CONTAINER (Configuration) ===
    lv_obj_t* left_container = lv_obj_create(tab);
    lv_obj_set_size(left_container, LV_PCT(48), LV_PCT(95));
    lv_obj_set_pos(left_container, 10, 10);
    lv_obj_set_style_bg_color(left_container, lv_color_hex(0x2a2a2a), 0);
    lv_obj_set_style_border_color(left_container, lv_color_hex(0x444444), 0);
    lv_obj_set_style_border_width(left_container, 1, 0);
    lv_obj_set_style_pad_all(left_container, 15, 0);
    lv_obj_clear_flag(left_container, LV_OBJ_FLAG_SCROLLABLE);
    
    int y_pos = 10;
    
    // Configuration header
    lv_obj_t* config_header = lv_label_create(left_container);
    lv_label_set_text(config_header, "Configuration");
    lv_obj_set_pos(config_header, 0, y_pos);
    lv_obj_set_style_text_font(config_header, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(config_header, lv_color_hex(0x00BFFF), 0);
    
    y_pos += 35;
    
    // DHCP Switch
    lv_obj_t* dhcp_label = lv_label_create(left_container);
    lv_label_set_text(dhcp_label, "DHCP:");
    lv_obj_set_pos(dhcp_label, 0, y_pos + 5);
    
    m_lan_dhcp_switch = lv_switch_create(left_container);
    lv_obj_set_pos(m_lan_dhcp_switch, 100, y_pos);
    lv_obj_add_state(m_lan_dhcp_switch, LV_STATE_CHECKED);  // Default to DHCP
    lv_obj_add_event_cb(m_lan_dhcp_switch, lan_dhcp_switch_cb, LV_EVENT_VALUE_CHANGED, this);
    
    y_pos += 50;
    
    // IP Address
    lv_obj_t* ip_label = lv_label_create(left_container);
    lv_label_set_text(ip_label, "IP Address:");
    lv_obj_set_pos(ip_label, 0, y_pos);
    
    y_pos += 22;
    
    m_lan_ip_input = lv_textarea_create(left_container);
    lv_obj_set_size(m_lan_ip_input, input_width, field_height);
    lv_obj_set_pos(m_lan_ip_input, 0, y_pos);
    lv_textarea_set_one_line(m_lan_ip_input, true);
    lv_textarea_set_text(m_lan_ip_input, "");
    lv_textarea_set_accepted_chars(m_lan_ip_input, "0123456789.");
    lv_obj_add_event_cb(m_lan_ip_input, textarea_focused_cb, LV_EVENT_FOCUSED, this);
    lv_obj_add_event_cb(m_lan_ip_input, textarea_defocused_cb, LV_EVENT_DEFOCUSED, this);
    lv_obj_add_state(m_lan_ip_input, LV_STATE_DISABLED);  // Disabled by default (DHCP on)
    
    y_pos += field_height + 15;
    
    // Netmask
    lv_obj_t* netmask_label = lv_label_create(left_container);
    lv_label_set_text(netmask_label, "Netmask:");
    lv_obj_set_pos(netmask_label, 0, y_pos);
    
    y_pos += 22;
    
    m_lan_netmask_input = lv_textarea_create(left_container);
    lv_obj_set_size(m_lan_netmask_input, input_width, field_height);
    lv_obj_set_pos(m_lan_netmask_input, 0, y_pos);
    lv_textarea_set_one_line(m_lan_netmask_input, true);
    lv_textarea_set_text(m_lan_netmask_input, "255.255.255.0");
    lv_textarea_set_accepted_chars(m_lan_netmask_input, "0123456789.");
    lv_obj_add_event_cb(m_lan_netmask_input, textarea_focused_cb, LV_EVENT_FOCUSED, this);
    lv_obj_add_event_cb(m_lan_netmask_input, textarea_defocused_cb, LV_EVENT_DEFOCUSED, this);
    lv_obj_add_state(m_lan_netmask_input, LV_STATE_DISABLED);
    
    y_pos += field_height + 15;
    
    // Gateway
    lv_obj_t* gateway_label = lv_label_create(left_container);
    lv_label_set_text(gateway_label, "Gateway:");
    lv_obj_set_pos(gateway_label, 0, y_pos);
    
    y_pos += 22;
    
    m_lan_gateway_input = lv_textarea_create(left_container);
    lv_obj_set_size(m_lan_gateway_input, input_width, field_height);
    lv_obj_set_pos(m_lan_gateway_input, 0, y_pos);
    lv_textarea_set_one_line(m_lan_gateway_input, true);
    lv_textarea_set_text(m_lan_gateway_input, "");
    lv_textarea_set_accepted_chars(m_lan_gateway_input, "0123456789.");
    lv_obj_add_event_cb(m_lan_gateway_input, textarea_focused_cb, LV_EVENT_FOCUSED, this);
    lv_obj_add_event_cb(m_lan_gateway_input, textarea_defocused_cb, LV_EVENT_DEFOCUSED, this);
    lv_obj_add_state(m_lan_gateway_input, LV_STATE_DISABLED);
    
    y_pos += field_height + 25;
    
    // Apply button (full width of input fields)
    lv_obj_t* apply_btn = lv_button_create(left_container);
    lv_obj_set_size(apply_btn, input_width, 45);
    lv_obj_set_pos(apply_btn, 0, y_pos);
    lv_obj_add_event_cb(apply_btn, lan_save_clicked_cb, LV_EVENT_CLICKED, this);
    
    lv_obj_t* apply_label = lv_label_create(apply_btn);
    lv_label_set_text(apply_label, "Apply");
    lv_obj_center(apply_label);
    
    // === RIGHT CONTAINER (Status) ===
    lv_obj_t* right_container = lv_obj_create(tab);
    lv_obj_set_size(right_container, LV_PCT(48), LV_PCT(95));
    lv_obj_set_pos(right_container, LV_PCT(52), 10);
    lv_obj_set_style_bg_color(right_container, lv_color_hex(0x2a2a2a), 0);
    lv_obj_set_style_border_color(right_container, lv_color_hex(0x444444), 0);
    lv_obj_set_style_border_width(right_container, 1, 0);
    lv_obj_set_style_pad_all(right_container, 15, 0);
    lv_obj_clear_flag(right_container, LV_OBJ_FLAG_SCROLLABLE);
    
    y_pos = 10;
    
    // Status header
    lv_obj_t* status_header = lv_label_create(right_container);
    lv_label_set_text(status_header, "Current Status");
    lv_obj_set_pos(status_header, 0, y_pos);
    lv_obj_set_style_text_font(status_header, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(status_header, lv_color_hex(0x00BFFF), 0);
    
    y_pos += 40;
    
    // Current connection status
    m_lan_current_status_label = lv_label_create(right_container);
    lv_label_set_text(m_lan_current_status_label, "Status: Checking...");
    lv_obj_set_pos(m_lan_current_status_label, 0, y_pos);
    lv_obj_set_style_text_color(m_lan_current_status_label, lv_color_hex(0xFFFFFF), 0);
    
    y_pos += 30;
    
    // Current IP address
    m_lan_current_ip_label = lv_label_create(right_container);
    lv_label_set_text(m_lan_current_ip_label, "IP: ---");
    lv_obj_set_pos(m_lan_current_ip_label, 0, y_pos);
    lv_obj_set_style_text_color(m_lan_current_ip_label, lv_color_hex(0xCCCCCC), 0);
    
    y_pos += 25;
    
    // Current Netmask
    m_lan_current_netmask_label = lv_label_create(right_container);
    lv_label_set_text(m_lan_current_netmask_label, "Netmask: ---");
    lv_obj_set_pos(m_lan_current_netmask_label, 0, y_pos);
    lv_obj_set_style_text_color(m_lan_current_netmask_label, lv_color_hex(0xCCCCCC), 0);
    
    y_pos += 25;
    
    // Current Gateway
    m_lan_current_gateway_label = lv_label_create(right_container);
    lv_label_set_text(m_lan_current_gateway_label, "Gateway: ---");
    lv_obj_set_pos(m_lan_current_gateway_label, 0, y_pos);
    lv_obj_set_style_text_color(m_lan_current_gateway_label, lv_color_hex(0xCCCCCC), 0);
    
    // Load saved LAN configuration
    loadNetworkConfig();  // Refresh from managers
    loadLanConfigToUI();
    
    // Update status from LanManager
    LanManager& lan = LanManager::getInstance();
    if (lan.isConnected()) {
        std::string status = "Connected";
        onLanStatusChanged(status, lan.getIpAddress(), lan.getNetmask(), lan.getGateway());
    } else {
        EthConnectionStatus status = lan.getStatus();
        std::string status_str;
        switch (status) {
            case EthConnectionStatus::DISCONNECTED:
                status_str = "Disconnected";
                break;
            case EthConnectionStatus::LINK_DOWN:
                status_str = "Cable unplugged";
                break;
            case EthConnectionStatus::LINK_UP:
                status_str = "Obtaining IP...";
                break;
            default:
                status_str = "Unknown";
                break;
        }
        onLanStatusChanged(status_str, "", "", "");
    }
    
    ESP_LOGI(TAG, "LAN tab created");
}

void SettingsUI::createWifiNetworkTab(lv_obj_t* tab) {
    int field_height = 40;
    int input_width = 280;
    
    // === LEFT CONTAINER (Configuration) ===
    lv_obj_t* left_container = lv_obj_create(tab);
    lv_obj_set_size(left_container, LV_PCT(48), LV_PCT(95));
    lv_obj_set_pos(left_container, 10, 10);
    lv_obj_set_style_bg_color(left_container, lv_color_hex(0x2a2a2a), 0);
    lv_obj_set_style_border_color(left_container, lv_color_hex(0x444444), 0);
    lv_obj_set_style_border_width(left_container, 1, 0);
    lv_obj_set_style_pad_all(left_container, 15, 0);
    lv_obj_clear_flag(left_container, LV_OBJ_FLAG_SCROLLABLE);
    
    int y_pos = 10;
    
    // Configuration header
    lv_obj_t* config_header = lv_label_create(left_container);
    lv_label_set_text(config_header, "IP Configuration");
    lv_obj_set_pos(config_header, 0, y_pos);
    lv_obj_set_style_text_font(config_header, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(config_header, lv_color_hex(0x00BFFF), 0);
    
    y_pos += 35;
    
    // DHCP Switch
    lv_obj_t* dhcp_label = lv_label_create(left_container);
    lv_label_set_text(dhcp_label, "DHCP:");
    lv_obj_set_pos(dhcp_label, 0, y_pos + 5);
    
    m_wifi_dhcp_switch = lv_switch_create(left_container);
    lv_obj_set_pos(m_wifi_dhcp_switch, 100, y_pos);
    lv_obj_add_state(m_wifi_dhcp_switch, LV_STATE_CHECKED);  // Default to DHCP
    lv_obj_add_event_cb(m_wifi_dhcp_switch, wifi_dhcp_switch_cb, LV_EVENT_VALUE_CHANGED, this);
    
    y_pos += 50;
    
    // IP Address
    lv_obj_t* ip_label = lv_label_create(left_container);
    lv_label_set_text(ip_label, "IP Address:");
    lv_obj_set_pos(ip_label, 0, y_pos);
    
    y_pos += 22;
    
    m_wifi_ip_input = lv_textarea_create(left_container);
    lv_obj_set_size(m_wifi_ip_input, input_width, field_height);
    lv_obj_set_pos(m_wifi_ip_input, 0, y_pos);
    lv_textarea_set_one_line(m_wifi_ip_input, true);
    lv_textarea_set_text(m_wifi_ip_input, "");
    lv_textarea_set_accepted_chars(m_wifi_ip_input, "0123456789.");
    lv_obj_add_event_cb(m_wifi_ip_input, textarea_focused_cb, LV_EVENT_FOCUSED, this);
    lv_obj_add_event_cb(m_wifi_ip_input, textarea_defocused_cb, LV_EVENT_DEFOCUSED, this);
    lv_obj_add_state(m_wifi_ip_input, LV_STATE_DISABLED);  // Disabled by default (DHCP on)
    
    y_pos += field_height + 15;
    
    // Netmask
    lv_obj_t* netmask_label = lv_label_create(left_container);
    lv_label_set_text(netmask_label, "Netmask:");
    lv_obj_set_pos(netmask_label, 0, y_pos);
    
    y_pos += 22;
    
    m_wifi_netmask_input = lv_textarea_create(left_container);
    lv_obj_set_size(m_wifi_netmask_input, input_width, field_height);
    lv_obj_set_pos(m_wifi_netmask_input, 0, y_pos);
    lv_textarea_set_one_line(m_wifi_netmask_input, true);
    lv_textarea_set_text(m_wifi_netmask_input, "255.255.255.0");
    lv_textarea_set_accepted_chars(m_wifi_netmask_input, "0123456789.");
    lv_obj_add_event_cb(m_wifi_netmask_input, textarea_focused_cb, LV_EVENT_FOCUSED, this);
    lv_obj_add_event_cb(m_wifi_netmask_input, textarea_defocused_cb, LV_EVENT_DEFOCUSED, this);
    lv_obj_add_state(m_wifi_netmask_input, LV_STATE_DISABLED);
    
    y_pos += field_height + 15;
    
    // Gateway
    lv_obj_t* gateway_label = lv_label_create(left_container);
    lv_label_set_text(gateway_label, "Gateway:");
    lv_obj_set_pos(gateway_label, 0, y_pos);
    
    y_pos += 22;
    
    m_wifi_gateway_input = lv_textarea_create(left_container);
    lv_obj_set_size(m_wifi_gateway_input, input_width, field_height);
    lv_obj_set_pos(m_wifi_gateway_input, 0, y_pos);
    lv_textarea_set_one_line(m_wifi_gateway_input, true);
    lv_textarea_set_text(m_wifi_gateway_input, "");
    lv_textarea_set_accepted_chars(m_wifi_gateway_input, "0123456789.");
    lv_obj_add_event_cb(m_wifi_gateway_input, textarea_focused_cb, LV_EVENT_FOCUSED, this);
    lv_obj_add_event_cb(m_wifi_gateway_input, textarea_defocused_cb, LV_EVENT_DEFOCUSED, this);
    lv_obj_add_state(m_wifi_gateway_input, LV_STATE_DISABLED);
    
    y_pos += field_height + 25;
    
    // Apply button (full width of input fields)
    lv_obj_t* apply_btn = lv_button_create(left_container);
    lv_obj_set_size(apply_btn, input_width, 45);
    lv_obj_set_pos(apply_btn, 0, y_pos);
    lv_obj_add_event_cb(apply_btn, wifi_save_clicked_cb, LV_EVENT_CLICKED, this);
    
    lv_obj_t* apply_label = lv_label_create(apply_btn);
    lv_label_set_text(apply_label, "Apply");
    lv_obj_center(apply_label);
    
    // === RIGHT CONTAINER (Status) ===
    lv_obj_t* right_container = lv_obj_create(tab);
    lv_obj_set_size(right_container, LV_PCT(48), LV_PCT(95));
    lv_obj_set_pos(right_container, LV_PCT(52), 10);
    lv_obj_set_style_bg_color(right_container, lv_color_hex(0x2a2a2a), 0);
    lv_obj_set_style_border_color(right_container, lv_color_hex(0x444444), 0);
    lv_obj_set_style_border_width(right_container, 1, 0);
    lv_obj_set_style_pad_all(right_container, 15, 0);
    lv_obj_clear_flag(right_container, LV_OBJ_FLAG_SCROLLABLE);
    
    y_pos = 10;
    
    // Status header
    lv_obj_t* status_header = lv_label_create(right_container);
    lv_label_set_text(status_header, "Current Status");
    lv_obj_set_pos(status_header, 0, y_pos);
    lv_obj_set_style_text_font(status_header, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(status_header, lv_color_hex(0x00BFFF), 0);
    
    y_pos += 40;
    
    // Current connection status
    m_wifi_current_status_label = lv_label_create(right_container);
    lv_label_set_text(m_wifi_current_status_label, "Status: Not connected");
    lv_obj_set_pos(m_wifi_current_status_label, 0, y_pos);
    lv_obj_set_style_text_color(m_wifi_current_status_label, lv_color_hex(0xFFFFFF), 0);
    
    y_pos += 30;
    
    // Current SSID
    m_wifi_current_ssid_label = lv_label_create(right_container);
    lv_label_set_text(m_wifi_current_ssid_label, "SSID: ---");
    lv_obj_set_pos(m_wifi_current_ssid_label, 0, y_pos);
    lv_obj_set_style_text_color(m_wifi_current_ssid_label, lv_color_hex(0xCCCCCC), 0);
    
    y_pos += 25;
    
    // Current IP address
    m_wifi_current_ip_label = lv_label_create(right_container);
    lv_label_set_text(m_wifi_current_ip_label, "IP: ---");
    lv_obj_set_pos(m_wifi_current_ip_label, 0, y_pos);
    lv_obj_set_style_text_color(m_wifi_current_ip_label, lv_color_hex(0xCCCCCC), 0);
    
    y_pos += 25;
    
    // Current Netmask
    m_wifi_current_netmask_label = lv_label_create(right_container);
    lv_label_set_text(m_wifi_current_netmask_label, "Netmask: ---");
    lv_obj_set_pos(m_wifi_current_netmask_label, 0, y_pos);
    lv_obj_set_style_text_color(m_wifi_current_netmask_label, lv_color_hex(0xCCCCCC), 0);
    
    y_pos += 25;
    
    // Current Gateway
    m_wifi_current_gateway_label = lv_label_create(right_container);
    lv_label_set_text(m_wifi_current_gateway_label, "Gateway: ---");
    lv_obj_set_pos(m_wifi_current_gateway_label, 0, y_pos);
    lv_obj_set_style_text_color(m_wifi_current_gateway_label, lv_color_hex(0xCCCCCC), 0);
    
    // Load saved WiFi configuration
    loadWifiConfigToUI();
    
    // Update status from WirelessManager
    WirelessManager& wifi = WirelessManager::getInstance();
    if (wifi.isConnected()) {
        std::string status = "Connected";
        onWifiStatusChanged(status, wifi.getCurrentSsid(), wifi.getIpAddress(), wifi.getNetmask(), wifi.getGateway());
    } else {
        WifiConnectionStatus status = wifi.getStatus();
        std::string status_str;
        switch (status) {
            case WifiConnectionStatus::DISCONNECTED:
                status_str = "Disconnected";
                break;
            case WifiConnectionStatus::CONNECTING:
                status_str = "Connecting...";
                break;
            case WifiConnectionStatus::FAILED:
                status_str = "Connection failed";
                break;
            default:
                status_str = "Unknown";
                break;
        }
        onWifiStatusChanged(status_str, "", "", "", "");
    }
    
    ESP_LOGI(TAG, "WiFi Network tab created");
}

void SettingsUI::createWifiRadioTab(lv_obj_t* tab) {
    int field_height = 40;
    int input_width = 280;
    
    // === LEFT CONTAINER (Connection) ===
    lv_obj_t* left_container = lv_obj_create(tab);
    lv_obj_set_size(left_container, LV_PCT(48), LV_PCT(95));
    lv_obj_set_pos(left_container, 10, 10);
    lv_obj_set_style_bg_color(left_container, lv_color_hex(0x2a2a2a), 0);
    lv_obj_set_style_border_color(left_container, lv_color_hex(0x444444), 0);
    lv_obj_set_style_border_width(left_container, 1, 0);
    lv_obj_set_style_pad_all(left_container, 15, 0);
    lv_obj_clear_flag(left_container, LV_OBJ_FLAG_SCROLLABLE);
    
    int y_pos = 10;
    
    // Connection header
    lv_obj_t* config_header = lv_label_create(left_container);
    lv_label_set_text(config_header, "Connect to Network");
    lv_obj_set_pos(config_header, 0, y_pos);
    lv_obj_set_style_text_font(config_header, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(config_header, lv_color_hex(0x00BFFF), 0);
    
    y_pos += 35;
    
    // SSID input
    lv_obj_t* ssid_label = lv_label_create(left_container);
    lv_label_set_text(ssid_label, "SSID:");
    lv_obj_set_pos(ssid_label, 0, y_pos);
    
    y_pos += 22;
    
    m_wifi_ssid_input = lv_textarea_create(left_container);
    lv_obj_set_size(m_wifi_ssid_input, input_width, field_height);
    lv_obj_set_pos(m_wifi_ssid_input, 0, y_pos);
    lv_textarea_set_one_line(m_wifi_ssid_input, true);
    lv_textarea_set_placeholder_text(m_wifi_ssid_input, "Select or type");
    lv_obj_add_event_cb(m_wifi_ssid_input, textarea_focused_cb, LV_EVENT_FOCUSED, this);
    lv_obj_add_event_cb(m_wifi_ssid_input, textarea_defocused_cb, LV_EVENT_DEFOCUSED, this);
    
    y_pos += field_height + 15;
    
    // Password input
    lv_obj_t* password_label = lv_label_create(left_container);
    lv_label_set_text(password_label, "Password:");
    lv_obj_set_pos(password_label, 0, y_pos);
    
    y_pos += 22;
    
    m_wifi_password_input = lv_textarea_create(left_container);
    lv_obj_set_size(m_wifi_password_input, input_width, field_height);
    lv_obj_set_pos(m_wifi_password_input, 0, y_pos);
    lv_textarea_set_one_line(m_wifi_password_input, true);
    lv_textarea_set_password_mode(m_wifi_password_input, true);
    lv_obj_add_event_cb(m_wifi_password_input, textarea_focused_cb, LV_EVENT_FOCUSED, this);
    lv_obj_add_event_cb(m_wifi_password_input, textarea_defocused_cb, LV_EVENT_DEFOCUSED, this);
    
    y_pos += field_height + 25;
    
    // Connect button
    lv_obj_t* connect_btn = lv_button_create(left_container);
    lv_obj_set_size(connect_btn, 135, 45);
    lv_obj_set_pos(connect_btn, 0, y_pos);
    lv_obj_add_event_cb(connect_btn, wifi_connect_clicked_cb, LV_EVENT_CLICKED, this);
    
    lv_obj_t* connect_label = lv_label_create(connect_btn);
    lv_label_set_text(connect_label, "Connect");
    lv_obj_center(connect_label);
    
    // Scan button
    lv_obj_t* scan_btn = lv_button_create(left_container);
    lv_obj_set_size(scan_btn, 135, 45);
    lv_obj_set_pos(scan_btn, 145, y_pos);
    lv_obj_add_event_cb(scan_btn, wifi_scan_clicked_cb, LV_EVENT_CLICKED, this);
    
    lv_obj_t* scan_label = lv_label_create(scan_btn);
    lv_label_set_text(scan_label, LV_SYMBOL_REFRESH " Scan");
    lv_obj_center(scan_label);
    
    y_pos += 60;
    
    // Separator
    lv_obj_t* separator = lv_obj_create(left_container);
    lv_obj_set_size(separator, input_width, 2);
    lv_obj_set_pos(separator, 0, y_pos);
    lv_obj_set_style_bg_color(separator, lv_color_hex(0x555555), 0);
    lv_obj_set_style_border_width(separator, 0, 0);
    
    y_pos += 15;
    
    // Status section header
    lv_obj_t* status_section_header = lv_label_create(left_container);
    lv_label_set_text(status_section_header, "Connection Status");
    lv_obj_set_pos(status_section_header, 0, y_pos);
    lv_obj_set_style_text_font(status_section_header, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(status_section_header, lv_color_hex(0x00BFFF), 0);
    
    y_pos += 30;
    
    // Connection state
    m_wifi_radio_conn_state_label = lv_label_create(left_container);
    lv_label_set_text(m_wifi_radio_conn_state_label, "Status: Not connected");
    lv_obj_set_pos(m_wifi_radio_conn_state_label, 0, y_pos);
    lv_obj_set_style_text_color(m_wifi_radio_conn_state_label, lv_color_hex(0xFFFFFF), 0);
    
    y_pos += 25;
    
    // SSID
    m_wifi_radio_ssid_label = lv_label_create(left_container);
    lv_label_set_text(m_wifi_radio_ssid_label, "Network: ---");
    lv_obj_set_pos(m_wifi_radio_ssid_label, 0, y_pos);
    lv_obj_set_style_text_color(m_wifi_radio_ssid_label, lv_color_hex(0xCCCCCC), 0);
    
    y_pos += 25;
    
    // Signal strength
    m_wifi_radio_signal_label = lv_label_create(left_container);
    lv_label_set_text(m_wifi_radio_signal_label, "Signal: ---");
    lv_obj_set_pos(m_wifi_radio_signal_label, 0, y_pos);
    lv_obj_set_style_text_color(m_wifi_radio_signal_label, lv_color_hex(0xCCCCCC), 0);
    
    y_pos += 25;
    
    // Channel
    m_wifi_radio_channel_label = lv_label_create(left_container);
    lv_label_set_text(m_wifi_radio_channel_label, "Channel: ---");
    lv_obj_set_pos(m_wifi_radio_channel_label, 0, y_pos);
    lv_obj_set_style_text_color(m_wifi_radio_channel_label, lv_color_hex(0xCCCCCC), 0);
    
    // === RIGHT CONTAINER (WiFi AP List) ===
    lv_obj_t* right_container = lv_obj_create(tab);
    lv_obj_set_size(right_container, LV_PCT(48), LV_PCT(95));
    lv_obj_set_pos(right_container, LV_PCT(52), 10);
    lv_obj_set_style_bg_color(right_container, lv_color_hex(0x2a2a2a), 0);
    lv_obj_set_style_border_color(right_container, lv_color_hex(0x444444), 0);
    lv_obj_set_style_border_width(right_container, 1, 0);
    lv_obj_set_style_pad_all(right_container, 15, 0);
    lv_obj_clear_flag(right_container, LV_OBJ_FLAG_SCROLLABLE);
    
    // WiFi AP List header
    lv_obj_t* list_header = lv_label_create(right_container);
    lv_label_set_text(list_header, "Available Networks");
    lv_obj_set_pos(list_header, 0, 10);
    lv_obj_set_style_text_font(list_header, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(list_header, lv_color_hex(0x00BFFF), 0);
    
    // WiFi table - fill the rest of the container
    m_wifi_list = lv_table_create(right_container);
    lv_obj_set_size(m_wifi_list, LV_PCT(100), LV_PCT(90));
    lv_obj_set_pos(m_wifi_list, 0, 45);
    
    // Set up 3 columns
    lv_table_set_column_count(m_wifi_list, 3);
    lv_table_set_column_width(m_wifi_list, 0, 180);  // SSID
    lv_table_set_column_width(m_wifi_list, 1, 120);   // Signal
    lv_table_set_column_width(m_wifi_list, 2, 120);   // Security
    
    // Add header row
    lv_table_set_cell_value(m_wifi_list, 0, 0, "SSID");
    lv_table_set_cell_value(m_wifi_list, 0, 1, "Signal");
    lv_table_set_cell_value(m_wifi_list, 0, 2, "Security");
    
    // Add event callback for table cell clicks
    lv_obj_add_event_cb(m_wifi_list, wifi_ap_clicked_cb, LV_EVENT_PRESSED, this);
    
    ESP_LOGI(TAG, "WiFi Radio tab created");
}

void SettingsUI::createAboutTab(lv_obj_t* tab) {
    int y_pos = 20;
    
    // Get app description
    const esp_app_desc_t* app_desc = esp_app_get_description();
    
    // App version
    lv_obj_t* version_label = lv_label_create(tab);
    char version_text[128];
    snprintf(version_text, sizeof(version_text), "Version: %s", app_desc->version);
    lv_label_set_text(version_label, version_text);
    lv_obj_set_pos(version_label, 20, y_pos);
    lv_obj_set_style_text_font(version_label, &lv_font_montserrat_16, 0);
    
    y_pos += 40;
    
    // Build date/time
    lv_obj_t* date_label = lv_label_create(tab);
    char date_text[128];
    snprintf(date_text, sizeof(date_text), "Built: %s %s", app_desc->date, app_desc->time);
    lv_label_set_text(date_label, date_text);
    lv_obj_set_pos(date_label, 20, y_pos);
    
    y_pos += 35;
    
    // IDF version
    lv_obj_t* idf_label = lv_label_create(tab);
    char idf_text[128];
    snprintf(idf_text, sizeof(idf_text), "ESP-IDF: %s", app_desc->idf_ver);
    lv_label_set_text(idf_label, idf_text);
    lv_obj_set_pos(idf_label, 20, y_pos);
    
    y_pos += 50;
    
    // Hardware section
    lv_obj_t* hw_title = lv_label_create(tab);
    lv_label_set_text(hw_title, "Hardware:");
    lv_obj_set_pos(hw_title, 20, y_pos);
    lv_obj_set_style_text_font(hw_title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(hw_title, lv_color_hex(0x3498DB), 0);
    
    y_pos += 35;
    
    lv_obj_t* hw_list = lv_label_create(tab);
    lv_label_set_text(hw_list, 
        "  • ESP32-P4 Function EV Board\n"
        "  • 800x600 IPS LCD Display\n"
        "  • GT911 Touch Controller\n"
        "  • Ethernet PHY (W5500)\n"
        "  • ESP32-C6 WiFi (SDIO)");
    lv_obj_set_pos(hw_list, 30, y_pos);
    
    y_pos += 120;
    
    // Features section
    lv_obj_t* feat_title = lv_label_create(tab);
    lv_label_set_text(feat_title, "Features:");
    lv_obj_set_pos(feat_title, 20, y_pos);
    lv_obj_set_style_text_font(feat_title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(feat_title, lv_color_hex(0x3498DB), 0);
    
    y_pos += 35;
    
    lv_obj_t* feat_list = lv_label_create(tab);
    lv_label_set_text(feat_list, 
        "  • MQTT Client (JSON Config)\n"
        "  • Dynamic Widget System\n"
        "  • Ethernet & WiFi Support\n"
        "  • Touch-Optimized UI\n"
        "  • Real-time Updates");
    lv_obj_set_pos(feat_list, 30, y_pos);
    
    ESP_LOGI(TAG, "About tab created");
}

void SettingsUI::show() {
    if (!m_visible && m_settings_screen) {
        // Move settings screen back on screen
        lv_obj_align(m_settings_screen, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_bg_opa(m_settings_layer, LV_OPA_50, 0);  // Semi-transparent backdrop
        lv_obj_set_style_bg_color(m_settings_layer, lv_color_hex(0x000000), 0);
        
        lv_obj_clear_flag(m_settings_screen, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(m_settings_screen);
        m_visible = true;
        ESP_LOGI(TAG, "Settings screen shown");
    }
}

void SettingsUI::hide() {
    if (m_settings_screen) {
        // Hide keyboard if visible
        if (m_keyboard) {
            lv_obj_add_flag(m_keyboard, LV_OBJ_FLAG_HIDDEN);
        }
        
        // Move settings screen off-screen (gear icon stays visible)
        lv_obj_set_pos(m_settings_screen, 0, -5000);
        lv_obj_add_flag(m_settings_screen, LV_OBJ_FLAG_HIDDEN);
        
        // Make layer transparent again
        lv_obj_set_style_bg_opa(m_settings_layer, LV_OPA_TRANSP, 0);
        
        m_visible = false;
        ESP_LOGI(TAG, "Settings screen hidden");
    }
}

void SettingsUI::bringToFront() {
    if (m_settings_layer) {
        lv_obj_move_foreground(m_settings_layer);
    }
}

void SettingsUI::gear_clicked_cb(lv_event_t* e) {
    SettingsUI* ui = static_cast<SettingsUI*>(lv_event_get_user_data(e));
    ui->show();
}

void SettingsUI::mqtt_save_clicked_cb(lv_event_t* e) {
    SettingsUI* ui = static_cast<SettingsUI*>(lv_event_get_user_data(e));
    
    // Hide keyboard if visible
    if (ui->m_keyboard) {
        lv_obj_add_flag(ui->m_keyboard, LV_OBJ_FLAG_HIDDEN);
        lv_obj_center(ui->m_settings_screen);
    }
    
    // Read values from inputs
    ui->m_broker_uri = lv_textarea_get_text(ui->m_broker_input);
    ui->m_username = lv_textarea_get_text(ui->m_username_input);
    ui->m_password = lv_textarea_get_text(ui->m_password_input);
    ui->m_client_id = lv_textarea_get_text(ui->m_client_id_input);
    ui->m_config_topic = lv_textarea_get_text(ui->m_config_topic_input);
    
    // Save to NVS
    if (ui->saveSettings()) {
        ESP_LOGI(TAG, "MQTT settings saved, reconnecting...");
        
        // Reconnect MQTT with new settings
        MQTTManager::getInstance().deinit();
        
        if (!ui->m_username.empty()) {
            MQTTManager::getInstance().init(ui->m_broker_uri, ui->m_username, 
                                            ui->m_password, ui->m_client_id);
        } else {
            MQTTManager::getInstance().init(ui->m_broker_uri, ui->m_client_id);
        }
    }
}

void SettingsUI::close_clicked_cb(lv_event_t* e) {
    SettingsUI* ui = static_cast<SettingsUI*>(lv_event_get_user_data(e));
    ui->hide();
}

void SettingsUI::tab_changed_cb(lv_event_t* e) {
    SettingsUI* ui = static_cast<SettingsUI*>(lv_event_get_user_data(e));
    
    // Get the active tab after the change
    uint32_t tab_idx = lv_tabview_get_tab_active(ui->m_tabview);
    
    ESP_LOGI(TAG, "Tab changed to index: %lu", tab_idx);
    
    // Tab indices: 0=MQTT, 1=LAN, 2=WiFi Network, 3=WiFi Radio, 4=About, 5=Close
    if (tab_idx == 5) {
        ESP_LOGI(TAG, "Close tab selected, hiding settings");
        ui->hide();
    } else if (tab_idx == 1) {
        // LAN tab - refresh status
        ESP_LOGI(TAG, "LAN tab activated, refreshing status");
        LanManager& lan = LanManager::getInstance();
        if (lan.isConnected()) {
            ui->onLanStatusChanged("Connected", lan.getIpAddress(), lan.getNetmask(), lan.getGateway());
        } else {
            EthConnectionStatus status = lan.getStatus();
            std::string status_str;
            switch (status) {
                case EthConnectionStatus::DISCONNECTED:
                    status_str = "Disconnected";
                    break;
                case EthConnectionStatus::LINK_DOWN:
                    status_str = "Cable unplugged";
                    break;
                case EthConnectionStatus::LINK_UP:
                    status_str = "Obtaining IP...";
                    break;
                default:
                    status_str = "Unknown";
                    break;
            }
            ui->onLanStatusChanged(status_str, "", "", "");
        }
    } else if (tab_idx == 2 || tab_idx == 3) {
        // WiFi Network or WiFi Radio tab - refresh status
        ESP_LOGI(TAG, "WiFi tab activated, refreshing status");
        WirelessManager& wifi = WirelessManager::getInstance();
        if (wifi.isConnected()) {
            ui->onWifiStatusChanged("Connected", wifi.getCurrentSsid(), wifi.getIpAddress(), wifi.getNetmask(), wifi.getGateway());
        } else {
            WifiConnectionStatus status = wifi.getStatus();
            std::string status_str;
            switch (status) {
                case WifiConnectionStatus::DISCONNECTED:
                    status_str = "Disconnected";
                    break;
                case WifiConnectionStatus::CONNECTING:
                    status_str = "Connecting...";
                    break;
                case WifiConnectionStatus::FAILED:
                    status_str = "Connection failed";
                    break;
                default:
                    status_str = "Unknown";
                    break;
            }
            ui->onWifiStatusChanged(status_str, "", "", "", "");
        }
    }
}

void SettingsUI::lan_dhcp_switch_cb(lv_event_t* e) {
    SettingsUI* ui = static_cast<SettingsUI*>(lv_event_get_user_data(e));
    lv_obj_t* sw = static_cast<lv_obj_t*>(lv_event_get_target(e));
    
    bool dhcp_enabled = lv_obj_has_state(sw, LV_STATE_CHECKED);
    
    // Enable/disable static IP fields based on DHCP state
    if (dhcp_enabled) {
        lv_obj_add_state(ui->m_lan_ip_input, LV_STATE_DISABLED);
        lv_obj_add_state(ui->m_lan_netmask_input, LV_STATE_DISABLED);
        lv_obj_add_state(ui->m_lan_gateway_input, LV_STATE_DISABLED);
        ESP_LOGI(TAG, "DHCP enabled, static IP fields disabled");
    } else {
        lv_obj_clear_state(ui->m_lan_ip_input, LV_STATE_DISABLED);
        lv_obj_clear_state(ui->m_lan_netmask_input, LV_STATE_DISABLED);
        lv_obj_clear_state(ui->m_lan_gateway_input, LV_STATE_DISABLED);
        ESP_LOGI(TAG, "DHCP disabled, static IP fields enabled");
    }
}

void SettingsUI::lan_save_clicked_cb(lv_event_t* e) {
    SettingsUI* ui = static_cast<SettingsUI*>(lv_event_get_user_data(e));
    
    // Hide keyboard if visible
    if (ui->m_keyboard) {
        lv_obj_add_flag(ui->m_keyboard, LV_OBJ_FLAG_HIDDEN);
        lv_obj_center(ui->m_settings_screen);
    }
    
    bool dhcp_enabled = lv_obj_has_state(ui->m_lan_dhcp_switch, LV_STATE_CHECKED);
    
    // Update network config
    ui->m_network_config.lan_dhcp = dhcp_enabled;
    if (!dhcp_enabled) {
        ui->m_network_config.lan_ip = lv_textarea_get_text(ui->m_lan_ip_input);
        ui->m_network_config.lan_netmask = lv_textarea_get_text(ui->m_lan_netmask_input);
        ui->m_network_config.lan_gateway = lv_textarea_get_text(ui->m_lan_gateway_input);
    }
    
    // Apply and save configuration via LAN Manager
    LanManager& lan = LanManager::getInstance();
    if (dhcp_enabled) {
        lan.setIpConfig(EthIpConfigMode::DHCP, nullptr);
        lan.saveConfig();
        ESP_LOGI(TAG, "LAN configured for DHCP and saved");
    } else {
        EthStaticIpConfig config;
        config.ip = ui->m_network_config.lan_ip;
        config.netmask = ui->m_network_config.lan_netmask;
        config.gateway = ui->m_network_config.lan_gateway;
        
        lan.setIpConfig(EthIpConfigMode::STATIC, &config);
        lan.saveConfig();
        ESP_LOGI(TAG, "LAN configured for static IP: %s and saved", config.ip.c_str());
    }
}

void SettingsUI::wifi_scan_clicked_cb(lv_event_t* e) {
    SettingsUI* ui = static_cast<SettingsUI*>(lv_event_get_user_data(e));
    ui->performWifiScan();
}

void SettingsUI::wifi_ap_clicked_cb(lv_event_t* e) {
    SettingsUI* ui = static_cast<SettingsUI*>(lv_event_get_user_data(e));
    lv_obj_t* table = static_cast<lv_obj_t*>(lv_event_get_target(e));
    
    // Get selected cell
    uint32_t row, col;
    lv_table_get_selected_cell(table, &row, &col);
    
    // Skip header row (0) and invalid selections
    if (row == 0 || row == LV_TABLE_CELL_NONE) {
        return;
    }
    
    // Get SSID from column 0
    const char* ssid = lv_table_get_cell_value(table, row, 0);
    if (ssid && ssid[0] != '\0') {
        ui->m_selected_ssid = std::string(ssid);
        ESP_LOGI(TAG, "Selected WiFi AP: %s", ui->m_selected_ssid.c_str());
        
        // Update SSID input field
        if (ui->m_wifi_ssid_input) {
            lv_textarea_set_text(ui->m_wifi_ssid_input, ui->m_selected_ssid.c_str());
        }
    }
}

void SettingsUI::wifi_connect_clicked_cb(lv_event_t* e) {
    SettingsUI* ui = static_cast<SettingsUI*>(lv_event_get_user_data(e));
    
    // Hide keyboard if visible
    if (ui->m_keyboard) {
        lv_obj_add_flag(ui->m_keyboard, LV_OBJ_FLAG_HIDDEN);
        lv_obj_center(ui->m_settings_screen);
    }
    
    // Get SSID from input field
    std::string ssid = lv_textarea_get_text(ui->m_wifi_ssid_input);
    if (ssid.empty()) {
        ESP_LOGW(TAG, "No WiFi SSID entered");
        if (ui->m_wifi_radio_conn_state_label) {
            lv_label_set_text(ui->m_wifi_radio_conn_state_label, "Enter SSID first");
            lv_obj_set_style_text_color(ui->m_wifi_radio_conn_state_label, lv_color_hex(0xFF0000), 0);
        }
        return;
    }
    
    std::string password = lv_textarea_get_text(ui->m_wifi_password_input);
    
    // Set initial connecting state
    if (ui->m_wifi_radio_conn_state_label) {
        lv_label_set_text_fmt(ui->m_wifi_radio_conn_state_label, "Connecting to %s...", ssid.c_str());
        lv_obj_set_style_text_color(ui->m_wifi_radio_conn_state_label, lv_color_hex(0xFFFF00), 0);
    }
    
    // Initiate connection - status updates will come via callback
    WirelessManager& wifi = WirelessManager::getInstance();
    wifi.connect(ssid, password, 15000);
    ESP_LOGI(TAG, "WiFi connection initiated for: %s", ssid.c_str());
}

void SettingsUI::textarea_focused_cb(lv_event_t* e) {
    SettingsUI* ui = static_cast<SettingsUI*>(lv_event_get_user_data(e));
    lv_obj_t* textarea = (lv_obj_t*)lv_event_get_target(e);
    
    if (ui->m_keyboard) {
        lv_keyboard_set_textarea(ui->m_keyboard, textarea);
        
        // Set keyboard mode based on textarea type
        if (textarea == ui->m_lan_ip_input || 
            textarea == ui->m_lan_netmask_input || 
            textarea == ui->m_lan_gateway_input ||
            textarea == ui->m_wifi_ip_input ||
            textarea == ui->m_wifi_netmask_input ||
            textarea == ui->m_wifi_gateway_input) {
            // Use number mode for IP address fields
            lv_keyboard_set_mode(ui->m_keyboard, LV_KEYBOARD_MODE_NUMBER);
            ESP_LOGD(TAG, "Keyboard mode set to NUMBER for IP field");
        } else {
            // Use text mode for other fields
            lv_keyboard_set_mode(ui->m_keyboard, LV_KEYBOARD_MODE_TEXT_LOWER);
            ESP_LOGD(TAG, "Keyboard mode set to TEXT");
        }
        
        lv_obj_clear_flag(ui->m_keyboard, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(ui->m_keyboard);  // Ensure keyboard is on top
        
        // Get keyboard and screen dimensions
        int32_t keyboard_height = lv_obj_get_height(ui->m_keyboard);
        int32_t screen_height = lv_obj_get_height(lv_screen_active());
        
        // Get textarea absolute position (including parent offsets)
        lv_area_t textarea_area;
        lv_obj_get_coords(textarea, &textarea_area);
        int32_t textarea_bottom = textarea_area.y2;
        
        // Calculate where keyboard starts
        int32_t keyboard_top = screen_height - keyboard_height;
        
        // Check if textarea would be covered by keyboard
        if (textarea_bottom > keyboard_top) {
            // Move screen up by just enough to show textarea above keyboard
            int32_t overlap = textarea_bottom - keyboard_top + 20; // +20px padding
            lv_obj_align(ui->m_settings_screen, LV_ALIGN_TOP_MID, 0, -overlap);
            ESP_LOGD(TAG, "Keyboard shown, moved screen up by %d px to avoid overlap", overlap);
        } else {
            // Textarea is already visible, no need to move
            lv_obj_center(ui->m_settings_screen);
            ESP_LOGD(TAG, "Keyboard shown, textarea already visible");
        }
    }
}

void SettingsUI::textarea_defocused_cb(lv_event_t* e) {
    SettingsUI* ui = static_cast<SettingsUI*>(lv_event_get_user_data(e));
    
    // Hide keyboard when textarea loses focus
    if (ui->m_keyboard) {
        lv_obj_add_flag(ui->m_keyboard, LV_OBJ_FLAG_HIDDEN);
        lv_obj_center(ui->m_settings_screen);
        ESP_LOGD(TAG, "Keyboard hidden (textarea defocused)");
    }
}



void SettingsUI::createKeyboard() {
    if (!m_keyboard) {
        // Create keyboard on root screen, not settings layer
        m_keyboard = lv_keyboard_create(lv_screen_active());
        lv_obj_set_size(m_keyboard, LV_PCT(100), LV_PCT(40));
        lv_obj_align(m_keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_add_flag(m_keyboard, LV_OBJ_FLAG_HIDDEN);  // Start hidden
        lv_obj_add_flag(m_keyboard, LV_OBJ_FLAG_FLOATING);  // Always on top
        
        // Add callback for when Enter is pressed on keyboard
        lv_obj_add_event_cb(m_keyboard, keyboard_ready_cb, LV_EVENT_READY, this);
        
        ESP_LOGI(TAG, "Keyboard created on root screen (floating)");
    }
}

void SettingsUI::destroyKeyboard() {
    if (m_keyboard) {
        lv_obj_delete(m_keyboard);
        m_keyboard = nullptr;
        ESP_LOGI(TAG, "Keyboard destroyed");
    }
}

void SettingsUI::keyboard_ready_cb(lv_event_t* e) {
    SettingsUI* ui = static_cast<SettingsUI*>(lv_event_get_user_data(e));
    
    // Hide keyboard when Enter is pressed
    if (ui->m_keyboard) {
        lv_obj_add_flag(ui->m_keyboard, LV_OBJ_FLAG_HIDDEN);
        // Restore settings panel to center position
        lv_obj_center(ui->m_settings_screen);
        ESP_LOGD(TAG, "Keyboard hidden (Enter pressed)");
    }
}

bool SettingsUI::loadSettings() {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "No saved settings found, using defaults");
        return false;
    }
    
    // Helper lambda to read string from NVS
    auto read_str = [&](const char* key, std::string& value) {
        size_t required_size = 0;
        err = nvs_get_str(nvs_handle, key, NULL, &required_size);
        if (err == ESP_OK && required_size > 0) {
            char* buffer = (char*)malloc(required_size);
            if (buffer) {
                err = nvs_get_str(nvs_handle, key, buffer, &required_size);
                if (err == ESP_OK) {
                    value = buffer;
                }
                free(buffer);
            }
        }
    };
    
    read_str(NVS_KEY_BROKER, m_broker_uri);
    read_str(NVS_KEY_USERNAME, m_username);
    read_str(NVS_KEY_PASSWORD, m_password);
    read_str(NVS_KEY_CLIENT_ID, m_client_id);
    read_str(NVS_KEY_CONFIG_TOPIC, m_config_topic);
    
    nvs_close(nvs_handle);
    
    ESP_LOGI(TAG, "Settings loaded from NVS");
    ESP_LOGI(TAG, "  Broker: %s", m_broker_uri.c_str());
    ESP_LOGI(TAG, "  Username: %s", m_username.empty() ? "(none)" : m_username.c_str());
    ESP_LOGI(TAG, "  Client ID: %s", m_client_id.empty() ? "(auto)" : m_client_id.c_str());
    ESP_LOGI(TAG, "  Config Topic: %s", m_config_topic.c_str());
    
    return true;
}

bool SettingsUI::saveSettings() {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS for writing: %s", esp_err_to_name(err));
        return false;
    }
    
    // Helper lambda to write string to NVS
    auto write_str = [&](const char* key, const std::string& value) {
        return nvs_set_str(nvs_handle, key, value.c_str());
    };
    
    bool success = true;
    success &= (write_str(NVS_KEY_BROKER, m_broker_uri) == ESP_OK);
    success &= (write_str(NVS_KEY_USERNAME, m_username) == ESP_OK);
    success &= (write_str(NVS_KEY_PASSWORD, m_password) == ESP_OK);
    success &= (write_str(NVS_KEY_CLIENT_ID, m_client_id) == ESP_OK);
    success &= (write_str(NVS_KEY_CONFIG_TOPIC, m_config_topic) == ESP_OK);
    
    err = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    
    if (err != ESP_OK || !success) {
        ESP_LOGE(TAG, "Failed to save settings to NVS");
        return false;
    }
    
    ESP_LOGI(TAG, "Settings saved to NVS");
    return true;
}

void SettingsUI::performWifiScan() {
    ESP_LOGI(TAG, "Starting WiFi scan...");
    
    // Update status to show scanning
    if (m_wifi_radio_conn_state_label) {
        lv_label_set_text(m_wifi_radio_conn_state_label, "Scanning...");
        lv_obj_set_style_text_color(m_wifi_radio_conn_state_label, lv_color_hex(0xFFAA00), 0);
    }
    
    WirelessManager& wifi = WirelessManager::getInstance();
    
    // Use async scan to avoid blocking HMI task
    wifi.scanAsync([this](const std::vector<WifiNetworkInfo>& networks, esp_err_t err) {
        struct WifiScanUiUpdate {
            SettingsUI* ui;
            std::vector<WifiAP> aps;
            bool success;
        };

        auto* update = new WifiScanUiUpdate{this, {}, err == ESP_OK};

        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Found %d WiFi networks", networks.size());

            // Convert to WifiAP structure
            update->aps.reserve(networks.size());
            for (const auto& net : networks) {
                WifiAP ap;
                ap.ssid = net.ssid;
                ap.rssi = net.rssi;
                ap.requires_password = (net.auth_mode != WIFI_AUTH_OPEN);
                update->aps.push_back(ap);
            }
        } else {
            ESP_LOGE(TAG, "WiFi scan failed: %s", esp_err_to_name(err));
        }

        lv_async_call([](void* user_data) {
            auto* data = static_cast<WifiScanUiUpdate*>(user_data);
            if (data->success) {
                data->ui->updateWifiScanResults(data->aps);
                if (data->ui->m_wifi_radio_conn_state_label) {
                    lv_label_set_text(data->ui->m_wifi_radio_conn_state_label, "Scan complete");
                    lv_obj_set_style_text_color(data->ui->m_wifi_radio_conn_state_label, lv_color_hex(0x00FF00), 0);
                }
            } else {
                if (data->ui->m_wifi_radio_conn_state_label) {
                    lv_label_set_text(data->ui->m_wifi_radio_conn_state_label, "Scan failed");
                    lv_obj_set_style_text_color(data->ui->m_wifi_radio_conn_state_label, lv_color_hex(0xFF0000), 0);
                }
            }
            delete data;
        }, update);
    }, 20);
}

void SettingsUI::updateWifiScanResults(const std::vector<WifiAP>& aps) {
    if (!m_wifi_list) return;
    
    ESP_LOGI(TAG, "Updating WiFi table with %d APs", aps.size());
    
    // Set row count (header + APs)
    lv_table_set_row_count(m_wifi_list, aps.size() + 1);
    
    // Populate table rows (starting from row 1, row 0 is header)
    for (size_t i = 0; i < aps.size(); i++) {
        const auto& ap = aps[i];
        uint16_t row = i + 1;
        
        // Column 0: SSID
        lv_table_set_cell_value(m_wifi_list, row, 0, ap.ssid.c_str());
        
        // Column 1: Signal strength in dB
        char signal_str[16];
        snprintf(signal_str, sizeof(signal_str), "%d dB", ap.rssi);
        lv_table_set_cell_value(m_wifi_list, row, 1, signal_str);
        
        // Column 2: Security
        const char* security = ap.requires_password ? "Protected" : "Open";
        lv_table_set_cell_value(m_wifi_list, row, 2, security);
    }
}

// Network configuration persistence
bool SettingsUI::loadNetworkConfig() {
    // Load LAN configuration from LAN Manager
    LanManager& lan = LanManager::getInstance();
    m_network_config.lan_dhcp = (lan.getIpMode() == EthIpConfigMode::DHCP);
    EthStaticIpConfig lan_config = lan.getStaticConfig();
    m_network_config.lan_ip = lan_config.ip;
    m_network_config.lan_netmask = lan_config.netmask;
    m_network_config.lan_gateway = lan_config.gateway;
    
    // Load WiFi configuration from Wireless Manager
    WirelessManager& wifi = WirelessManager::getInstance();
    m_network_config.wifi_dhcp = (wifi.getIpMode() == IpConfigMode::DHCP);
    StaticIpConfig wifi_config = wifi.getStaticConfig();
    m_network_config.wifi_ip = wifi_config.ip;
    m_network_config.wifi_netmask = wifi_config.netmask;
    m_network_config.wifi_gateway = wifi_config.gateway;
    
    ESP_LOGI(TAG, "Network config loaded from managers");
    return true;
}

bool SettingsUI::saveNetworkConfig() {
    // Delegate to managers - they handle their own NVS storage
    ESP_LOGI(TAG, "Network config will be saved by respective managers");
    return true;
}

void SettingsUI::loadLanConfigToUI() {
    if (!m_lan_dhcp_switch || !m_lan_ip_input) return;
    
    // Set DHCP switch state
    if (m_network_config.lan_dhcp) {
        lv_obj_add_state(m_lan_dhcp_switch, LV_STATE_CHECKED);
        lv_obj_add_state(m_lan_ip_input, LV_STATE_DISABLED);
        lv_obj_add_state(m_lan_netmask_input, LV_STATE_DISABLED);
        lv_obj_add_state(m_lan_gateway_input, LV_STATE_DISABLED);
    } else {
        lv_obj_clear_state(m_lan_dhcp_switch, LV_STATE_CHECKED);
        lv_obj_clear_state(m_lan_ip_input, LV_STATE_DISABLED);
        lv_obj_clear_state(m_lan_netmask_input, LV_STATE_DISABLED);
        lv_obj_clear_state(m_lan_gateway_input, LV_STATE_DISABLED);
    }
    
    // Set IP values
    lv_textarea_set_text(m_lan_ip_input, m_network_config.lan_ip.c_str());
    lv_textarea_set_text(m_lan_netmask_input, m_network_config.lan_netmask.c_str());
    lv_textarea_set_text(m_lan_gateway_input, m_network_config.lan_gateway.c_str());
}

void SettingsUI::loadWifiConfigToUI() {
    if (!m_wifi_dhcp_switch || !m_wifi_ip_input) return;
    
    // Set DHCP switch state
    if (m_network_config.wifi_dhcp) {
        lv_obj_add_state(m_wifi_dhcp_switch, LV_STATE_CHECKED);
        lv_obj_add_state(m_wifi_ip_input, LV_STATE_DISABLED);
        lv_obj_add_state(m_wifi_netmask_input, LV_STATE_DISABLED);
        lv_obj_add_state(m_wifi_gateway_input, LV_STATE_DISABLED);
    } else {
        lv_obj_clear_state(m_wifi_dhcp_switch, LV_STATE_CHECKED);
        lv_obj_clear_state(m_wifi_ip_input, LV_STATE_DISABLED);
        lv_obj_clear_state(m_wifi_netmask_input, LV_STATE_DISABLED);
        lv_obj_clear_state(m_wifi_gateway_input, LV_STATE_DISABLED);
    }
    
    // Set IP values
    lv_textarea_set_text(m_wifi_ip_input, m_network_config.wifi_ip.c_str());
    lv_textarea_set_text(m_wifi_netmask_input, m_network_config.wifi_netmask.c_str());
    lv_textarea_set_text(m_wifi_gateway_input, m_network_config.wifi_gateway.c_str());
}

void SettingsUI::wifi_dhcp_switch_cb(lv_event_t* e) {
    SettingsUI* ui = static_cast<SettingsUI*>(lv_event_get_user_data(e));
    lv_obj_t* sw = static_cast<lv_obj_t*>(lv_event_get_target(e));
    
    bool dhcp_enabled = lv_obj_has_state(sw, LV_STATE_CHECKED);
    
    // Enable/disable static IP fields based on DHCP state
    if (dhcp_enabled) {
        lv_obj_add_state(ui->m_wifi_ip_input, LV_STATE_DISABLED);
        lv_obj_add_state(ui->m_wifi_netmask_input, LV_STATE_DISABLED);
        lv_obj_add_state(ui->m_wifi_gateway_input, LV_STATE_DISABLED);
        ESP_LOGI(TAG, "WiFi DHCP enabled, static IP fields disabled");
    } else {
        lv_obj_clear_state(ui->m_wifi_ip_input, LV_STATE_DISABLED);
        lv_obj_clear_state(ui->m_wifi_netmask_input, LV_STATE_DISABLED);
        lv_obj_clear_state(ui->m_wifi_gateway_input, LV_STATE_DISABLED);
        ESP_LOGI(TAG, "WiFi DHCP disabled, static IP fields enabled");
    }
}

void SettingsUI::wifi_save_clicked_cb(lv_event_t* e) {
    SettingsUI* ui = static_cast<SettingsUI*>(lv_event_get_user_data(e));
    
    // Hide keyboard if visible
    if (ui->m_keyboard) {
        lv_obj_add_flag(ui->m_keyboard, LV_OBJ_FLAG_HIDDEN);
        lv_obj_center(ui->m_settings_screen);
    }
    
    bool dhcp_enabled = lv_obj_has_state(ui->m_wifi_dhcp_switch, LV_STATE_CHECKED);
    
    // Update network config
    ui->m_network_config.wifi_dhcp = dhcp_enabled;
    if (!dhcp_enabled) {
        ui->m_network_config.wifi_ip = lv_textarea_get_text(ui->m_wifi_ip_input);
        ui->m_network_config.wifi_netmask = lv_textarea_get_text(ui->m_wifi_netmask_input);
        ui->m_network_config.wifi_gateway = lv_textarea_get_text(ui->m_wifi_gateway_input);
    }
    
    // Apply and save configuration via Wireless Manager
    WirelessManager& wifi = WirelessManager::getInstance();
    if (dhcp_enabled) {
        wifi.setIpConfig(IpConfigMode::DHCP, nullptr);
        wifi.saveConfig();
        ESP_LOGI(TAG, "WiFi configured for DHCP and saved");
    } else {
        StaticIpConfig config;
        config.ip = ui->m_network_config.wifi_ip;
        config.netmask = ui->m_network_config.wifi_netmask;
        config.gateway = ui->m_network_config.wifi_gateway;
        
        wifi.setIpConfig(IpConfigMode::STATIC, &config);
        wifi.saveConfig();
        ESP_LOGI(TAG, "WiFi configured for static IP: %s and saved", config.ip.c_str());
    }
}

// Network manager callback implementations
void SettingsUI::onLanStatusChanged(const std::string& status, const std::string& ip, const std::string& netmask, const std::string& gateway) {
    if (!m_lan_current_status_label) return;
    
    // Update status label with appropriate color
    std::string status_text = "Status: " + status;
    lv_label_set_text(m_lan_current_status_label, status_text.c_str());
    
    if (status == "Connected") {
        lv_obj_set_style_text_color(m_lan_current_status_label, lv_color_hex(0x00FF00), 0);
    } else if (status == "Obtaining IP..." || status == "Link up") {
        lv_obj_set_style_text_color(m_lan_current_status_label, lv_color_hex(0xFFFF00), 0);
    } else {
        lv_obj_set_style_text_color(m_lan_current_status_label, lv_color_hex(0xFF6666), 0);
    }
    
    // Update IP information
    if (m_lan_current_ip_label) {
        std::string ip_text = ip.empty() ? "IP: ---" : "IP: " + ip;
        lv_label_set_text(m_lan_current_ip_label, ip_text.c_str());
    }
    
    if (m_lan_current_netmask_label) {
        std::string netmask_text = netmask.empty() ? "Netmask: ---" : "Netmask: " + netmask;
        lv_label_set_text(m_lan_current_netmask_label, netmask_text.c_str());
    }
    
    if (m_lan_current_gateway_label) {
        std::string gateway_text = gateway.empty() ? "Gateway: ---" : "Gateway: " + gateway;
        lv_label_set_text(m_lan_current_gateway_label, gateway_text.c_str());
    }
}

void SettingsUI::onWifiStatusChanged(const std::string& status, const std::string& ssid, const std::string& ip, const std::string& netmask, const std::string& gateway) {
    if (!m_wifi_current_status_label) return;
    
    // Update status label with appropriate color
    std::string status_text = "Status: " + status;
    lv_label_set_text(m_wifi_current_status_label, status_text.c_str());
    
    if (status == "Connected") {
        lv_obj_set_style_text_color(m_wifi_current_status_label, lv_color_hex(0x00FF00), 0);
    } else if (status == "Connecting..." || status.find("Obtaining") != std::string::npos) {
        lv_obj_set_style_text_color(m_wifi_current_status_label, lv_color_hex(0xFFFF00), 0);
    } else {
        lv_obj_set_style_text_color(m_wifi_current_status_label, lv_color_hex(0xFF6666), 0);
    }
    
    // Update SSID
    if (m_wifi_current_ssid_label) {
        std::string ssid_text = ssid.empty() ? "SSID: ---" : "SSID: " + ssid;
        lv_label_set_text(m_wifi_current_ssid_label, ssid_text.c_str());
    }
    
    // Update IP information
    if (m_wifi_current_ip_label) {
        std::string ip_text = ip.empty() ? "IP: ---" : "IP: " + ip;
        lv_label_set_text(m_wifi_current_ip_label, ip_text.c_str());
    }
    
    if (m_wifi_current_netmask_label) {
        std::string netmask_text = netmask.empty() ? "Netmask: ---" : "Netmask: " + netmask;
        lv_label_set_text(m_wifi_current_netmask_label, netmask_text.c_str());
    }
    
    if (m_wifi_current_gateway_label) {
        std::string gateway_text = gateway.empty() ? "Gateway: ---" : "Gateway: " + gateway;
        lv_label_set_text(m_wifi_current_gateway_label, gateway_text.c_str());
    }
    
    // Update WiFi Radio tab status labels (only if they exist)
    if (m_wifi_radio_conn_state_label) {
        std::string conn_text = "Status: " + status;
        lv_label_set_text(m_wifi_radio_conn_state_label, conn_text.c_str());
        
        if (status == "Connected") {
            lv_obj_set_style_text_color(m_wifi_radio_conn_state_label, lv_color_hex(0x00FF00), 0);
        } else if (status == "Connecting..." || status.find("Obtaining") != std::string::npos) {
            lv_obj_set_style_text_color(m_wifi_radio_conn_state_label, lv_color_hex(0xFFFF00), 0);
        } else {
            lv_obj_set_style_text_color(m_wifi_radio_conn_state_label, lv_color_hex(0xFF6666), 0);
        }
    }
    
    if (m_wifi_radio_ssid_label) {
        std::string ssid_text = ssid.empty() ? "Network: ---" : "Network: " + ssid;
        lv_label_set_text(m_wifi_radio_ssid_label, ssid_text.c_str());
    }
    
    // Signal and channel would need additional data from WirelessManager
    // For now, these remain as placeholders
}

void SettingsUI::onMqttStatusChanged(bool connected, uint32_t messages_received, uint32_t messages_sent) {
    if (!m_mqtt_status_label) return;
    
    // Update connection status with appropriate color
    if (connected) {
        lv_label_set_text(m_mqtt_status_label, "Status: Connected");
        lv_obj_set_style_text_color(m_mqtt_status_label, lv_color_hex(0x00FF00), 0);
    } else {
        lv_label_set_text(m_mqtt_status_label, "Status: Disconnected");
        lv_obj_set_style_text_color(m_mqtt_status_label, lv_color_hex(0xFF6666), 0);
    }
    
    // Update broker display
    if (m_mqtt_broker_label) {
        if (connected && !m_broker_uri.empty()) {
            std::string broker_text = "Broker: " + m_broker_uri;
            // Truncate if too long
            if (broker_text.length() > 40) {
                broker_text = broker_text.substr(0, 37) + "...";
            }
            lv_label_set_text(m_mqtt_broker_label, broker_text.c_str());
        } else {
            lv_label_set_text(m_mqtt_broker_label, "Broker: ---");
        }
    }
    
    // Update statistics
    if (m_mqtt_messages_received_label) {
        char buf[32];
        snprintf(buf, sizeof(buf), "Received: %lu", (unsigned long)messages_received);
        lv_label_set_text(m_mqtt_messages_received_label, buf);
    }
    
    if (m_mqtt_messages_sent_label) {
        char buf[32];
        snprintf(buf, sizeof(buf), "Sent: %lu", (unsigned long)messages_sent);
        lv_label_set_text(m_mqtt_messages_sent_label, buf);
    }
}
