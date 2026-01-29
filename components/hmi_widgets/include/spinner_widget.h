#pragma once

#include "hmi_widget.h"

class SpinnerWidget : public HMIWidget {
public:
    bool create(const std::string& id, int x, int y, int w, int h, cJSON* properties, lv_obj_t* parent = nullptr) override;
    void destroy() override;
    void onMqttMessage(const std::string& topic, const std::string& payload) override;
    
private:
    static void async_update_cb(void* user_data);
    
    struct AsyncUpdateData {
        SpinnerWidget* widget;
        bool visible;
    };
    
    void updateVisibility(bool visible);
    
    std::string m_mqtt_topic;
    lv_color_t m_color;
    bool m_has_color = false;
};
