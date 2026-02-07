#pragma once

#include "hmi_widget.h"

class ContainerWidget : public HMIWidget {
public:
    ContainerWidget(const std::string& id, int x, int y, int w, int h, cJSON* properties, lv_obj_t* parent = nullptr);
    ~ContainerWidget() override;
    void onMqttMessage(const std::string& topic, const std::string& payload) override;
};
