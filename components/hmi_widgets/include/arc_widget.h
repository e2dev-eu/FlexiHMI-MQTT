#pragma once

#include "hmi_widget.h"

class ArcWidget : public HMIWidget {
public:
    ArcWidget(const std::string& id, int x, int y, int w, int h, cJSON* properties, lv_obj_t* parent = nullptr);
    ~ArcWidget() override;
    void onMqttMessage(const std::string& topic, const std::string& payload) override;
    
private:
    static void arc_event_cb(lv_event_t* e);
    static void async_update_cb(void* user_data);
    
    struct AsyncUpdateData {
        ArcWidget* widget;
        int value;
    };
    
    void updateValue(int value);
    
    std::string m_mqtt_topic;
    int m_min;
    int m_max;
    int m_value;
    bool m_retained;
    bool m_updating_from_mqtt = false;
    uint32_t m_subscription_handle = 0;  // MQTT subscription handle
    lv_color_t m_color;
    bool m_has_color = false;
};
