#include "config_manager.h"
#include "mqtt_manager.h"
#include "label_widget.h"
#include "button_widget.h"
#include "container_widget.h"
#include "switch_widget.h"
#include "slider_widget.h"
#include "bar_widget.h"
#include "arc_widget.h"
#include "checkbox_widget.h"
#include "dropdown_widget.h"
#include "led_widget.h"
#include "spinner_widget.h"
#include "tabview_widget.h"
#include "gauge_widget.h"
#include "image_widget.h"
#include "esp_log.h"

// C wrapper functions for bringing UI elements to front
extern "C" {
    void settings_ui_bring_to_front();
    void status_info_bring_to_front();
}

// Forward declare for dynamic_cast
class TabviewWidget;

static const char* TAG = "ConfigManager";

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
    
    ESP_LOGI(TAG, "Processing new configuration (%d bytes)", json_config.length());
    ESP_LOGV(TAG, "JSON content: %s", json_config.c_str());
    
    cJSON* root = cJSON_Parse(json_config.c_str());
    if (!root) {
        const char* error_ptr = cJSON_GetErrorPtr();
        ESP_LOGE(TAG, "Failed to parse JSON!");
        ESP_LOGE(TAG, "Error position: %s", error_ptr ? error_ptr : "unknown");
        ESP_LOGE(TAG, "First 100 chars: %.100s", json_config.c_str());
        return false;
    }
    
    ESP_LOGV(TAG, "JSON parsed successfully");
    
    // Extract version (optional, for logging only)
    cJSON* version_item = cJSON_GetObjectItem(root, "version");
    int new_version = 0;
    if (version_item && cJSON_IsNumber(version_item)) {
        new_version = version_item->valueint;
        ESP_LOGV(TAG, "Configuration version: %d", new_version);
    } else {
        ESP_LOGV(TAG, "No version field, applying configuration anyway");
    }
    
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
    ESP_LOGV(TAG, "Found %d widgets in configuration", widget_count);
    
    bool success = parseWidgets(widgets_array);
    
    if (success) {
        m_current_version = new_version;
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
        ESP_LOGV(TAG, "Config queued for application by HMI task");
        xSemaphoreGive(m_config_mutex);
    } else {
        ESP_LOGE(TAG, "Failed to queue config - mutex timeout");
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
    } else if (type == "bar") {
        return new BarWidget();
    } else if (type == "arc") {
        return new ArcWidget();
    } else if (type == "checkbox") {
        return new CheckboxWidget();
    } else if (type == "dropdown") {
        return new DropdownWidget();
    } else if (type == "led") {
        return new LEDWidget();
    } else if (type == "spinner") {
        return new SpinnerWidget();
    } else if (type == "tabview") {
        return new TabviewWidget();
    } else if (type == "gauge") {
        return new GaugeWidget();
    }    else if (type == "image") {
        return new ImageWidget();
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
    ESP_LOGV(TAG, "Widget fields:");
    cJSON* field = widget_json->child;
    while (field) {
        ESP_LOGV(TAG, "  - %s (type: %d)", field->string ? field->string : "null", field->type);
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
    ESP_LOGV(TAG, "Widget type: %s", type.c_str());
    
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
    ESP_LOGV(TAG, "Widget id: %s", id.c_str());
    
    // Extract position and size (directly from widget object)
    cJSON* x_item = cJSON_GetObjectItem(widget_json, "x");
    cJSON* y_item = cJSON_GetObjectItem(widget_json, "y");
    cJSON* w_item = cJSON_GetObjectItem(widget_json, "w");
    cJSON* h_item = cJSON_GetObjectItem(widget_json, "h");
    
    if (!x_item || !cJSON_IsNumber(x_item)) {
        ESP_LOGE(TAG, "Missing or invalid 'x' for widget '%s'", id.c_str());
        return false;
    }
    if (!y_item || !cJSON_IsNumber(y_item)) {
        ESP_LOGE(TAG, "Missing or invalid 'y' for widget '%s'", id.c_str());
        return false;
    }
    if (!w_item || !cJSON_IsNumber(w_item)) {
        ESP_LOGE(TAG, "Missing or invalid 'w' for widget '%s'", id.c_str());
        return false;
    }
    if (!h_item || !cJSON_IsNumber(h_item)) {
        ESP_LOGE(TAG, "Missing or invalid 'h' for widget '%s'", id.c_str());
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
        ESP_LOGV(TAG, "Widget '%s' subscribed to %s", id.c_str(), topic.c_str());
    }
    
    // Process children if present
    cJSON* children = cJSON_GetObjectItem(widget_json, "children");
    if (children) {
        // Check if this is a tabview with per-tab children (object format)
        if (type == "tabview" && cJSON_IsObject(children)) {
            // Safe cast since we know type is "tabview"
            TabviewWidget* tabview = static_cast<TabviewWidget*>(widget);
            // Iterate through each tab name and parse its children
            for (const auto& tab_name : tabview->getTabNames()) {
                cJSON* tab_children = cJSON_GetObjectItem(children, tab_name.c_str());
                if (tab_children && cJSON_IsArray(tab_children)) {
                    lv_obj_t* tab_obj = tabview->getTabByName(tab_name);
                    if (tab_obj && lv_obj_is_valid(tab_obj)) {
                        ESP_LOGV(TAG, "Parsing %d children for tab '%s'", 
                                 cJSON_GetArraySize(tab_children), tab_name.c_str());
                        parseWidgets(tab_children, tab_obj);
                    }
                }
            }
        }
        // Standard array-based children for containers and other widgets
        else if (cJSON_IsArray(children)) {
            int child_count = cJSON_GetArraySize(children);
            ESP_LOGV(TAG, "Widget '%s' has %d children, parsing recursively...", id.c_str(), child_count);
            lv_obj_t* parent_obj = widget->getLvglObject();
            if (parent_obj && lv_obj_is_valid(parent_obj)) {
                ESP_LOGV(TAG, "Parent LVGL object is valid, creating children");
                parseWidgets(children, parent_obj);
            } else {
                ESP_LOGE(TAG, "Widget '%s' has invalid LVGL object, cannot create children", id.c_str());
            }
        }
    }
    
    m_active_widgets.push_back(widget);
    ESP_LOGV(TAG, "Created widget: type=%s, id=%s, pos=(%d,%d), size=(%dx%d)", 
             type.c_str(), id.c_str(), x, y, w, h);
    
    return true;
}

void ConfigManager::destroyAllWidgets() {
    ESP_LOGV(TAG, "Destroying %d widgets", m_active_widgets.size());
    
    for (HMIWidget* widget : m_active_widgets) {
        widget->destroy();
        delete widget;
    }
    
    m_active_widgets.clear();
}
