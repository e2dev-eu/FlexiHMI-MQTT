#include "settings_ui.h"
#include "mqtt_manager.h"
#include "wireless_manager.h"
#include "lan_manager.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_app_desc.h"
#include "esp_idf_version.h"

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
    : m_gear_icon(nullptr)
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
    , m_lan_status_label(nullptr)
    , m_wifi_list(nullptr)
    , m_wifi_ssid_input(nullptr)
    , m_wifi_password_input(nullptr)
    , m_wifi_status_label(nullptr)
    , m_visible(false)
    , m_broker_uri("mqtt://192.168.100.1")
    , m_config_topic("hmi/config") {
}

SettingsUI::~SettingsUI() {
}

void SettingsUI::init(lv_obj_t* parent_screen) {
    createGearIcon(parent_screen);
    loadSettings();
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
    // Create tabview directly on screen (no container)
    m_tabview = lv_tabview_create(lv_screen_active());
    lv_obj_set_size(m_tabview, LV_PCT(100), LV_PCT(100));
    lv_obj_align(m_tabview, LV_ALIGN_CENTER, 0, 0);
    
    // Keep on top - move to foreground
    lv_obj_move_foreground(m_tabview);
    lv_obj_add_flag(m_tabview, LV_OBJ_FLAG_FLOATING);
    
    // Create tabs
    lv_obj_t* mqtt_tab = lv_tabview_add_tab(m_tabview, "MQTT");
    lv_obj_t* lan_tab = lv_tabview_add_tab(m_tabview, "LAN");
    lv_obj_t* wifi_tab = lv_tabview_add_tab(m_tabview, "WiFi");
    lv_obj_t* about_tab = lv_tabview_add_tab(m_tabview, "About");
    lv_obj_t* close_tab = lv_tabview_add_tab(m_tabview, "Close");
    (void)close_tab;  // Intentionally empty tab
    
    // Populate tabs
    createMqttTab(mqtt_tab);
    createLanTab(lan_tab);
    createWifiTab(wifi_tab);
    createAboutTab(about_tab);
    
    // Add event callback to tabview for tab changes
    lv_obj_add_event_cb(m_tabview, tab_changed_cb, LV_EVENT_VALUE_CHANGED, this);
    
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
        m_lan_dhcp_switch = nullptr;
        m_lan_ip_input = nullptr;
        m_lan_netmask_input = nullptr;
        m_lan_gateway_input = nullptr;
        m_lan_status_label = nullptr;
        m_wifi_list = nullptr;
        m_wifi_ssid_input = nullptr;
        m_wifi_password_input = nullptr;
        m_wifi_status_label = nullptr;
        ESP_LOGI(TAG, "Settings screen destroyed");
    }
}

void SettingsUI::createMqttTab(lv_obj_t* tab) {
    int y_pos = 20;
    int field_height = 40;
    int field_spacing = 60;
    
    // Broker URI
    lv_obj_t* broker_label = lv_label_create(tab);
    lv_label_set_text(broker_label, "Broker URI:");
    lv_obj_set_pos(broker_label, 20, y_pos);
    
    m_broker_input = lv_textarea_create(tab);
    lv_obj_set_size(m_broker_input, LV_PCT(65), field_height);
    lv_obj_set_pos(m_broker_input, LV_PCT(35), y_pos);
    lv_textarea_set_one_line(m_broker_input, true);
    lv_textarea_set_text(m_broker_input, m_broker_uri.c_str());
    lv_obj_add_event_cb(m_broker_input, textarea_focused_cb, LV_EVENT_FOCUSED, this);
    lv_obj_add_event_cb(m_broker_input, textarea_defocused_cb, LV_EVENT_DEFOCUSED, this);
    
    y_pos += field_spacing;
    
    // Username
    lv_obj_t* username_label = lv_label_create(tab);
    lv_label_set_text(username_label, "Username:");
    lv_obj_set_pos(username_label, 20, y_pos);
    
    m_username_input = lv_textarea_create(tab);
    lv_obj_set_size(m_username_input, LV_PCT(65), field_height);
    lv_obj_set_pos(m_username_input, LV_PCT(35), y_pos);
    lv_textarea_set_one_line(m_username_input, true);
    lv_textarea_set_text(m_username_input, m_username.c_str());
    lv_obj_add_event_cb(m_username_input, textarea_focused_cb, LV_EVENT_FOCUSED, this);
    lv_obj_add_event_cb(m_username_input, textarea_defocused_cb, LV_EVENT_DEFOCUSED, this);
    
    y_pos += field_spacing;
    
    // Password
    lv_obj_t* password_label = lv_label_create(tab);
    lv_label_set_text(password_label, "Password:");
    lv_obj_set_pos(password_label, 20, y_pos);
    
    m_password_input = lv_textarea_create(tab);
    lv_obj_set_size(m_password_input, LV_PCT(65), field_height);
    lv_obj_set_pos(m_password_input, LV_PCT(35), y_pos);
    lv_textarea_set_one_line(m_password_input, true);
    lv_textarea_set_password_mode(m_password_input, true);
    lv_textarea_set_text(m_password_input, m_password.c_str());
    lv_obj_add_event_cb(m_password_input, textarea_focused_cb, LV_EVENT_FOCUSED, this);
    lv_obj_add_event_cb(m_password_input, textarea_defocused_cb, LV_EVENT_DEFOCUSED, this);
    
    y_pos += field_spacing;
    
    // Client ID
    lv_obj_t* client_id_label = lv_label_create(tab);
    lv_label_set_text(client_id_label, "Client ID:");
    lv_obj_set_pos(client_id_label, 20, y_pos);
    
    m_client_id_input = lv_textarea_create(tab);
    lv_obj_set_size(m_client_id_input, LV_PCT(65), field_height);
    lv_obj_set_pos(m_client_id_input, LV_PCT(35), y_pos);
    lv_textarea_set_one_line(m_client_id_input, true);
    lv_textarea_set_text(m_client_id_input, m_client_id.c_str());
    lv_obj_add_event_cb(m_client_id_input, textarea_focused_cb, LV_EVENT_FOCUSED, this);
    lv_obj_add_event_cb(m_client_id_input, textarea_defocused_cb, LV_EVENT_DEFOCUSED, this);
    
    y_pos += field_spacing;
    
    // Config Topic
    lv_obj_t* topic_label = lv_label_create(tab);
    lv_label_set_text(topic_label, "Config Topic:");
    lv_obj_set_pos(topic_label, 20, y_pos);
    
    m_config_topic_input = lv_textarea_create(tab);
    lv_obj_set_size(m_config_topic_input, LV_PCT(65), field_height);
    lv_obj_set_pos(m_config_topic_input, LV_PCT(35), y_pos);
    lv_textarea_set_one_line(m_config_topic_input, true);
    lv_textarea_set_text(m_config_topic_input, m_config_topic.c_str());
    lv_obj_add_event_cb(m_config_topic_input, textarea_focused_cb, LV_EVENT_FOCUSED, this);
    lv_obj_add_event_cb(m_config_topic_input, textarea_defocused_cb, LV_EVENT_DEFOCUSED, this);
    
    y_pos += field_spacing + 20;
    
    // Save button
    lv_obj_t* save_btn = lv_button_create(tab);
    lv_obj_set_size(save_btn, 150, 50);
    lv_obj_set_pos(save_btn, 20, y_pos);
    lv_obj_add_event_cb(save_btn, mqtt_save_clicked_cb, LV_EVENT_CLICKED, this);
    
    lv_obj_t* save_label = lv_label_create(save_btn);
    lv_label_set_text(save_label, "Save & Apply");
    lv_obj_center(save_label);
    
    ESP_LOGI(TAG, "MQTT tab created");
}

void SettingsUI::createLanTab(lv_obj_t* tab) {
    int y_pos = 20;
    int field_height = 40;
    
    // DHCP Switch
    lv_obj_t* dhcp_label = lv_label_create(tab);
    lv_label_set_text(dhcp_label, "DHCP:");
    lv_obj_set_pos(dhcp_label, 20, y_pos);
    
    m_lan_dhcp_switch = lv_switch_create(tab);
    lv_obj_set_pos(m_lan_dhcp_switch, 150, y_pos);
    lv_obj_add_state(m_lan_dhcp_switch, LV_STATE_CHECKED);  // Default to DHCP
    lv_obj_add_event_cb(m_lan_dhcp_switch, lan_dhcp_switch_cb, LV_EVENT_VALUE_CHANGED, this);
    
    y_pos += 60;
    
    // Static IP fields
    lv_obj_t* ip_label = lv_label_create(tab);
    lv_label_set_text(ip_label, "IP Address:");
    lv_obj_set_pos(ip_label, 20, y_pos);
    
    m_lan_ip_input = lv_textarea_create(tab);
    lv_obj_set_size(m_lan_ip_input, LV_PCT(60), field_height);
    lv_obj_set_pos(m_lan_ip_input, LV_PCT(40), y_pos);
    lv_textarea_set_one_line(m_lan_ip_input, true);
    lv_textarea_set_text(m_lan_ip_input, "");
    lv_textarea_set_accepted_chars(m_lan_ip_input, "0123456789.");
    lv_obj_add_event_cb(m_lan_ip_input, textarea_focused_cb, LV_EVENT_FOCUSED, this);
    lv_obj_add_event_cb(m_lan_ip_input, textarea_defocused_cb, LV_EVENT_DEFOCUSED, this);
    lv_obj_add_state(m_lan_ip_input, LV_STATE_DISABLED);  // Disabled by default (DHCP on)
    
    y_pos += 60;
    
    // Netmask
    lv_obj_t* netmask_label = lv_label_create(tab);
    lv_label_set_text(netmask_label, "Netmask:");
    lv_obj_set_pos(netmask_label, 20, y_pos);
    
    m_lan_netmask_input = lv_textarea_create(tab);
    lv_obj_set_size(m_lan_netmask_input, LV_PCT(60), field_height);
    lv_obj_set_pos(m_lan_netmask_input, LV_PCT(40), y_pos);
    lv_textarea_set_one_line(m_lan_netmask_input, true);
    lv_textarea_set_text(m_lan_netmask_input, "255.255.255.0");
    lv_textarea_set_accepted_chars(m_lan_netmask_input, "0123456789.");
    lv_obj_add_event_cb(m_lan_netmask_input, textarea_focused_cb, LV_EVENT_FOCUSED, this);
    lv_obj_add_event_cb(m_lan_netmask_input, textarea_defocused_cb, LV_EVENT_DEFOCUSED, this);
    lv_obj_add_state(m_lan_netmask_input, LV_STATE_DISABLED);
    
    y_pos += 60;
    
    // Gateway
    lv_obj_t* gateway_label = lv_label_create(tab);
    lv_label_set_text(gateway_label, "Gateway:");
    lv_obj_set_pos(gateway_label, 20, y_pos);
    
    m_lan_gateway_input = lv_textarea_create(tab);
    lv_obj_set_size(m_lan_gateway_input, LV_PCT(60), field_height);
    lv_obj_set_pos(m_lan_gateway_input, LV_PCT(40), y_pos);
    lv_textarea_set_one_line(m_lan_gateway_input, true);
    lv_textarea_set_text(m_lan_gateway_input, "");
    lv_textarea_set_accepted_chars(m_lan_gateway_input, "0123456789.");
    lv_obj_add_event_cb(m_lan_gateway_input, textarea_focused_cb, LV_EVENT_FOCUSED, this);
    lv_obj_add_event_cb(m_lan_gateway_input, textarea_defocused_cb, LV_EVENT_DEFOCUSED, this);
    lv_obj_add_state(m_lan_gateway_input, LV_STATE_DISABLED);
    
    y_pos += 80;
    
    // Status label
    m_lan_status_label = lv_label_create(tab);
    lv_label_set_text(m_lan_status_label, "Status: Checking...");
    lv_obj_set_pos(m_lan_status_label, 20, y_pos);
    lv_obj_set_style_text_color(m_lan_status_label, lv_color_hex(0xFFFFFF), 0);
    
    // Update status from LanManager
    LanManager& lan = LanManager::getInstance();
    if (lan.isConnected()) {
        updateEthStatus(true, lan.getIpAddress());
    } else {
        updateEthStatus(false, "");
    }
    
    y_pos += 60;
    
    // Apply button
    lv_obj_t* apply_btn = lv_button_create(tab);
    lv_obj_set_size(apply_btn, 150, 50);
    lv_obj_set_pos(apply_btn, 20, y_pos);
    lv_obj_add_event_cb(apply_btn, lan_save_clicked_cb, LV_EVENT_CLICKED, this);
    
    lv_obj_t* apply_label = lv_label_create(apply_btn);
    lv_label_set_text(apply_label, "Apply");
    lv_obj_center(apply_label);
    
    ESP_LOGI(TAG, "LAN tab created");
}

void SettingsUI::createWifiTab(lv_obj_t* tab) {
    int y_pos = 20;
    
    // Scan button
    lv_obj_t* scan_btn = lv_button_create(tab);
    lv_obj_set_size(scan_btn, 150, 50);
    lv_obj_set_pos(scan_btn, 20, y_pos);
    lv_obj_add_event_cb(scan_btn, wifi_scan_clicked_cb, LV_EVENT_CLICKED, this);
    
    lv_obj_t* scan_label = lv_label_create(scan_btn);
    lv_label_set_text(scan_label, LV_SYMBOL_REFRESH " Scan");
    lv_obj_center(scan_label);
    
    y_pos += 70;
    
    // WiFi table (SSID, Signal dB, Security)
    m_wifi_list = lv_table_create(tab);
    lv_obj_set_size(m_wifi_list, LV_PCT(95), 200);
    lv_obj_set_pos(m_wifi_list, 10, y_pos);
    
    // Set up 3 columns
    lv_table_set_column_count(m_wifi_list, 3);
    lv_table_set_column_width(m_wifi_list, 0, 450);  // SSID
    lv_table_set_column_width(m_wifi_list, 1, 100);  // Signal
    lv_table_set_column_width(m_wifi_list, 2, 130);  // Security
    
    // Add header row
    lv_table_set_cell_value(m_wifi_list, 0, 0, "SSID");
    lv_table_set_cell_value(m_wifi_list, 0, 1, "Signal (dB)");
    lv_table_set_cell_value(m_wifi_list, 0, 2, "Security");
    
    // Add event callback for table cell clicks
    lv_obj_add_event_cb(m_wifi_list, wifi_ap_clicked_cb, LV_EVENT_PRESSED, this);
    
    y_pos += 220;
    
    // SSID input
    lv_obj_t* ssid_label = lv_label_create(tab);
    lv_label_set_text(ssid_label, "SSID:");
    lv_obj_set_pos(ssid_label, 20, y_pos);
    
    m_wifi_ssid_input = lv_textarea_create(tab);
    lv_obj_set_size(m_wifi_ssid_input, LV_PCT(60), 40);
    lv_obj_set_pos(m_wifi_ssid_input, LV_PCT(40), y_pos);
    lv_textarea_set_one_line(m_wifi_ssid_input, true);
    lv_textarea_set_placeholder_text(m_wifi_ssid_input, "Select AP or type SSID");
    lv_obj_add_event_cb(m_wifi_ssid_input, textarea_focused_cb, LV_EVENT_FOCUSED, this);
    lv_obj_add_event_cb(m_wifi_ssid_input, textarea_defocused_cb, LV_EVENT_DEFOCUSED, this);
    
    y_pos += 60;
    
    // Password input
    lv_obj_t* password_label = lv_label_create(tab);
    lv_label_set_text(password_label, "Password:");
    lv_obj_set_pos(password_label, 20, y_pos);
    
    m_wifi_password_input = lv_textarea_create(tab);
    lv_obj_set_size(m_wifi_password_input, LV_PCT(60), 40);
    lv_obj_set_pos(m_wifi_password_input, LV_PCT(40), y_pos);
    lv_textarea_set_one_line(m_wifi_password_input, true);
    lv_textarea_set_password_mode(m_wifi_password_input, true);
    lv_obj_add_event_cb(m_wifi_password_input, textarea_focused_cb, LV_EVENT_FOCUSED, this);
    lv_obj_add_event_cb(m_wifi_password_input, textarea_defocused_cb, LV_EVENT_DEFOCUSED, this);
    
    y_pos += 60;
    
    // Connect button
    lv_obj_t* connect_btn = lv_button_create(tab);
    lv_obj_set_size(connect_btn, 150, 50);
    lv_obj_set_pos(connect_btn, 20, y_pos);
    lv_obj_add_event_cb(connect_btn, wifi_connect_clicked_cb, LV_EVENT_CLICKED, this);
    
    lv_obj_t* connect_label = lv_label_create(connect_btn);
    lv_label_set_text(connect_label, "Connect");
    lv_obj_center(connect_label);
    
    y_pos += 60;
    
    // Status label
    m_wifi_status_label = lv_label_create(tab);
    lv_label_set_text(m_wifi_status_label, "Status: Not connected");
    lv_obj_set_pos(m_wifi_status_label, 20, y_pos);
    lv_obj_set_style_text_color(m_wifi_status_label, lv_color_hex(0xFFFFFF), 0);
    
    // Update status from WirelessManager
    WirelessManager& wifi = WirelessManager::getInstance();
    if (wifi.isConnected()) {
        updateWifiStatus(true, wifi.getCurrentSsid(), wifi.getIpAddress());
    } else {
        updateWifiStatus(false, "", "");
    }
    
    ESP_LOGI(TAG, "WiFi tab created");
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
    if (!m_visible) {
        createSettingsScreen();
        m_visible = true;
    }
}

void SettingsUI::hide() {
    if (m_visible) {
        destroySettingsScreen();
        m_visible = false;
    }
}

void SettingsUI::bringToFront() {
    if (m_gear_icon) {
        lv_obj_move_foreground(m_gear_icon);
    }
    // Also bring settings screen to front if visible
    if (m_visible && m_tabview) {
        lv_obj_move_foreground(m_tabview);
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
    
    // Tab indices: 0=MQTT, 1=LAN, 2=WiFi, 3=About, 4=Close
    if (tab_idx == 4) {
        ESP_LOGI(TAG, "Close tab selected, hiding settings");
        ui->hide();
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
    LanManager& lan = LanManager::getInstance();
    
    if (dhcp_enabled) {
        // Apply DHCP
        lan.setIpConfig(EthIpConfigMode::DHCP, nullptr);
        ESP_LOGI(TAG, "LAN configured for DHCP");
    } else {
        // Apply static IP
        EthStaticIpConfig config;
        config.ip = lv_textarea_get_text(ui->m_lan_ip_input);
        config.netmask = lv_textarea_get_text(ui->m_lan_netmask_input);
        config.gateway = lv_textarea_get_text(ui->m_lan_gateway_input);
        
        lan.setIpConfig(EthIpConfigMode::STATIC, &config);
        ESP_LOGI(TAG, "LAN configured for static IP: %s", config.ip.c_str());
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
        lv_label_set_text(ui->m_wifi_status_label, "Status: Enter SSID first");
        return;
    }
    
    std::string password = lv_textarea_get_text(ui->m_wifi_password_input);
    
    lv_label_set_text(ui->m_wifi_status_label, "Status: Connecting...");
    lv_label_set_text_fmt(ui->m_wifi_status_label, "Connecting to %s...", ssid.c_str());
    
    WirelessManager& wifi = WirelessManager::getInstance();
    esp_err_t err = wifi.connect(ssid, password, 15000);
    
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "WiFi connected successfully");
        ui->updateWifiStatus(true, ssid, wifi.getIpAddress());
    } else {
        ESP_LOGE(TAG, "WiFi connection failed: %s", esp_err_to_name(err));
        lv_label_set_text(ui->m_wifi_status_label, "Status: Connection failed");
    }
}

void SettingsUI::textarea_focused_cb(lv_event_t* e) {
    SettingsUI* ui = static_cast<SettingsUI*>(lv_event_get_user_data(e));
    lv_obj_t* textarea = (lv_obj_t*)lv_event_get_target(e);
    
    if (ui->m_keyboard) {
        lv_keyboard_set_textarea(ui->m_keyboard, textarea);
        
        // Set keyboard mode based on textarea type
        if (textarea == ui->m_lan_ip_input || 
            textarea == ui->m_lan_netmask_input || 
            textarea == ui->m_lan_gateway_input) {
            // Use number mode for IP address fields
            lv_keyboard_set_mode(ui->m_keyboard, LV_KEYBOARD_MODE_NUMBER);
            ESP_LOGD(TAG, "Keyboard mode set to NUMBER for IP field");
        } else {
            // Use text mode for other fields
            lv_keyboard_set_mode(ui->m_keyboard, LV_KEYBOARD_MODE_TEXT_LOWER);
            ESP_LOGD(TAG, "Keyboard mode set to TEXT");
        }
        
        lv_obj_clear_flag(ui->m_keyboard, LV_OBJ_FLAG_HIDDEN);
        
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
        m_keyboard = lv_keyboard_create(lv_screen_active());
        lv_obj_set_size(m_keyboard, LV_PCT(100), LV_PCT(40));
        lv_obj_align(m_keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_add_flag(m_keyboard, LV_OBJ_FLAG_HIDDEN);  // Start hidden
        
        // Add callback for when Enter is pressed on keyboard
        lv_obj_add_event_cb(m_keyboard, keyboard_ready_cb, LV_EVENT_READY, this);
        
        ESP_LOGI(TAG, "Keyboard created");
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
    if (m_wifi_status_label) {
        lv_label_set_text(m_wifi_status_label, "Scanning...");
        lv_obj_set_style_text_color(m_wifi_status_label, lv_color_hex(0xFFAA00), 0);
    }
    
    WirelessManager& wifi = WirelessManager::getInstance();
    
    // Use async scan to avoid blocking HMI task
    wifi.scanAsync([this](const std::vector<WifiNetworkInfo>& networks, esp_err_t err) {
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Found %d WiFi networks", networks.size());
            
            // Convert to WifiAP structure
            std::vector<WifiAP> aps;
            for (const auto& net : networks) {
                WifiAP ap;
                ap.ssid = net.ssid;
                ap.rssi = net.rssi;
                ap.requires_password = (net.auth_mode != WIFI_AUTH_OPEN);
                aps.push_back(ap);
            }
            
            // Update UI from LVGL task context
            updateWifiScanResults(aps);
            
            if (m_wifi_status_label) {
                lv_label_set_text(m_wifi_status_label, "Scan complete");
                lv_obj_set_style_text_color(m_wifi_status_label, lv_color_hex(0x00FF00), 0);
            }
        } else {
            ESP_LOGE(TAG, "WiFi scan failed: %s", esp_err_to_name(err));
            
            if (m_wifi_status_label) {
                lv_label_set_text(m_wifi_status_label, "Scan failed");
                lv_obj_set_style_text_color(m_wifi_status_label, lv_color_hex(0xFF0000), 0);
            }
        }
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

void SettingsUI::updateEthStatus(bool connected, const std::string& ip) {
    if (!m_lan_status_label) return;
    
    if (connected && !ip.empty()) {
        char status[128];
        snprintf(status, sizeof(status), "Status: Connected - %s", ip.c_str());
        lv_label_set_text(m_lan_status_label, status);
        lv_obj_set_style_text_color(m_lan_status_label, lv_color_hex(0x00FF00), 0);
    } else {
        lv_label_set_text(m_lan_status_label, "Status: Disconnected");
        lv_obj_set_style_text_color(m_lan_status_label, lv_color_hex(0xFF0000), 0);
    }
}

void SettingsUI::updateWifiStatus(bool connected, const std::string& ssid, const std::string& ip) {
    if (!m_wifi_status_label) return;
    
    if (connected && !ssid.empty() && !ip.empty()) {
        char status[128];
        snprintf(status, sizeof(status), "Connected: %s - %s", ssid.c_str(), ip.c_str());
        lv_label_set_text(m_wifi_status_label, status);
        lv_obj_set_style_text_color(m_wifi_status_label, lv_color_hex(0x00FF00), 0);
    } else {
        lv_label_set_text(m_wifi_status_label, "Status: Not connected");
        lv_obj_set_style_text_color(m_wifi_status_label, lv_color_hex(0xFF0000), 0);
    }
}
