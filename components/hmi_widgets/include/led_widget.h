#pragma once

#include "hmi_widget.h"

class LEDWidget : public HMIWidget {
public:
    bool create(const std::string& id, int x, int y, int w, int h, cJSON* properties, lv_obj_t* parent = nullptr) override;
    void destroy() override;
    void onMqttMessage(const std::string& topic, const std::string& payload) override;
    
private:
    static void async_update_cb(void* user_data);
    
    struct AsyncUpdateData {
        LEDWidget* widget;
        uint8_t brightness;
    };
    
    void updateBrightness(uint8_t brightness);
    
    std::string m_mqtt_topic;
    uint8_t m_brightness;
    uint32_t m_subscription_handle = 0;  // MQTT subscription handle
    lv_color_t m_color_on;
    lv_color_t m_color_off;
};
