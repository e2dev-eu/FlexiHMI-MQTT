#pragma once

#include "hmi_widget.h"

class BarWidget : public HMIWidget {
public:
    BarWidget(const std::string& id, int x, int y, int w, int h, cJSON* properties, lv_obj_t* parent = nullptr);
    ~BarWidget() override;
    void onMqttMessage(const std::string& topic, const std::string& payload) override;
    
private:
    static void async_update_cb(void* user_data);
    
    void updateValue(int value);
    
    std::string m_mqtt_topic;
    int m_min;
    int m_max;
    int m_value;
    int m_pending_value = 0;
    uint32_t m_subscription_handle = 0;  // MQTT subscription handle
    lv_color_t m_color;
    bool m_has_color = false;
    lv_obj_t* m_label = nullptr;
};
