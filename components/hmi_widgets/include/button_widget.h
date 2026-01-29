#ifndef BUTTON_WIDGET_H
#define BUTTON_WIDGET_H

#include "hmi_widget.h"
#include "cJSON.h"

/**
 * @brief Button widget - publishes MQTT message on click
 */
class ButtonWidget : public HMIWidget {
public:
    bool create(const std::string& id, int x, int y, int w, int h, cJSON* properties, lv_obj_t* parent = nullptr) override;
    void destroy() override;
    void onMqttMessage(const std::string& topic, const std::string& payload) override;

private:
    static void button_event_cb(lv_event_t* e);

    std::string m_button_text;
    std::string m_publish_topic;
    std::string m_publish_payload;
    lv_obj_t* m_label;
    lv_color_t m_color;
    bool m_has_color = false;
};

#endif // BUTTON_WIDGET_H
