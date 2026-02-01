#include "image_widget.h"
#include "mqtt_manager.h"
#include <esp_log.h>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>
#include "mbedtls/base64.h"

static const char *TAG = "ImageWidget";

bool ImageWidget::create(const std::string& id, int x, int y, int w, int h, cJSON* properties, lv_obj_t* parent) {
    m_id = id;
    
    // Extract properties
    if (properties) {
        cJSON* path_item = cJSON_GetObjectItem(properties, "image_path");
        if (path_item && cJSON_IsString(path_item)) {
            m_image_path = path_item->valuestring;
        }
        
        cJSON* mqtt_topic = cJSON_GetObjectItem(properties, "mqtt_topic");
        if (mqtt_topic && cJSON_IsString(mqtt_topic)) {
            m_mqtt_topic = mqtt_topic->valuestring;
        }
    }
    
    // Create image object
    lv_obj_t* parent_obj = parent ? parent : lv_screen_active();
    m_lvgl_obj = lv_image_create(parent_obj);
    if (!m_lvgl_obj) {
        ESP_LOGE(TAG, "Failed to create image widget: %s", id.c_str());
        return false;
    }
    
    lv_obj_set_pos(m_lvgl_obj, x, y);
    lv_obj_set_size(m_lvgl_obj, w, h);
    
    // Set image object properties for proper scaling
    lv_obj_set_style_radius(m_lvgl_obj, 0, LV_PART_MAIN);
    
    // Important: Set image to scale/fit within the widget bounds
    lv_image_set_scale(m_lvgl_obj, 256);  // 256 = 100% (no scaling by default)
    lv_image_set_rotation(m_lvgl_obj, 0);
    lv_image_set_pivot(m_lvgl_obj, 0, 0);
    
    // Enable antialiasing for better quality
    lv_obj_set_style_image_opa(m_lvgl_obj, LV_OPA_COVER, LV_PART_MAIN);
    
    ESP_LOGI(TAG, "Image widget configured: %dx%d at (%d,%d)", w, h, x, y);
    
    // Load initial image if path/data provided
    if (!m_image_path.empty()) {
        ESP_LOGI(TAG, "Loading initial image data (%d bytes)", m_image_path.size());
        
        // Check if it's base64 data or file path
        if (isBase64Data(m_image_path)) {
            ESP_LOGI(TAG, "Initial data is base64-encoded");
            if (!loadImageFromBase64(m_image_path)) {
                ESP_LOGE(TAG, "Failed to load initial base64 image");
            } else {
                ESP_LOGI(TAG, "Successfully loaded initial base64 image");
            }
        } else {
            ESP_LOGI(TAG, "Initial data is file path: %s", m_image_path.c_str());
            if (!loadImageFromPath(m_image_path)) {
                ESP_LOGE(TAG, "Failed to load initial image: %s", m_image_path.c_str());
            } else {
                ESP_LOGI(TAG, "Successfully loaded initial image: %s", m_image_path.c_str());
            }
        }
    } else {
        ESP_LOGW(TAG, "No initial image path/data provided for widget: %s", id.c_str());
    }
    
    // Subscribe to MQTT topic if specified
    if (!m_mqtt_topic.empty()) {
        m_subscription_handle = MQTTManager::getInstance().subscribe(m_mqtt_topic, 0,
            [this](const std::string& topic, const std::string& payload) {
                this->onMqttMessage(topic, payload);
            });
        
        if (m_subscription_handle != 0) {
            ESP_LOGI(TAG, "Image %s subscribed to %s for updates", id.c_str(), m_mqtt_topic.c_str());
        }
    }
    
    ESP_LOGI(TAG, "Created image widget: %s at (%d,%d) size (%dx%d)", 
             id.c_str(), x, y, w, h);
    
    return true;
}

void ImageWidget::destroy() {
    if (m_subscription_handle != 0) {
        MQTTManager::getInstance().unsubscribe(m_subscription_handle);
        m_subscription_handle = 0;
    }
    
    // Free base64 decoded data (only once!)
    if (m_decoded_data) {
        free(m_decoded_data);
        m_decoded_data = nullptr;
        m_decoded_size = 0;
    }
    
    // Free image descriptor (but not the data - already freed above)
    if (m_img_dsc) {
        // Note: m_img_dsc->data points to m_decoded_data, already freed above
        free(m_img_dsc);
        m_img_dsc = nullptr;
    }
    
    if (m_lvgl_obj && lv_obj_is_valid(m_lvgl_obj)) {
        lv_obj_delete(m_lvgl_obj);
        m_lvgl_obj = nullptr;
    }
    
    ESP_LOGI(TAG, "Destroyed image widget: %s", m_id.c_str());
}

void ImageWidget::onMqttMessage(const std::string& topic, const std::string& payload) {
    // Payload can be either a file path or base64-encoded image data
    ESP_LOGI(TAG, "Image %s received MQTT message on %s (size: %d bytes)", 
             m_id.c_str(), topic.c_str(), payload.size());
    
    AsyncUpdateData* data = new AsyncUpdateData{this, payload};
    lv_async_call(async_update_cb, data);
    ESP_LOGI(TAG, "Scheduled async update for image %s", m_id.c_str());
}

void ImageWidget::async_update_cb(void* user_data) {
    AsyncUpdateData* data = static_cast<AsyncUpdateData*>(user_data);
    if (data && data->widget) {
        ESP_LOGI(TAG, "Async callback executing for image update (size: %d bytes)", data->data.size());
        data->widget->updateImage(data->data);
    } else {
        ESP_LOGE(TAG, "Invalid async callback data");
    }
    delete data;
}

void ImageWidget::updateImage(const std::string& data) {
    if (!m_lvgl_obj || !lv_obj_is_valid(m_lvgl_obj)) {
        return;
    }
    
    bool success = false;
    
    // Check if data is base64-encoded or a file path
    if (isBase64Data(data)) {
        ESP_LOGI(TAG, "Detected base64-encoded image data");
        success = loadImageFromBase64(data);
    } else {
        ESP_LOGI(TAG, "Detected file path: %s", data.c_str());
        success = loadImageFromPath(data);
    }
    
    if (success) {
        m_image_path = data;
        ESP_LOGI(TAG, "Updated image %s successfully", m_id.c_str());
    } else {
        ESP_LOGE(TAG, "Failed to update image %s", m_id.c_str());
    }
}

bool ImageWidget::loadImageFromPath(const std::string& path) {
    if (path.empty()) {
        ESP_LOGE(TAG, "Empty image path provided");
        return false;
    }
    
    ESP_LOGI(TAG, "Attempting to load image from: %s", path.c_str());
    
    // Check if file exists and is accessible
    struct stat st;
    if (stat(path.c_str(), &st) != 0) {
        ESP_LOGE(TAG, "File does not exist or is not accessible: %s", path.c_str());
        return false;
    }
    
    // Check if it's a regular file
    if (!S_ISREG(st.st_mode)) {
        ESP_LOGE(TAG, "Path is not a regular file: %s", path.c_str());
        return false;
    }
    
    ESP_LOGI(TAG, "File exists, size: %ld bytes", st.st_size);
    
    if (st.st_size == 0) {
        ESP_LOGE(TAG, "File is empty: %s", path.c_str());
        return false;
    }
    
    // Check file extension for supported formats
    const char* ext = strrchr(path.c_str(), '.');
    if (ext) {
        ESP_LOGI(TAG, "File extension: %s", ext);
        if (strcasecmp(ext, ".jpg") != 0 && 
            strcasecmp(ext, ".jpeg") != 0 && 
            strcasecmp(ext, ".png") != 0 && 
            strcasecmp(ext, ".bmp") != 0) {
            ESP_LOGW(TAG, "Unsupported file extension: %s (supported: jpg, jpeg, png, bmp)", ext);
        }
    } else {
        ESP_LOGW(TAG, "No file extension found in path: %s", path.c_str());
    }
    
    // For SD card paths, LVGL can load directly using filesystem
    // With LV_USE_FS_POSIX enabled, use drive letter prefix
    // Path format: "S:/lena_256.jpg" (S: maps to /sdcard)
    std::string lvgl_path = path;
    
    // If path starts with /sdcard, convert to LVGL format
    if (path.find("/sdcard/") == 0) {
        lvgl_path = "S:" + path.substr(7);  // Remove "/sdcard" and add "S:"
        ESP_LOGI(TAG, "Converted path to LVGL format: %s", lvgl_path.c_str());
    } else if (path.find("/sdcard") == 0) {
        lvgl_path = "S:" + path.substr(7);  // Handle "/sdcard" without trailing slash
        ESP_LOGI(TAG, "Converted path to LVGL format: %s", lvgl_path.c_str());
    }
    
    ESP_LOGI(TAG, "Setting LVGL image source to: %s", lvgl_path.c_str());
    lv_image_set_src(m_lvgl_obj, lvgl_path.c_str());
    
    // Force LVGL to process the image immediately
    lv_obj_invalidate(m_lvgl_obj);
    
    // Check image object properties
    lv_coord_t w = lv_obj_get_width(m_lvgl_obj);
    lv_coord_t h = lv_obj_get_height(m_lvgl_obj);
    ESP_LOGI(TAG, "Image widget size: %dx%d", w, h);
    
    // Get image source info
    const void* src = lv_image_get_src(m_lvgl_obj);
    if (src) {
        ESP_LOGI(TAG, "Image source is set (not NULL)");
        lv_image_src_t src_type = lv_image_src_get_type(src);
        if (src_type == LV_IMAGE_SRC_FILE) {
            ESP_LOGI(TAG, "Image source type: FILE");
        } else if (src_type == LV_IMAGE_SRC_VARIABLE) {
            ESP_LOGI(TAG, "Image source type: VARIABLE");
        } else if (src_type == LV_IMAGE_SRC_SYMBOL) {
            ESP_LOGI(TAG, "Image source type: SYMBOL");
        } else {
            ESP_LOGI(TAG, "Image source type: UNKNOWN (%d)", src_type);
        }
    } else {
        ESP_LOGW(TAG, "Image source is NULL");
    }
    
    // Note: LVGL will handle image decoding (JPEG, PNG, BMP)
    // The ESP32-P4 has hardware JPEG decoder which LVGL can utilize
    
    ESP_LOGI(TAG, "Successfully loaded image from: %s", path.c_str());
    return true;
}

bool ImageWidget::isBase64Data(const std::string& data) {
    // Base64 strings are typically much longer than file paths
    // and contain only base64 characters: A-Z, a-z, 0-9, +, /, =
    if (data.size() < 100) {
        return false;  // Too short to be a meaningful base64 image
    }
    
    // Check if it starts with a path indicator
    if (data[0] == '/' || data.find(":/") != std::string::npos) {
        return false;  // Looks like a file path
    }
    
    // Check if most characters are base64 characters
    int base64_chars = 0;
    int sample_size = std::min((int)data.size(), 200);  // Check first 200 chars
    
    for (int i = 0; i < sample_size; i++) {
        char c = data[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || 
            (c >= '0' && c <= '9') || c == '+' || c == '/' || c == '=') {
            base64_chars++;
        }
    }
    
    // If more than 95% are base64 characters, assume it's base64
    return (base64_chars * 100 / sample_size) > 95;
}

bool ImageWidget::loadImageFromBase64(const std::string& base64_data) {
    ESP_LOGI(TAG, "Decoding base64 image data (%d bytes)", base64_data.size());
    
    // Calculate required buffer size for decoded data
    size_t decoded_len = 0;
    int ret = mbedtls_base64_decode(nullptr, 0, &decoded_len, 
                                     (const unsigned char*)base64_data.c_str(), 
                                     base64_data.size());
    
    if (ret != MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL) {
        ESP_LOGE(TAG, "Failed to calculate base64 decode size: %d", ret);
        return false;
    }
    
    ESP_LOGI(TAG, "Base64 decode will produce %d bytes", decoded_len);
    
    // Free previous decoded data if any
    if (m_decoded_data) {
        free(m_decoded_data);
        m_decoded_data = nullptr;
    }
    
    // Allocate buffer for decoded data
    m_decoded_data = (uint8_t*)malloc(decoded_len);
    if (!m_decoded_data) {
        ESP_LOGE(TAG, "Failed to allocate %d bytes for decoded image", decoded_len);
        return false;
    }
    
    // Decode base64 data
    ret = mbedtls_base64_decode(m_decoded_data, decoded_len, &m_decoded_size,
                                 (const unsigned char*)base64_data.c_str(),
                                 base64_data.size());
    
    if (ret != 0) {
        ESP_LOGE(TAG, "Base64 decode failed: %d", ret);
        free(m_decoded_data);
        m_decoded_data = nullptr;
        return false;
    }
    
    ESP_LOGI(TAG, "Successfully decoded %d bytes of image data", m_decoded_size);
    
    // Free previous image descriptor if any
    if (m_img_dsc) {
        free(m_img_dsc);
        m_img_dsc = nullptr;
    }
    
    // Allocate persistent image descriptor (must stay in memory for LVGL)
    m_img_dsc = (lv_image_dsc_t*)malloc(sizeof(lv_image_dsc_t));
    if (!m_img_dsc) {
        ESP_LOGE(TAG, "Failed to allocate image descriptor");
        free(m_decoded_data);
        m_decoded_data = nullptr;
        return false;
    }
    
    // Fill image descriptor with decoded data
    memset(m_img_dsc, 0, sizeof(lv_image_dsc_t));
    m_img_dsc->data = m_decoded_data;
    m_img_dsc->data_size = m_decoded_size;
    m_img_dsc->header.w = 0;  // Will be filled by decoder
    m_img_dsc->header.h = 0;
    m_img_dsc->header.cf = LV_COLOR_FORMAT_UNKNOWN;  // Let LVGL detect format
    m_img_dsc->header.stride = 0;
    
    ESP_LOGI(TAG, "Image descriptor: data=%p, size=%d, w=%d, h=%d, cf=%d", 
             m_img_dsc->data, m_img_dsc->data_size, 
             m_img_dsc->header.w, m_img_dsc->header.h, m_img_dsc->header.cf);
    
    // Set the image source to the persistent descriptor
    lv_image_set_src(m_lvgl_obj, m_img_dsc);
    
    // Check what LVGL thinks the source is
    const void* src = lv_image_get_src(m_lvgl_obj);
    if (src) {
        ESP_LOGI(TAG, "Image source set successfully");
        lv_image_src_t src_type = lv_image_src_get_type(src);
        ESP_LOGI(TAG, "Image source type after set: %d (1=FILE, 2=VARIABLE, 3=SYMBOL)", src_type);
    } else {
        ESP_LOGE(TAG, "Image source is NULL after setting!");
    }
    
    // Force LVGL to process the image
    lv_obj_invalidate(m_lvgl_obj);
    
    ESP_LOGI(TAG, "Base64 image loaded successfully");
    return true;
}

