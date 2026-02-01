#pragma once

#include "hmi_widget.h"

class GaugeWidget : public HMIWidget {
public:
    bool create(const std::string& id, int x, int y, int w, int h, cJSON* properties, lv_obj_t* parent = nullptr) override;
    void destroy() override;
    void onMqttMessage(const std::string& topic, const std::string& payload) override;
    
private:
    static void async_update_cb(void* user_data);
    
    struct AsyncUpdateData {
        GaugeWidget* widget;
        int value;
    };
    
    void updateValue(int value);
    
    std::string m_mqtt_topic;
    int m_value;
    int m_min_value;
    int m_max_value;
    uint32_t m_subscription_handle = 0;  // MQTT subscription handle
    
    lv_obj_t* m_needle;  // Needle line object
};
