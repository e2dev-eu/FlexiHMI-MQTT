#ifndef LABEL_WIDGET_H
#define LABEL_WIDGET_H

#include "hmi_widget.h"
#include "cJSON.h"

/**
 * @brief Label widget - displays text, can be updated via MQTT
 */
class LabelWidget : public HMIWidget {
public:
    LabelWidget(const std::string& id, int x, int y, int w, int h, cJSON* properties, lv_obj_t* parent = nullptr);
    ~LabelWidget() override;
    void onMqttMessage(const std::string& topic, const std::string& payload) override;

private:
    static void async_update_cb(void* user_data);
    void updateText(const std::string& text);
    
    std::string m_text;
    std::string m_pending_text;
    std::string m_format;  // printf-style format string
    std::string m_mqtt_topic;
    uint32_t m_subscription_handle = 0;
    lv_color_t m_color;
    bool m_has_color = false;
};

#endif // LABEL_WIDGET_H
