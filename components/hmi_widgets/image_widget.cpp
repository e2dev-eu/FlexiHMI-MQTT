#include "image_widget.h"
#include "mqtt_manager.h"
#include <esp_log.h>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>

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
    
    // Load initial image if path provided
    if (!m_image_path.empty()) {
        ESP_LOGI(TAG, "Loading initial image: %s", m_image_path.c_str());
        if (!loadImageFromPath(m_image_path)) {
            ESP_LOGE(TAG, "Failed to load initial image: %s", m_image_path.c_str());
        } else {
            ESP_LOGI(TAG, "Successfully loaded initial image: %s", m_image_path.c_str());
        }
    } else {
        ESP_LOGW(TAG, "No initial image path provided for widget: %s", id.c_str());
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
    
    if (m_img_dsc) {
        // Free image data if allocated
        if (m_img_dsc->data) {
            free((void*)m_img_dsc->data);
        }
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
    // Payload should be the image path (e.g., "/sdcard/images/photo.jpg")
    ESP_LOGI(TAG, "Image %s received MQTT message on %s: %s", 
             m_id.c_str(), topic.c_str(), payload.c_str());
    
    AsyncUpdateData* data = new AsyncUpdateData{this, payload};
    lv_async_call(async_update_cb, data);
    ESP_LOGI(TAG, "Scheduled async update for image %s", m_id.c_str());
}

void ImageWidget::async_update_cb(void* user_data) {
    AsyncUpdateData* data = static_cast<AsyncUpdateData*>(user_data);
    if (data && data->widget) {
        ESP_LOGI(TAG, "Async callback executing for image update: %s", data->path.c_str());
        data->widget->updateImage(data->path);
    } else {
        ESP_LOGE(TAG, "Invalid async callback data");
    }
    delete data;
}

void ImageWidget::updateImage(const std::string& path) {
    if (!m_lvgl_obj || !lv_obj_is_valid(m_lvgl_obj)) {
        return;
    }
    
    if (loadImageFromPath(path)) {
        m_image_path = path;
        ESP_LOGI(TAG, "Updated image %s to: %s", m_id.c_str(), path.c_str());
    } else {
        ESP_LOGE(TAG, "Failed to update image %s to: %s", m_id.c_str(), path.c_str());
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

// Register this widget type
