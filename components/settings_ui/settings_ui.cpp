#include "settings_ui.h"
#include "mqtt_manager.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"

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
    , m_broker_input(nullptr)
    , m_port_input(nullptr)
    , m_username_input(nullptr)
    , m_password_input(nullptr)
    , m_client_id_input(nullptr)
    , m_config_topic_input(nullptr)
    , m_keyboard(nullptr)
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
    // Create gear button in top-right corner
    m_gear_icon = lv_button_create(parent);
    lv_obj_set_size(m_gear_icon, 50, 50);
    lv_obj_align(m_gear_icon, LV_ALIGN_TOP_RIGHT, -10, 10);
    
    // Add gear symbol
    lv_obj_t* label = lv_label_create(m_gear_icon);
    lv_label_set_text(label, LV_SYMBOL_SETTINGS);
    lv_obj_center(label);
    
    lv_obj_add_event_cb(m_gear_icon, gear_clicked_cb, LV_EVENT_CLICKED, this);
    
    ESP_LOGI(TAG, "Gear icon created");
}

void SettingsUI::createSettingsScreen() {
    // Create modal overlay
    m_settings_screen = lv_obj_create(lv_screen_active());
    lv_obj_set_size(m_settings_screen, LV_PCT(80), LV_PCT(80));
    lv_obj_center(m_settings_screen);
    lv_obj_set_style_bg_color(m_settings_screen, lv_color_hex(0x2C3E50), 0);
    lv_obj_set_style_border_color(m_settings_screen, lv_color_hex(0x3498DB), 0);
    lv_obj_set_style_border_width(m_settings_screen, 3, 0);
    
    // Title
    lv_obj_t* title = lv_label_create(m_settings_screen);
    lv_label_set_text(title, "MQTT Settings");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    
    int y_pos = 60;
    int field_height = 40;
    int field_spacing = 60;
    
    // Broker URI
    lv_obj_t* broker_label = lv_label_create(m_settings_screen);
    lv_label_set_text(broker_label, "Broker URI:");
    lv_obj_align(broker_label, LV_ALIGN_TOP_LEFT, 20, y_pos);
    
    m_broker_input = lv_textarea_create(m_settings_screen);
    lv_obj_set_size(m_broker_input, LV_PCT(70), field_height);
    lv_obj_align(m_broker_input, LV_ALIGN_TOP_RIGHT, -20, y_pos);
    lv_textarea_set_one_line(m_broker_input, true);
    lv_textarea_set_text(m_broker_input, m_broker_uri.c_str());
    lv_obj_add_event_cb(m_broker_input, textarea_focused_cb, LV_EVENT_FOCUSED, this);
    
    y_pos += field_spacing;
    
    // Username
    lv_obj_t* username_label = lv_label_create(m_settings_screen);
    lv_label_set_text(username_label, "Username:");
    lv_obj_align(username_label, LV_ALIGN_TOP_LEFT, 20, y_pos);
    
    m_username_input = lv_textarea_create(m_settings_screen);
    lv_obj_set_size(m_username_input, LV_PCT(70), field_height);
    lv_obj_align(m_username_input, LV_ALIGN_TOP_RIGHT, -20, y_pos);
    lv_textarea_set_one_line(m_username_input, true);
    lv_textarea_set_text(m_username_input, m_username.c_str());
    lv_obj_add_event_cb(m_username_input, textarea_focused_cb, LV_EVENT_FOCUSED, this);
    
    y_pos += field_spacing;
    
    // Password
    lv_obj_t* password_label = lv_label_create(m_settings_screen);
    lv_label_set_text(password_label, "Password:");
    lv_obj_align(password_label, LV_ALIGN_TOP_LEFT, 20, y_pos);
    
    m_password_input = lv_textarea_create(m_settings_screen);
    lv_obj_set_size(m_password_input, LV_PCT(70), field_height);
    lv_obj_align(m_password_input, LV_ALIGN_TOP_RIGHT, -20, y_pos);
    lv_textarea_set_one_line(m_password_input, true);
    lv_textarea_set_password_mode(m_password_input, true);
    lv_textarea_set_text(m_password_input, m_password.c_str());
    lv_obj_add_event_cb(m_password_input, textarea_focused_cb, LV_EVENT_FOCUSED, this);
    
    y_pos += field_spacing;
    
    // Client ID
    lv_obj_t* client_id_label = lv_label_create(m_settings_screen);
    lv_label_set_text(client_id_label, "Client ID:");
    lv_obj_align(client_id_label, LV_ALIGN_TOP_LEFT, 20, y_pos);
    
    m_client_id_input = lv_textarea_create(m_settings_screen);
    lv_obj_set_size(m_client_id_input, LV_PCT(70), field_height);
    lv_obj_align(m_client_id_input, LV_ALIGN_TOP_RIGHT, -20, y_pos);
    lv_textarea_set_one_line(m_client_id_input, true);
    lv_textarea_set_text(m_client_id_input, m_client_id.c_str());
    lv_obj_add_event_cb(m_client_id_input, textarea_focused_cb, LV_EVENT_FOCUSED, this);
    
    y_pos += field_spacing;
    
    // Config Topic
    lv_obj_t* topic_label = lv_label_create(m_settings_screen);
    lv_label_set_text(topic_label, "Config Topic:");
    lv_obj_align(topic_label, LV_ALIGN_TOP_LEFT, 20, y_pos);
    
    m_config_topic_input = lv_textarea_create(m_settings_screen);
    lv_obj_set_size(m_config_topic_input, LV_PCT(70), field_height);
    lv_obj_align(m_config_topic_input, LV_ALIGN_TOP_RIGHT, -20, y_pos);
    lv_textarea_set_one_line(m_config_topic_input, true);
    lv_textarea_set_text(m_config_topic_input, m_config_topic.c_str());
    lv_obj_add_event_cb(m_config_topic_input, textarea_focused_cb, LV_EVENT_FOCUSED, this);
    
    // Buttons at bottom
    lv_obj_t* save_btn = lv_button_create(m_settings_screen);
    lv_obj_set_size(save_btn, 120, 50);
    lv_obj_align(save_btn, LV_ALIGN_BOTTOM_LEFT, 30, -20);
    lv_obj_add_event_cb(save_btn, button_clicked_cb, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(save_btn, save_clicked_cb, LV_EVENT_CLICKED, this);
    
    lv_obj_t* save_label = lv_label_create(save_btn);
    lv_label_set_text(save_label, "Save");
    lv_obj_center(save_label);
    
    lv_obj_t* cancel_btn = lv_button_create(m_settings_screen);
    lv_obj_set_size(cancel_btn, 120, 50);
    lv_obj_align(cancel_btn, LV_ALIGN_BOTTOM_RIGHT, -30, -20);
    lv_obj_add_event_cb(cancel_btn, button_clicked_cb, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(cancel_btn, cancel_clicked_cb, LV_EVENT_CLICKED, this);
    
    lv_obj_t* cancel_label = lv_label_create(cancel_btn);
    lv_label_set_text(cancel_label, "Cancel");
    lv_obj_center(cancel_label);
    
    // Create keyboard
    createKeyboard();
    
    ESP_LOGI(TAG, "Settings screen created");
}

void SettingsUI::destroySettingsScreen() {
    if (m_settings_screen) {
        destroyKeyboard();
        lv_obj_delete(m_settings_screen);
        m_settings_screen = nullptr;
        m_broker_input = nullptr;
        m_port_input = nullptr;
        m_username_input = nullptr;
        m_password_input = nullptr;
        m_client_id_input = nullptr;
        m_config_topic_input = nullptr;
        ESP_LOGI(TAG, "Settings screen destroyed");
    }
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
}

void SettingsUI::gear_clicked_cb(lv_event_t* e) {
    SettingsUI* ui = static_cast<SettingsUI*>(lv_event_get_user_data(e));
    ui->show();
}

void SettingsUI::save_clicked_cb(lv_event_t* e) {
    SettingsUI* ui = static_cast<SettingsUI*>(lv_event_get_user_data(e));
    
    // Read values from inputs
    ui->m_broker_uri = lv_textarea_get_text(ui->m_broker_input);
    ui->m_username = lv_textarea_get_text(ui->m_username_input);
    ui->m_password = lv_textarea_get_text(ui->m_password_input);
    ui->m_client_id = lv_textarea_get_text(ui->m_client_id_input);
    ui->m_config_topic = lv_textarea_get_text(ui->m_config_topic_input);
    
    // Save to NVS
    if (ui->saveSettings()) {
        ESP_LOGI(TAG, "Settings saved, reconnecting to MQTT...");
        
        // Reconnect MQTT with new settings
        MQTTManager::getInstance().deinit();
        
        if (!ui->m_username.empty()) {
            MQTTManager::getInstance().init(ui->m_broker_uri, ui->m_username, 
                                            ui->m_password, ui->m_client_id);
        } else {
            MQTTManager::getInstance().init(ui->m_broker_uri, ui->m_client_id);
        }
    }
    
    ui->hide();
}

void SettingsUI::cancel_clicked_cb(lv_event_t* e) {
    SettingsUI* ui = static_cast<SettingsUI*>(lv_event_get_user_data(e));
    ui->hide();
}

void SettingsUI::textarea_focused_cb(lv_event_t* e) {
    SettingsUI* ui = static_cast<SettingsUI*>(lv_event_get_user_data(e));
    lv_obj_t* textarea = (lv_obj_t*)lv_event_get_target(e);
    
    if (ui->m_keyboard) {
        lv_keyboard_set_textarea(ui->m_keyboard, textarea);
        lv_obj_clear_flag(ui->m_keyboard, LV_OBJ_FLAG_HIDDEN);
        
        // Move settings panel up to keep textarea visible above keyboard
        // Keyboard is 40% of screen (600px * 0.4 = 240px)
        // Move panel up by half the keyboard height to ensure visibility
        lv_obj_align(ui->m_settings_screen, LV_ALIGN_TOP_MID, 0, 10);
        
        ESP_LOGD(TAG, "Keyboard shown for textarea");
    }
}

void SettingsUI::button_clicked_cb(lv_event_t* e) {
    SettingsUI* ui = static_cast<SettingsUI*>(lv_event_get_user_data(e));
    
    // Hide keyboard when any button is clicked
    if (ui->m_keyboard) {
        lv_obj_add_flag(ui->m_keyboard, LV_OBJ_FLAG_HIDDEN);
        // Restore settings panel to center position
        lv_obj_center(ui->m_settings_screen);
        ESP_LOGD(TAG, "Keyboard hidden");
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
