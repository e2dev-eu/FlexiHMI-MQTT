#ifndef HMI_WIDGET_H
#define HMI_WIDGET_H

#include <string>
#include "lvgl.h"
#include "cJSON.h"

/**
 * @brief Base class for all HMI widgets
 */
class HMIWidget {
public:
    virtual ~HMIWidget() = default;
    
    /**
     * @brief Create the widget with specified geometry and properties
     * @param id Unique identifier for the widget
     * @param x X position
     * @param y Y position
     * @param w Width
     * @param h Height
     * @param properties JSON object with widget-specific properties
     * @param parent Optional parent LVGL object (nullptr = screen)
     * @return true if created successfully
     */
    virtual bool create(const std::string& id, int x, int y, int w, int h, cJSON* properties, lv_obj_t* parent = nullptr) = 0;
    
    /**
     * @brief Destroy the widget and free resources
     */
    virtual void destroy() = 0;
    
    /**
     * @brief Handle incoming MQTT message
     * @param topic MQTT topic
     * @param payload Message payload
     */
    virtual void onMqttMessage(const std::string& topic, const std::string& payload) = 0;
    
    /**
     * @brief Get widget ID
     */
    const std::string& getId() const { return m_id; }
    
    /**
     * @brief Get the underlying LVGL object
     */
    lv_obj_t* getLvglObject() const { return m_lvgl_obj; }

protected:
    std::string m_id;
    lv_obj_t* m_lvgl_obj = nullptr;
};

#endif // HMI_WIDGET_H
