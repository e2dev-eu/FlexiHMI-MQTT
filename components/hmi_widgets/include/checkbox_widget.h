#pragma once

#include "hmi_widget.h"

class CheckboxWidget : public HMIWidget {
public:
    CheckboxWidget(const std::string& id, int x, int y, int w, int h, cJSON* properties, lv_obj_t* parent = nullptr);
    ~CheckboxWidget() override;
    void onMqttMessage(const std::string& topic, const std::string& payload) override;
    
private:
    static void checkbox_event_cb(lv_event_t* e);
    static void async_update_cb(void* user_data);
    
    struct AsyncUpdateData {
        CheckboxWidget* widget;
        bool checked;
    };
    
    void updateState(bool checked);
    
    std::string m_mqtt_topic;
    std::string m_text;
    bool m_checked;
    bool m_retained;
    bool m_updating_from_mqtt = false;
    uint32_t m_subscription_handle = 0;  // MQTT subscription handle
    lv_color_t m_color;
    bool m_has_color = false;
};
