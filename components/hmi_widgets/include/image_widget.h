#ifndef IMAGE_WIDGET_H
#define IMAGE_WIDGET_H

#include "hmi_widget.h"
#include "cJSON.h"
#include <string>

/**
 * @brief Image widget - displays images from SD card or HTTP URLs
 * 
 * Supports JPEG, PNG, BMP formats
 * Can update image source via MQTT
 */
class ImageWidget : public HMIWidget {
public:
    bool create(const std::string& id, int x, int y, int w, int h, cJSON* properties, lv_obj_t* parent = nullptr) override;
    void destroy() override;
    void onMqttMessage(const std::string& topic, const std::string& payload) override;

private:
    struct AsyncUpdateData {
        ImageWidget* widget;
        std::string path;
    };
    
    static void async_update_cb(void* user_data);
    void updateImage(const std::string& path);
    bool loadImageFromPath(const std::string& path);
    
    std::string m_image_path;
    std::string m_mqtt_topic;
    uint32_t m_subscription_handle = 0;
    lv_img_dsc_t* m_img_dsc = nullptr;
};

#endif // IMAGE_WIDGET_H
