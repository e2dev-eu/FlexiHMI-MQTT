#include "config_manager.h"
#include "mqtt_manager.h"
#include "label_widget.h"
#include "button_widget.h"
#include "container_widget.h"
#include "switch_widget.h"
#include "slider_widget.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"

// C wrapper functions for bringing UI elements to front
extern "C" {
    void settings_ui_bring_to_front();
    void status_info_bring_to_front();
}

static const char* TAG = "ConfigManager";
static const char* NVS_NAMESPACE = "hmi_config";
static const char* NVS_KEY_CONFIG = "json_config";
static const char* NVS_KEY_VERSION = "version";

ConfigManager& ConfigManager::getInstance() {
    static ConfigManager instance;
    return instance;
}

ConfigManager::ConfigManager() : m_current_version(0), m_has_pending_config(false) {
    m_config_mutex = xSemaphoreCreateMutex();
}

ConfigManager::~ConfigManager() {
    destroyAllWidgets();
    if (m_config_mutex) {
        vSemaphoreDelete(m_config_mutex);
    }
}

bool ConfigManager::parseAndApply(const std::string& json_config) {
    if (json_config.empty()) {
        ESP_LOGE(TAG, "Empty configuration");
        return false;
    }
    
    ESP_LOGI(TAG, "Parsing JSON configuration, length: %d bytes", json_config.length());
    ESP_LOGI(TAG, "JSON content: %s", json_config.c_str());
    
    cJSON* root = cJSON_Parse(json_config.c_str());
    if (!root) {
        const char* error_ptr = cJSON_GetErrorPtr();
        ESP_LOGE(TAG, "Failed to parse JSON!");
        ESP_LOGE(TAG, "Error position: %s", error_ptr ? error_ptr : "unknown");
        ESP_LOGE(TAG, "First 100 chars: %.100s", json_config.c_str());
        return false;
    }
    
    ESP_LOGI(TAG, "JSON parsed successfully");
    
    // Extract version (optional, for logging only)
    cJSON* version_item = cJSON_GetObjectItem(root, "version");
    int new_version = 0;
    if (version_item && cJSON_IsNumber(version_item)) {
        new_version = version_item->valueint;
        ESP_LOGI(TAG, "Configuration version: %d", new_version);
    } else {
        ESP_LOGI(TAG, "No version field, applying configuration anyway");
    }
    
    // Always apply new configuration
    ESP_LOGI(TAG, "Applying new configuration...");
    
    // Destroy existing widgets
    destroyAllWidgets();
    
    // Parse widgets array
    cJSON* widgets_array = cJSON_GetObjectItem(root, "widgets");
    if (!widgets_array) {
        ESP_LOGE(TAG, "Missing 'widgets' array in JSON root");
        cJSON_Delete(root);
        return false;
    }
    if (!cJSON_IsArray(widgets_array)) {
        ESP_LOGE(TAG, "'widgets' field is not an array (type: %d)", widgets_array->type);
        cJSON_Delete(root);
        return false;
    }
    
    int widget_count = cJSON_GetArraySize(widgets_array);
    ESP_LOGI(TAG, "Found %d widgets in configuration", widget_count);
    
    bool success = parseWidgets(widgets_array);
    
    if (success) {
        m_current_version = new_version;
        saveCachedConfig(json_config);
        ESP_LOGI(TAG, "Configuration applied successfully, %d widgets created", m_active_widgets.size());
        
        // Bring settings and info icons to foreground so they're always on top
        settings_ui_bring_to_front();
        status_info_bring_to_front();
    } else {
        ESP_LOGE(TAG, "Failed to apply configuration");
    }
    
    cJSON_Delete(root);
    return success;
}

void ConfigManager::queueConfig(const std::string& json_config) {
    if (xSemaphoreTake(m_config_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        m_pending_config = json_config;
        m_has_pending_config = true;
        ESP_LOGI(TAG, "Config queued for application by HMI task");
        xSemaphoreGive(m_config_mutex);
    } else {
        ESP_LOGW(TAG, "Failed to queue config - mutex timeout");
    }
}

void ConfigManager::processPendingConfig() {
    if (!m_has_pending_config) {
        return; // No pending config
    }
    
    std::string config_to_apply;
    if (xSemaphoreTake(m_config_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (m_has_pending_config) {
            config_to_apply = m_pending_config;
            m_has_pending_config = false;
            m_pending_config.clear();
        }
        xSemaphoreGive(m_config_mutex);
    }
    
    if (!config_to_apply.empty()) {
        ESP_LOGI(TAG, "Processing pending config from HMI task");
        parseAndApply(config_to_apply);
    }
}

bool ConfigManager::parseWidgets(cJSON* widgets_array, lv_obj_t* parent) {
    int array_size = cJSON_GetArraySize(widgets_array);
    ESP_LOGI(TAG, "Parsing %d widgets", array_size);
    
    for (int i = 0; i < array_size; i++) {
        ESP_LOGI(TAG, "Processing widget %d/%d", i+1, array_size);
        cJSON* widget_json = cJSON_GetArrayItem(widgets_array, i);
        if (!widget_json) {
            ESP_LOGE(TAG, "Widget at index %d is NULL", i);
            continue;
        }
        if (!createWidget(widget_json, parent)) {
            ESP_LOGW(TAG, "Failed to create widget at index %d", i);
            // Continue with other widgets instead of failing completely
        }
    }
    
    return true;
}

HMIWidget* ConfigManager::createWidgetByType(const std::string& type) {
    if (type == "label") {
        return new LabelWidget();
    } else if (type == "button") {
        return new ButtonWidget();
    } else if (type == "container") {
        return new ContainerWidget();
    } else if (type == "switch") {
        return new SwitchWidget();
    } else if (type == "slider") {
        return new SliderWidget();
    }
    
    ESP_LOGE(TAG, "Unknown widget type: %s", type.c_str());
    return nullptr;
}

bool ConfigManager::createWidget(cJSON* widget_json, lv_obj_t* parent) {
    if (!widget_json) {
        ESP_LOGE(TAG, "Widget JSON is NULL");
        return false;
    }
    if (!cJSON_IsObject(widget_json)) {
        ESP_LOGE(TAG, "Widget JSON is not an object (type: %d)", widget_json->type);
        return false;
    }
    
    // Log all fields in this widget
    ESP_LOGI(TAG, "Widget fields:");
    cJSON* field = widget_json->child;
    while (field) {
        ESP_LOGI(TAG, "  - %s (type: %d)", field->string ? field->string : "null", field->type);
        field = field->next;
    }
    
    // Extract type
    cJSON* type_item = cJSON_GetObjectItem(widget_json, "type");
    if (!type_item) {
        ESP_LOGE(TAG, "Missing 'type' field in widget");
        return false;
    }
    if (!cJSON_IsString(type_item)) {
        ESP_LOGE(TAG, "'type' field is not a string (type: %d)", type_item->type);
        return false;
    }
    std::string type = type_item->valuestring;
    ESP_LOGI(TAG, "Widget type: %s", type.c_str());
    
    // Extract id
    cJSON* id_item = cJSON_GetObjectItem(widget_json, "id");
    if (!id_item) {
        ESP_LOGE(TAG, "Missing 'id' field in widget");
        return false;
    }
    if (!cJSON_IsString(id_item)) {
        ESP_LOGE(TAG, "'id' field is not a string (type: %d)", id_item->type);
        return false;
    }
    std::string id = id_item->valuestring;
    ESP_LOGI(TAG, "Widget id: %s", id.c_str());
    
    // Extract geometry
    cJSON* geometry = cJSON_GetObjectItem(widget_json, "geometry");
    if (!geometry) {
        ESP_LOGE(TAG, "Missing 'geometry' field for widget '%s'", id.c_str());
        return false;
    }
    if (!cJSON_IsObject(geometry)) {
        ESP_LOGE(TAG, "'geometry' field is not an object (type: %d)", geometry->type);
        return false;
    }
    
    cJSON* x_item = cJSON_GetObjectItem(geometry, "x");
    cJSON* y_item = cJSON_GetObjectItem(geometry, "y");
    cJSON* w_item = cJSON_GetObjectItem(geometry, "width");
    cJSON* h_item = cJSON_GetObjectItem(geometry, "height");
    
    if (!x_item || !cJSON_IsNumber(x_item)) {
        ESP_LOGE(TAG, "Missing or invalid 'x' in geometry");
        return false;
    }
    if (!y_item || !cJSON_IsNumber(y_item)) {
        ESP_LOGE(TAG, "Missing or invalid 'y' in geometry");
        return false;
    }
    if (!w_item || !cJSON_IsNumber(w_item)) {
        ESP_LOGE(TAG, "Missing or invalid 'width' in geometry");
        return false;
    }
    if (!h_item || !cJSON_IsNumber(h_item)) {
        ESP_LOGE(TAG, "Missing or invalid 'height' in geometry");
        return false;
    }
    
    int x = x_item->valueint;
    int y = y_item->valueint;
    int w = w_item->valueint;
    int h = h_item->valueint;
    
    // Get properties object
    cJSON* properties = cJSON_GetObjectItem(widget_json, "properties");
    if (!properties) {
        properties = cJSON_CreateObject();
    }
    
    // Create widget using direct factory instead of registry
    HMIWidget* widget = createWidgetByType(type);
    if (!widget) {
        ESP_LOGE(TAG, "Failed to create widget of type '%s'", type.c_str());
        return false;
    }
    
    // Create the widget on screen (with optional parent)
    if (!widget->create(id, x, y, w, h, properties, parent)) {
        ESP_LOGE(TAG, "Failed to initialize widget '%s'", id.c_str());
        delete widget;
        return false;
    }
    
    // Setup MQTT subscription if needed
    cJSON* mqtt_sub = cJSON_GetObjectItem(widget_json, "mqtt_subscribe");
    if (mqtt_sub && cJSON_IsString(mqtt_sub)) {
        std::string topic = mqtt_sub->valuestring;
        MQTTManager::getInstance().subscribe(topic, 0, 
            [widget](const std::string& topic, const std::string& payload) {
                widget->onMqttMessage(topic, payload);
            });
        ESP_LOGI(TAG, "Widget '%s' subscribed to %s", id.c_str(), topic.c_str());
    }
    
    // Process children if present
    cJSON* children = cJSON_GetObjectItem(widget_json, "children");
    if (children && cJSON_IsArray(children)) {
        int child_count = cJSON_GetArraySize(children);
        ESP_LOGI(TAG, "Widget '%s' has %d children, parsing recursively...", id.c_str(), child_count);
        lv_obj_t* parent_obj = widget->getLvglObject();
        if (parent_obj && lv_obj_is_valid(parent_obj)) {
            ESP_LOGI(TAG, "Parent LVGL object is valid, creating children");
            parseWidgets(children, parent_obj);
        } else {
            ESP_LOGE(TAG, "Widget '%s' has invalid LVGL object, cannot create children", id.c_str());
        }
    }
    
    m_active_widgets.push_back(widget);
    ESP_LOGI(TAG, "Created widget: type=%s, id=%s, pos=(%d,%d), size=(%dx%d)", 
             type.c_str(), id.c_str(), x, y, w, h);
    
    return true;
}

void ConfigManager::destroyAllWidgets() {
    ESP_LOGI(TAG, "Destroying %d widgets", m_active_widgets.size());
    
    for (HMIWidget* widget : m_active_widgets) {
        widget->destroy();
        delete widget;
    }
    
    m_active_widgets.clear();
}

bool ConfigManager::loadCachedConfig() {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "No cached configuration found");
        return false;
    }
    
    // Get version
    int32_t version = 0;
    err = nvs_get_i32(nvs_handle, NVS_KEY_VERSION, &version);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "No cached version found");
        nvs_close(nvs_handle);
        return false;
    }
    
    // Get config size
    size_t required_size = 0;
    err = nvs_get_str(nvs_handle, NVS_KEY_CONFIG, NULL, &required_size);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to get cached config size");
        nvs_close(nvs_handle);
        return false;
    }
    
    // Allocate and read config
    char* json_config = (char*)malloc(required_size);
    if (!json_config) {
        ESP_LOGE(TAG, "Failed to allocate memory for cached config");
        nvs_close(nvs_handle);
        return false;
    }
    
    err = nvs_get_str(nvs_handle, NVS_KEY_CONFIG, json_config, &required_size);
    nvs_close(nvs_handle);
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read cached config");
        free(json_config);
        return false;
    }
    
    ESP_LOGI(TAG, "Loaded cached configuration (version %d)", (int)version);
    
    std::string config_str(json_config);
    free(json_config);
    
    return parseAndApply(config_str);
}

bool ConfigManager::saveCachedConfig(const std::string& json_config) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS for writing: %s", esp_err_to_name(err));
        return false;
    }
    
    // Save version
    err = nvs_set_i32(nvs_handle, NVS_KEY_VERSION, m_current_version);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save version: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return false;
    }
    
    // Save config
    err = nvs_set_str(nvs_handle, NVS_KEY_CONFIG, json_config.c_str());
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save config: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return false;
    }
    
    err = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit NVS: %s", esp_err_to_name(err));
        return false;
    }
    
    ESP_LOGI(TAG, "Configuration cached to NVS");
    return true;
}
