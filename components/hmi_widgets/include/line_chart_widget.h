#pragma once

#include "hmi_widget.h"

class LineChartWidget : public HMIWidget {
public:
    LineChartWidget(const std::string& id, int x, int y, int w, int h, cJSON* properties, lv_obj_t* parent = nullptr);
    ~LineChartWidget() override;
    void onMqttMessage(const std::string& topic, const std::string& payload) override;

private:
    static void async_update_cb(void* user_data);
    void pushValue(int value);

    std::string m_mqtt_topic;
    int m_min_value = 0;
    int m_max_value = 100;
    int m_pending_value = 0;
    int m_point_count = 32;
    uint32_t m_subscription_handle = 0;
    lv_chart_series_t* m_series = nullptr;
};
