#pragma once

#include "hmi_widget.h"
#include <vector>

class DropdownWidget : public HMIWidget {
public:
    DropdownWidget(const std::string& id, int x, int y, int w, int h, cJSON* properties, lv_obj_t* parent = nullptr);
    ~DropdownWidget() override;
    void onMqttMessage(const std::string& topic, const std::string& payload) override;
    
private:
    static void dropdown_event_cb(lv_event_t* e);
    static void async_update_cb(void* user_data);
    
    struct AsyncUpdateData {
        DropdownWidget* widget;
        uint16_t selected;
    };
    
    void updateSelection(uint16_t selected);
    
    std::string m_mqtt_topic;
    std::vector<std::string> m_options;
    uint16_t m_selected;
    bool m_retained;
    bool m_updating_from_mqtt = false;
    std::string m_last_published_payload;  // Track last published payload to ignore echo
    uint32_t m_subscription_handle = 0;  // MQTT subscription handle
    lv_color_t m_color;
    bool m_has_color = false;
};
