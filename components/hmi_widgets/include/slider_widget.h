#pragma once

#include "hmi_widget.h"

class SliderWidget : public HMIWidget {
public:
    bool create(const std::string& id, int x, int y, int w, int h, cJSON* properties, lv_obj_t* parent = nullptr) override;
    void destroy() override;
    void onMqttMessage(const std::string& topic, const std::string& payload) override;
    
private:
    static void slider_event_cb(lv_event_t* e);
    static void async_update_cb(void* user_data);
    
    struct AsyncUpdateData {
        SliderWidget* widget;
        int value;
    };
    
    void updateValue(int value);
    
    std::string m_label;
    std::string m_mqtt_topic;
    int m_min;
    int m_max;
    int m_value;
    bool m_retained;
    bool m_updating_from_mqtt = false;  // Prevent feedback loop
    lv_color_t m_color;
    bool m_has_color = false;
    lv_obj_t* m_label_obj = nullptr;
    lv_obj_t* m_value_label = nullptr;
};
