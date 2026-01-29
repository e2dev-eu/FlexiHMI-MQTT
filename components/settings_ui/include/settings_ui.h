#pragma once

#include "lvgl.h"
#include <string>

class SettingsUI {
public:
    static SettingsUI& getInstance();
    
    // Initialize settings UI (creates gear icon)
    void init(lv_obj_t* parent_screen);
    
    // Show/hide settings screen
    void show();
    void hide();
    
    // Check if settings screen is visible
    bool isVisible() const { return m_visible; }
    
    // Load settings from NVS
    bool loadSettings();
    
    // Save settings to NVS
    bool saveSettings();
    
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
    
    static void gear_clicked_cb(lv_event_t* e);
    static void save_clicked_cb(lv_event_t* e);
    static void cancel_clicked_cb(lv_event_t* e);
    static void textarea_focused_cb(lv_event_t* e);
    static void button_clicked_cb(lv_event_t* e);
    static void keyboard_ready_cb(lv_event_t* e);
    
    void createKeyboard();
    void destroyKeyboard();
    
    lv_obj_t* m_gear_icon;
    lv_obj_t* m_settings_screen;
    lv_obj_t* m_broker_input;
    lv_obj_t* m_port_input;
    lv_obj_t* m_username_input;
    lv_obj_t* m_password_input;
    lv_obj_t* m_client_id_input;
    lv_obj_t* m_config_topic_input;
    lv_obj_t* m_keyboard;
    
    bool m_visible;
    
    // Settings values
    std::string m_broker_uri;
    std::string m_username;
    std::string m_password;
    std::string m_client_id;
    std::string m_config_topic;
};
