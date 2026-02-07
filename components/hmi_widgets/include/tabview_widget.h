#pragma once

#include "hmi_widget.h"
#include <string>
#include <vector>
#include <map>

class TabviewWidget : public HMIWidget {
public:
    TabviewWidget(const std::string& id, int x, int y, int w, int h, cJSON* properties, lv_obj_t* parent = nullptr);
    ~TabviewWidget() override;
    void onMqttMessage(const std::string& topic, const std::string& payload) override;
    
    // Get tab object by name for child widget placement
    lv_obj_t* getTabByName(const std::string& tab_name);
    
    // Get all tab names
    const std::vector<std::string>& getTabNames() const { return m_tab_names; }

private:
    static void tab_changed_event_cb(lv_event_t* e);
    void onTabChanged(uint32_t tab_index);
    
    std::vector<std::string> m_tab_names;
    std::map<std::string, lv_obj_t*> m_tab_objects;
    std::string m_mqtt_topic;
    bool m_retained;
    bool m_updating_from_mqtt;
    uint32_t m_active_tab;
    uint32_t m_subscription_handle = 0;  // MQTT subscription handle
};
