#ifndef WIDGET_REGISTRY_H
#define WIDGET_REGISTRY_H

#include "hmi_widget.h"
#include <string>
#include <map>

/**
 * @brief Factory for creating widgets by type name
 */
class WidgetRegistry {
public:
    using WidgetFactory = HMIWidget* (*)();
    
    static void registerWidget(const std::string& type, WidgetFactory factory);
    static HMIWidget* createWidget(const std::string& type);
    static void initialize();  // Initialize all built-in widgets
    
private:
    static std::map<std::string, WidgetFactory>& getRegistry();
};

// Macro to auto-register widget types
#define REGISTER_WIDGET(type_name, class_name) \
    namespace { \
        HMIWidget* create##class_name() { return new class_name(); } \
        struct Register##class_name { \
            Register##class_name() { \
                WidgetRegistry::registerWidget(type_name, create##class_name); \
            } \
        }; \
        static Register##class_name register##class_name; \
    }

#endif // WIDGET_REGISTRY_H
