#ifndef BUTTON_WIDGET_H
#define BUTTON_WIDGET_H

#include "hmi_widget.h"
#include "cJSON.h"

/**
 * @brief Button widget - publishes MQTT message on click
 */
class ButtonWidget : public HMIWidget {
public:
    ButtonWidget(const std::string& id, int x, int y, int w, int h, cJSON* properties, lv_obj_t* parent = nullptr);
    ~ButtonWidget() override;
    void onMqttMessage(const std::string& topic, const std::string& payload) override;

private:
    static void button_event_cb(lv_event_t* e);

    std::string m_button_text;
    std::string m_mqtt_topic;
    std::string m_mqtt_payload;
    lv_obj_t* m_label;
    lv_color_t m_color;
    bool m_has_color = false;
    bool m_retained = false;  // Default to non-retained for button clicks
};

#endif // BUTTON_WIDGET_H
