#include "image_widget.h"
#include "mqtt_manager.h"
#include <esp_log.h>
#include <cstring>
#include <cstdint>
#include <esp_heap_caps.h>
#include <sys/stat.h>
#include <unistd.h>
#include "mbedtls/base64.h"

static const char *TAG = "ImageWidget";

static uint8_t* alloc_image_buffer(size_t size) {
    uint8_t* buf = (uint8_t*)heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf) {
        buf = (uint8_t*)heap_caps_malloc(size, MALLOC_CAP_8BIT);
    }
    return buf;
}

static void free_image_buffer(void* buf) {
    if (buf) {
        heap_caps_free(buf);
    }
}

void ImageWidget::free_timer_cb(lv_timer_t* timer) {
    ImageWidget* widget = static_cast<ImageWidget*>(lv_timer_get_user_data(timer));
    if (!widget) {
        return;
    }

    for (const auto& pending : widget->m_pending_free) {
        if (pending.dsc) {
            lv_image_cache_drop(pending.dsc);
            free(pending.dsc);
        }
        if (pending.data) {
            free_image_buffer(pending.data);
        }
    }
    widget->m_pending_free.clear();

    if (widget->m_free_timer) {
        lv_timer_del(widget->m_free_timer);
        widget->m_free_timer = nullptr;
    }
}

void ImageWidget::schedule_free(lv_image_dsc_t* dsc, uint8_t* data) {
    if (!dsc && !data) {
        return;
    }

    m_pending_free.push_back({dsc, data});

    if (!m_free_timer) {
        m_free_timer = lv_timer_create(free_timer_cb, 300, this);
    }
}

static uint32_t read_be32(const uint8_t* data) {
    return static_cast<uint32_t>((static_cast<uint32_t>(data[0]) << 24) |
        (static_cast<uint32_t>(data[1]) << 16) |
        (static_cast<uint32_t>(data[2]) << 8) |
        static_cast<uint32_t>(data[3]));
}

static bool is_qoi_data(const uint8_t* data, size_t size) {
    if (!data || size < 14) {
        return false;
    }
    return memcmp(data, "qoif", 4) == 0;
}

static bool is_qoi_base64(const std::string& base64_data) {
    if (base64_data.size() < 12) {
        return false;
    }

    size_t prefix_len = base64_data.size() < 24 ? base64_data.size() : 24;
    prefix_len = (prefix_len / 4) * 4;
    if (prefix_len < 8) {
        return false;
    }

    uint8_t decoded[32] = {0};
    size_t decoded_len = 0;
    int ret = mbedtls_base64_decode(decoded, sizeof(decoded), &decoded_len,
                                    (const unsigned char*)base64_data.data(),
                                    prefix_len);
    if (ret != 0 || decoded_len < 4) {
        return false;
    }

    return memcmp(decoded, "qoif", 4) == 0;
}

static bool get_qoi_dimensions(const uint8_t* data, size_t size, uint16_t* out_w, uint16_t* out_h) {
    if (!data || size < 14 || memcmp(data, "qoif", 4) != 0) {
        return false;
    }
    uint32_t w = read_be32(data + 4);
    uint32_t h = read_be32(data + 8);
    if (w == 0 || h == 0 || w > 65535 || h > 65535) {
        return false;
    }
    *out_w = static_cast<uint16_t>(w);
    *out_h = static_cast<uint16_t>(h);
    return true;
}

ImageWidget::ImageWidget(const std::string& id, int x, int y, int w, int h, cJSON* properties, lv_obj_t* parent) {
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

    m_pending_data = m_image_path;
    
    // Create image object
    lv_obj_t* parent_obj = parent ? parent : lv_screen_active();
    m_lvgl_obj = lv_image_create(parent_obj);
    if (!m_lvgl_obj) {
        ESP_LOGE(TAG, "Failed to create image widget: %s", id.c_str());
        return;
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
}

ImageWidget::~ImageWidget() {
    cancelAsync(async_update_cb, this);
    if (m_subscription_handle != 0) {
        MQTTManager::getInstance().unsubscribe(m_subscription_handle);
        m_subscription_handle = 0;
    }
    
    // Free base64 decoded data (only once!)
    if (m_decoded_data) {
        free_image_buffer(m_decoded_data);
        m_decoded_data = nullptr;
        m_decoded_size = 0;
    }

    // Free any pending buffers
    if (!m_pending_free.empty()) {
        for (const auto& pending : m_pending_free) {
            if (pending.dsc) {
                lv_image_cache_drop(pending.dsc);
                free(pending.dsc);
            }
            if (pending.data) {
                free_image_buffer(pending.data);
            }
        }
        m_pending_free.clear();
    }

    if (m_free_timer) {
        lv_timer_del(m_free_timer);
        m_free_timer = nullptr;
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

    m_pending_data = payload;
    scheduleAsync(async_update_cb, this);
    ESP_LOGI(TAG, "Scheduled async update for image %s", m_id.c_str());
}

void ImageWidget::async_update_cb(void* user_data) {
    ImageWidget* widget = static_cast<ImageWidget*>(user_data);
    if (!widget) {
        return;
    }
    widget->markAsyncComplete();
    ESP_LOGI(TAG, "Async callback executing for image update (size: %d bytes)", widget->m_pending_data.size());
    widget->updateImage(widget->m_pending_data);
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
    
    // Check file extension for supported format (QOI)
    const char* ext = strrchr(path.c_str(), '.');
    if (ext) {
        ESP_LOGI(TAG, "File extension: %s", ext);
        if (strcasecmp(ext, ".qoi") != 0) {
            ESP_LOGE(TAG, "Unsupported file extension: %s (supported: qoi)", ext);
            return false;
        }
    } else {
        ESP_LOGE(TAG, "No file extension found in path: %s", path.c_str());
        return false;
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
    
    // Note: LVGL will handle QOI decoding
    
    ESP_LOGI(TAG, "Successfully loaded image from: %s", path.c_str());
    return true;
}

bool ImageWidget::isBase64Data(const std::string& data) {
    if(data.empty() || data.rfind("/sdcard/", 0) == 0 || data.rfind("S:/", 0) == 0) {
        return false; // Looks like a file path
    }

    return true; // Assume it's base64 data if not a file path
}

bool ImageWidget::loadImageFromBase64(const std::string& base64_data) {
    ESP_LOGD(TAG, "Decoding base64 image data (%d bytes)", base64_data.size());

    if (!is_qoi_base64(base64_data)) {
        ESP_LOGE(TAG, "Base64 data is not QOI");
        return false;
    }
    
    // Calculate required buffer size for decoded data
    size_t decoded_len = 0;
    int ret = mbedtls_base64_decode(nullptr, 0, &decoded_len, 
                                     (const unsigned char*)base64_data.c_str(), 
                                     base64_data.size());
    
    if (ret != MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL) {
        ESP_LOGE(TAG, "Failed to calculate base64 decode size: %d", ret);
        return false;
    }
    
    ESP_LOGD(TAG, "Base64 decode will produce %d bytes", decoded_len);
    
    // Allocate buffer for decoded data (temporary until validated)
    uint8_t* decoded_data = alloc_image_buffer(decoded_len);
    if (!decoded_data) {
        ESP_LOGE(TAG, "Failed to allocate %d bytes for decoded image", decoded_len);
        return false;
    }
    
    // Decode base64 data
    size_t decoded_size = 0;
    ret = mbedtls_base64_decode(decoded_data, decoded_len, &decoded_size,
                                 (const unsigned char*)base64_data.c_str(),
                                 base64_data.size());
    
    if (ret != 0) {
        ESP_LOGE(TAG, "Base64 decode failed: %d", ret);
        free_image_buffer(decoded_data);
        return false;
    }
    
    ESP_LOGD(TAG, "Successfully decoded %d bytes of image data", decoded_size);

    uint16_t img_w = 0;
    uint16_t img_h = 0;
    if (!is_qoi_data(decoded_data, decoded_size)) {
        ESP_LOGE(TAG, "Decoded data is not QOI");
        free_image_buffer(decoded_data);
        return false;
    }

    if (!get_qoi_dimensions(decoded_data, decoded_size, &img_w, &img_h)) {
        ESP_LOGE(TAG, "Failed to parse QOI dimensions");
        free_image_buffer(decoded_data);
        return false;
    }
    
    // Log first few bytes for debugging
    ESP_LOGD(TAG, "Image data (first 4 bytes): %02X %02X %02X %02X", 
             decoded_size > 0 ? decoded_data[0] : 0,
             decoded_size > 1 ? decoded_data[1] : 0,
             decoded_size > 2 ? decoded_data[2] : 0,
             decoded_size > 3 ? decoded_data[3] : 0);

    // Allocate persistent image descriptor (must stay in memory for LVGL)
    lv_image_dsc_t* new_img_dsc = (lv_image_dsc_t*)malloc(sizeof(lv_image_dsc_t));
    if (!new_img_dsc) {
        ESP_LOGE(TAG, "Failed to allocate image descriptor");
        free_image_buffer(decoded_data);
        return false;
    }
    
    // Initialize descriptor for LVGL
    memset(new_img_dsc, 0, sizeof(lv_image_dsc_t));
    new_img_dsc->header.magic = LV_IMAGE_HEADER_MAGIC;
    new_img_dsc->header.cf = LV_COLOR_FORMAT_RAW;
    new_img_dsc->header.w = img_w;
    new_img_dsc->header.h = img_h;
    new_img_dsc->data = decoded_data;
    new_img_dsc->data_size = decoded_size;
    
    // Set the image source to the persistent descriptor
    ESP_LOGD(TAG, "Calling lv_image_set_src with descriptor...");
    lv_image_set_src(m_lvgl_obj, new_img_dsc);
    
    // Check what LVGL thinks the source is
    const void* src = lv_image_get_src(m_lvgl_obj);
    if (src) {
        lv_image_src_t src_type = lv_image_src_get_type(src);
        ESP_LOGD(TAG, "Image source set: type=%d", src_type);
    } else {
        ESP_LOGE(TAG, "Image source is NULL after setting!");
        free_image_buffer(decoded_data);
        free(new_img_dsc);
        return false;
    }

    // Drop cache and free old buffers only after successful set
    if (m_img_dsc || m_decoded_data) {
        schedule_free(m_img_dsc, m_decoded_data);
    }
    m_img_dsc = new_img_dsc;
    m_decoded_data = decoded_data;
    m_decoded_size = decoded_size;
    
    // Force LVGL to process the image
    ESP_LOGI(TAG, "Invalidating object to trigger redraw...");
    lv_obj_invalidate(m_lvgl_obj);
        
    ESP_LOGI(TAG, "Base64 image loaded successfully");
    return true;
}

