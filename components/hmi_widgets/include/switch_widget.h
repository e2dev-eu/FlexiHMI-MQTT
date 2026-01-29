#pragma once

#include "hmi_widget.h"

class SwitchWidget : public HMIWidget {
public:
    bool create(const std::string& id, int x, int y, int w, int h, cJSON* properties, lv_obj_t* parent = nullptr) override;
    void destroy() override;
    void onMqttMessage(const std::string& topic, const std::string& payload) override;
    
private:
    static void switch_event_cb(lv_event_t* e);
    
    std::string m_mqtt_topic;
    bool m_state;
};
