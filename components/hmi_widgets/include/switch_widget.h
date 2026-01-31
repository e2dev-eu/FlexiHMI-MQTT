#pragma once

#include "hmi_widget.h"

class SwitchWidget : public HMIWidget {
public:
    bool create(const std::string& id, int x, int y, int w, int h, cJSON* properties, lv_obj_t* parent = nullptr) override;
    void destroy() override;
    void onMqttMessage(const std::string& topic, const std::string& payload) override;
    
private:
    static void switch_event_cb(lv_event_t* e);
    static void async_update_cb(void* user_data);
    
    struct AsyncUpdateData {
        SwitchWidget* widget;
        bool state;
    };
    
    void updateState(bool state);
    
    std::string m_mqtt_topic;
    bool m_state;
    bool m_retained;
    bool m_updating_from_mqtt = false;  // Prevent feedback loop
    uint32_t m_subscription_handle = 0;  // MQTT subscription handle
    lv_color_t m_color;
    bool m_has_color = false;
};
