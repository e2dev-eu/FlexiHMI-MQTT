#ifndef IMAGE_WIDGET_H
#define IMAGE_WIDGET_H

#include "hmi_widget.h"
#include "cJSON.h"
#include <string>

/**
 * @brief Image widget - displays images from SD card or base64-encoded data
 * 
 * Supports JPEG, PNG, BMP formats
 * Can update image source via MQTT (file path or base64 data)
 */
class ImageWidget : public HMIWidget {
public:
    bool create(const std::string& id, int x, int y, int w, int h, cJSON* properties, lv_obj_t* parent = nullptr) override;
    void destroy() override;
    void onMqttMessage(const std::string& topic, const std::string& payload) override;

private:
    struct AsyncUpdateData {
        ImageWidget* widget;
        std::string data;
    };
    
    static void async_update_cb(void* user_data);
    void updateImage(const std::string& data);
    bool loadImageFromPath(const std::string& path);
    bool loadImageFromBase64(const std::string& base64_data);
    bool isBase64Data(const std::string& data);
    
    std::string m_image_path;
    std::string m_mqtt_topic;
    uint32_t m_subscription_handle = 0;
    lv_image_dsc_t* m_img_dsc = nullptr;
    uint8_t* m_decoded_data = nullptr;  // For base64 decoded images
    size_t m_decoded_size = 0;
};

#endif // IMAGE_WIDGET_H
