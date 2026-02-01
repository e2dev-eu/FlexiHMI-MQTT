#pragma once

#include "esp_err.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

class BacklightManager {
public:
    static BacklightManager& getInstance();

    /**
     * @brief Initialize the backlight manager
     * 
     * @param dim_timeout_sec Seconds of inactivity before dimming starts
     * @param dim_brightness_percent Target brightness percentage when dimmed (0-100)
     * @param fade_duration_ms Duration of fade animation in milliseconds
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t init(uint32_t dim_timeout_sec = 30, 
                   uint8_t dim_brightness_percent = 20,
                   uint32_t fade_duration_ms = 1000);

    /**
     * @brief Deinitialize the backlight manager
     */
    void deinit();

    /**
     * @brief Reset the inactivity timer (call on touch/user activity)
     */
    void resetTimer();

    /**
     * @brief Get current brightness
     */
    uint8_t getCurrentBrightness() const;


private:
    BacklightManager();
    ~BacklightManager();
    BacklightManager(const BacklightManager&) = delete;
    BacklightManager& operator=(const BacklightManager&) = delete;

    /**
     * @brief Dim timer callback for inactivity
     */
    static void dimTimerCallback(void* arg);
    
    /**
     * @brief Fade timer callback for smooth brightness transitions
     */
    static void fadeTimerCallback(void* arg);

    /**
     * @brief Start the dimming process
     */
    void startDimming();

    bool m_initialized;                 // True if initialized
    uint32_t m_dim_timeout_sec;         // Inactivity timeout in seconds
    uint8_t m_normal_brightness;        // Brightness percentage when not dimmed
    uint8_t m_dim_brightness;           // Brightness percentage when dimmed
    uint32_t m_fade_duration_ms;        // Fade animation duration in milliseconds
    
    uint8_t m_current_brightness;       // Current brightness percentage
    bool m_is_dimmed;                   // True if currently dimmed
    
    esp_timer_handle_t m_dim_timer;     // Timer for inactivity dimming
    esp_timer_handle_t m_fade_timer;    // Timer for fade animation
    
    uint8_t m_fade_start_brightness;    // Brightness at start of fade
    uint8_t m_fade_target_brightness;   // Target brightness at end of fade
    uint32_t m_fade_start_time;         // Timestamp when fade started (in microseconds)    
    
    SemaphoreHandle_t m_mutex;          // Mutex for thread safety
};
