#ifndef LABEL_WIDGET_H
#define LABEL_WIDGET_H

#include "hmi_widget.h"
#include "cJSON.h"

/**
 * @brief Label widget - displays text, can be updated via MQTT
 */
class LabelWidget : public HMIWidget {
public:
    bool create(const std::string& id, int x, int y, int w, int h, cJSON* properties, lv_obj_t* parent = nullptr) override;
    void destroy() override;
    void onMqttMessage(const std::string& topic, const std::string& payload) override;

private:
    std::string m_text;
    std::string m_format;  // printf-style format string
    lv_color_t m_color;
    bool m_has_color = false;
};

#endif // LABEL_WIDGET_H
