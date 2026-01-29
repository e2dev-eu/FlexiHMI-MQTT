#include "widget_registry.h"
#include "label_widget.h"
#include "button_widget.h"
#include <esp_log.h>

static const char* TAG = "WidgetRegistry";

std::map<std::string, WidgetRegistry::WidgetFactory>& WidgetRegistry::getRegistry() {
    static std::map<std::string, WidgetFactory> registry;
    return registry;
}

void WidgetRegistry::registerWidget(const std::string& type, WidgetFactory factory) {
    getRegistry()[type] = factory;
    ESP_LOGI(TAG, "Registered widget type: %s", type.c_str());
}

HMIWidget* WidgetRegistry::createWidget(const std::string& type) {
    auto& registry = getRegistry();
    auto it = registry.find(type);
    if (it != registry.end()) {
        return it->second();
    }
    ESP_LOGE(TAG, "Widget type '%s' not found", type.c_str());
    return nullptr;
}

void WidgetRegistry::initialize() {
    static bool initialized = false;
    if (initialized) {
        return;
    }
    
    ESP_LOGI(TAG, "Initializing widget registry");
    
    // Register label widget
    registerWidget("label", []() -> HMIWidget* { return new LabelWidget(); });
    
    // Register button widget
    registerWidget("button", []() -> HMIWidget* { return new ButtonWidget(); });
    
    initialized = true;
    ESP_LOGI(TAG, "Widget registry initialized with %d types", getRegistry().size());
}
