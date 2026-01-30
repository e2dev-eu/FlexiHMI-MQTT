#pragma once

#include <string>
#include <vector>
#include <functional>
#include "esp_err.h"
#include "esp_wifi_types.h"
#include "esp_netif_types.h"

/**
 * @brief Wi-Fi network information structure
 */
struct WifiNetworkInfo {
    std::string ssid;
    int8_t rssi;
    wifi_auth_mode_t auth_mode;
    uint8_t channel;
};

/**
 * @brief Wi-Fi connection status
 */
enum class WifiConnectionStatus {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    FAILED
};

/**
 * @brief IP configuration mode
 */
enum class IpConfigMode {
    DHCP,
    STATIC
};

/**
 * @brief Static IP configuration
 */
struct StaticIpConfig {
    std::string ip;
    std::string gateway;
    std::string netmask;
    std::string dns1;
    std::string dns2;
};

/**
 * @brief WiFi scan callback - called when scan completes
 * @param networks Vector of discovered networks (empty on error)
 * @param err Error code (ESP_OK on success)
 */
using WifiScanCallback = std::function<void(const std::vector<WifiNetworkInfo>&, esp_err_t)>;

/**
 * @brief WiFi scan callback - called when scan completes
 * @param networks Vector of discovered networks (empty on error)
 * @param err Error code (ESP_OK on success)
 */
using WifiScanCallback = std::function<void(const std::vector<WifiNetworkInfo>&, esp_err_t)>;

/**
 * @brief WirelessManager class for managing ESP32-C6 Wi-Fi connectivity
 * 
 * This class provides an interface to control the ESP32-C6 module which is
 * connected to ESP32-P4 via SDIO. It supports:
 * - Network scanning
 * - Connection/disconnection
 * - Static IP and DHCP configuration
 * - Event callbacks for status updates
 */
class WirelessManager {
public:
    /**
     * @brief Callback type for connection status changes
     */
    using StatusCallback = std::function<void(WifiConnectionStatus status, const std::string& info)>;
    
    /**
     * @brief Callback type for IP address changes
     */
    using IpCallback = std::function<void(const std::string& ip, const std::string& netmask, const std::string& gateway)>;

    /**
     * @brief Get singleton instance
     */
    static WirelessManager& getInstance();

    /**
     * @brief Initialize the wireless manager and ESP32-C6 SDIO interface
     * 
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t init();

    /**
     * @brief Deinitialize the wireless manager
     * 
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t deinit();

    /**
     * @brief Scan for available Wi-Fi networks
     * 
     * @param networks Vector to store found networks
     * @param max_results Maximum number of results to return (0 = all)
     * @param scan_time_ms Scan duration in milliseconds
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t scan(std::vector<WifiNetworkInfo>& networks, uint16_t max_results = 0, uint32_t scan_time_ms = 3000);

    /**
     * @brief Scan for available Wi-Fi networks (non-blocking)
     * 
     * @param callback Function called when scan completes
     * @param max_results Maximum number of results to return (0 = all)
     * @return esp_err_t ESP_OK if scan started successfully
     */
    esp_err_t scanAsync(WifiScanCallback callback, uint16_t max_results = 20);

    /**
     * @brief Connect to a Wi-Fi network
     * 
     * @param ssid Network SSID
     * @param password Network password (empty for open networks)
     * @param timeout_ms Connection timeout in milliseconds
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t connect(const std::string& ssid, const std::string& password, uint32_t timeout_ms = 10000);

    /**
     * @brief Disconnect from current Wi-Fi network
     * 
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t disconnect();

    /**
     * @brief Set IP configuration mode
     * 
     * @param mode DHCP or STATIC
     * @param config Static IP configuration (required if mode is STATIC)
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t setIpConfig(IpConfigMode mode, const StaticIpConfig* config = nullptr);

    /**
     * @brief Get current connection status
     * 
     * @return WifiConnectionStatus Current status
     */
    WifiConnectionStatus getStatus() const;

    /**
     * @brief Check if connected to a network
     * 
     * @return true if connected
     */
    bool isConnected() const;

    /**
     * @brief Get current SSID
     * 
     * @return std::string Current SSID (empty if not connected)
     */
    std::string getCurrentSsid() const;

    /**
     * @brief Get current IP address
     * 
     * @return std::string Current IP address (empty if not connected)
     */
    std::string getIpAddress() const;

    /**
     * @brief Get current RSSI (signal strength)
     * 
     * @return int8_t RSSI in dBm (0 if not connected)
     */
    int8_t getRssi() const;

    /**
     * @brief Set status change callback
     * 
     * @param callback Callback function
     */
    void setStatusCallback(StatusCallback callback);

    /**
     * @brief Set IP change callback
     * 
     * @param callback Callback function
     */
    void setIpCallback(IpCallback callback);

private:
    WirelessManager();
    ~WirelessManager();
    WirelessManager(const WirelessManager&) = delete;
    WirelessManager& operator=(const WirelessManager&) = delete;

    // Event handlers
    static void wifi_event_handler(void* arg, esp_event_base_t event_base, 
                                   int32_t event_id, void* event_data);
    static void ip_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data);

    // Member variables
    bool m_initialized;
    WifiConnectionStatus m_status;
    std::string m_current_ssid;
    std::string m_current_ip;
    std::string m_current_netmask;
    std::string m_current_gateway;
    int8_t m_current_rssi;
    IpConfigMode m_ip_mode;
    StaticIpConfig m_static_config;
    
    StatusCallback m_status_callback;
    IpCallback m_ip_callback;
    
    esp_netif_t* m_sta_netif;
    esp_event_handler_instance_t m_wifi_event_handler;
    esp_event_handler_instance_t m_ip_event_handler;
    
    // Async scan support
    static void scanTask(void* arg);
    struct ScanTaskParams {
        WifiScanCallback callback;
        uint16_t max_results;
    };
};
