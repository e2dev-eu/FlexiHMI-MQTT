#pragma once

#include "hmi_widget.h"

class SpinnerWidget : public HMIWidget {
public:
    SpinnerWidget(const std::string& id, int x, int y, int w, int h, cJSON* properties, lv_obj_t* parent = nullptr);
    ~SpinnerWidget() override;
    void onMqttMessage(const std::string& topic, const std::string& payload) override;
    
private:
    static void async_update_cb(void* user_data);
    
    struct AsyncUpdateData {
        SpinnerWidget* widget;
        bool visible;
    };
    
    void updateVisibility(bool visible);
    
    std::string m_mqtt_topic;
    uint32_t m_subscription_handle = 0;  // MQTT subscription handle
    lv_color_t m_color;
    bool m_has_color = false;
};
