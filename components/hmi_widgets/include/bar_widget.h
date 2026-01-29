#pragma once

#include "hmi_widget.h"

class BarWidget : public HMIWidget {
public:
    bool create(const std::string& id, int x, int y, int w, int h, cJSON* properties, lv_obj_t* parent = nullptr) override;
    void destroy() override;
    void onMqttMessage(const std::string& topic, const std::string& payload) override;
    
private:
    static void async_update_cb(void* user_data);
    
    struct AsyncUpdateData {
        BarWidget* widget;
        int value;
    };
    
    void updateValue(int value);
    
    std::string m_mqtt_topic;
    int m_min;
    int m_max;
    int m_value;
    lv_color_t m_color;
    bool m_has_color = false;
    lv_obj_t* m_label = nullptr;
};
