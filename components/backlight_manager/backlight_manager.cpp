#include "backlight_manager.h"
#include "sdkconfig.h"
#include "esp_err.h"
#include "esp_log.h"
#include <cmath>

static const char* TAG = "BacklightManager";

#if CONFIG_BSP_BOARD_JC1060WP470C_I_W_Y
extern "C" esp_err_t jc_bsp_display_brightness_set(int brightness_percent);
#define panel_display_brightness_set jc_bsp_display_brightness_set
#else
extern "C" esp_err_t bsp_display_brightness_set(int brightness_percent);
#define panel_display_brightness_set bsp_display_brightness_set
#endif

BacklightManager& BacklightManager::getInstance() {
    static BacklightManager instance;
    return instance;
}

BacklightManager::BacklightManager()
    : m_initialized(false)
    , m_dim_timeout_sec(30)
    , m_normal_brightness(100)
    , m_dim_brightness(20)
    , m_fade_duration_ms(1000)
    , m_current_brightness(100)
    , m_is_dimmed(false)
    , m_dim_timer(nullptr)
    , m_fade_timer(nullptr)
    , m_fade_start_brightness(100)
    , m_fade_target_brightness(100)
    , m_fade_start_time(0)
{
    m_mutex = xSemaphoreCreateMutex();
}

BacklightManager::~BacklightManager() {
    deinit();
    if (m_mutex) {
        vSemaphoreDelete(m_mutex);
        m_mutex = nullptr;
    }
}

esp_err_t BacklightManager::init(uint32_t dim_timeout_sec, 
                                  uint8_t dim_brightness_percent,
                                  uint32_t fade_duration_ms) {
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    
    if (m_initialized) {
        ESP_LOGW(TAG, "Already initialized");
        xSemaphoreGive(m_mutex);
        return ESP_OK;
    }

    m_dim_timeout_sec = dim_timeout_sec;
    m_dim_brightness = dim_brightness_percent > 100 ? 100 : dim_brightness_percent;
    m_fade_duration_ms = fade_duration_ms;
    m_current_brightness = m_normal_brightness;

    // Create inactivity timer
    const esp_timer_create_args_t dim_timer_args = {
        .callback = &dimTimerCallback,
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "backlight_dim",
        .skip_unhandled_events = false
    };

    esp_err_t ret = esp_timer_create(&dim_timer_args, &m_dim_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create dim timer: %s", esp_err_to_name(ret));
        xSemaphoreGive(m_mutex);
        return ret;
    }

    // Create fade animation timer (runs at 60 FPS)
    const esp_timer_create_args_t fade_timer_args = {
        .callback = &fadeTimerCallback,
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "backlight_fade",
        .skip_unhandled_events = false
    };

    ret = esp_timer_create(&fade_timer_args, &m_fade_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create fade timer: %s", esp_err_to_name(ret));
        esp_timer_delete(m_dim_timer);
        m_dim_timer = nullptr;
        xSemaphoreGive(m_mutex);
        return ret;
    }

    // Start the inactivity timer
    ret = esp_timer_start_once(m_dim_timer, (uint64_t)m_dim_timeout_sec * 1000000ULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start dim timer: %s", esp_err_to_name(ret));
        esp_timer_delete(m_dim_timer);
        esp_timer_delete(m_fade_timer);
        m_dim_timer = nullptr;
        m_fade_timer = nullptr;
        xSemaphoreGive(m_mutex);
        return ret;
    }

    m_initialized = true;
    ESP_LOGI(TAG, "Backlight manager initialized (timeout: %lus, dim: %d%%, fade: %lums)",
             m_dim_timeout_sec, m_dim_brightness, m_fade_duration_ms);
    
    xSemaphoreGive(m_mutex);
    return ESP_OK;
}

void BacklightManager::deinit() {
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    
    if (!m_initialized) {
        xSemaphoreGive(m_mutex);
        return;
    }

    if (m_fade_timer) {
        esp_timer_stop(m_fade_timer);
        esp_timer_delete(m_fade_timer);
        m_fade_timer = nullptr;
    }

    if (m_dim_timer) {
        esp_timer_stop(m_dim_timer);
        esp_timer_delete(m_dim_timer);
        m_dim_timer = nullptr;
    }

    m_initialized = false;
    xSemaphoreGive(m_mutex);
    
    ESP_LOGI(TAG, "Backlight manager deinitialized");
}

void BacklightManager::resetTimer() {
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    
    if (!m_initialized) {
        xSemaphoreGive(m_mutex);
        return;
    }

    // If dimmed, restore brightness
    if (m_is_dimmed) {
        m_is_dimmed = false;
        m_fade_start_brightness = m_current_brightness;
        m_fade_target_brightness = m_normal_brightness;
        m_fade_start_time = esp_timer_get_time();
        
        // Stop fade timer if running and restart
        esp_timer_stop(m_fade_timer);
        esp_timer_start_periodic(m_fade_timer, 50 * 1000);
    }

    // Restart the inactivity timer
    esp_timer_stop(m_dim_timer);
    esp_timer_start_once(m_dim_timer, (uint64_t)m_dim_timeout_sec * 1000000ULL);
    
    xSemaphoreGive(m_mutex);
}

uint8_t BacklightManager::getCurrentBrightness() const {
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    uint8_t brightness = m_current_brightness;
    xSemaphoreGive(m_mutex);
    return brightness;
}

void BacklightManager::dimTimerCallback(void* arg) {
    BacklightManager* manager = static_cast<BacklightManager*>(arg);
    manager->startDimming();
}

void BacklightManager::fadeTimerCallback(void* arg) {
    BacklightManager* manager = static_cast<BacklightManager*>(arg);
    
    xSemaphoreTake(manager->m_mutex, portMAX_DELAY);
    
    uint32_t elapsed = (esp_timer_get_time() - manager->m_fade_start_time) / 1000; // Convert to ms
    
    if (elapsed >= manager->m_fade_duration_ms) {
        // Fade complete
        manager->m_current_brightness = manager->m_fade_target_brightness;
        esp_timer_stop(manager->m_fade_timer);
        
        ESP_LOGI(TAG, "Fade complete: %d%%", manager->m_current_brightness);
        xSemaphoreGive(manager->m_mutex);
        
        // Update brightness outside mutex
        panel_display_brightness_set(manager->m_current_brightness);
        return;
    }
    
    // Calculate smooth fade progress (ease-in-out)
    float progress = (float)elapsed / (float)manager->m_fade_duration_ms;
    // progress = progress * progress * (3.0f - 2.0f * progress); // Smoothstep
    
    int brightness_diff = (int)manager->m_fade_target_brightness - (int)manager->m_fade_start_brightness;
    manager->m_current_brightness = manager->m_fade_start_brightness + (brightness_diff * progress);
    
    uint8_t current_brightness = manager->m_current_brightness;
    xSemaphoreGive(manager->m_mutex);
    
    // Update brightness outside mutex
    panel_display_brightness_set(current_brightness);
}

void BacklightManager::startDimming() {
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    
    if (m_is_dimmed) {
        xSemaphoreGive(m_mutex);
        return;
    }

    ESP_LOGI(TAG, "Starting dim: %d%% -> %d%%", m_current_brightness, m_dim_brightness);
    
    m_is_dimmed = true;
    m_fade_start_brightness = m_current_brightness;
    m_fade_target_brightness = m_dim_brightness;
    m_fade_start_time = esp_timer_get_time();
    
    xSemaphoreGive(m_mutex);
    
    // Start timer outside mutex
    esp_timer_start_periodic(m_fade_timer, 50 * 1000);
}


