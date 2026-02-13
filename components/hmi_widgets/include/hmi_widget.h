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
    void scheduleAsync(lv_async_cb_t cb, void* user_data);
    void cancelAsync(lv_async_cb_t cb, void* user_data);
    void markAsyncComplete();

    std::string m_id;
    lv_obj_t* m_lvgl_obj = nullptr;
    bool m_async_pending = false;
};

#endif // HMI_WIDGET_H
