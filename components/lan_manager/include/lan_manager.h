#pragma once

#include <string>
#include <functional>
#include "esp_err.h"
#include "esp_eth.h"
#include "esp_netif_types.h"

/**
 * @brief Ethernet connection status
 */
enum class EthConnectionStatus {
    DISCONNECTED,
    LINK_DOWN,
    LINK_UP,
    CONNECTED
};

/**
 * @brief IP configuration mode
 */
enum class EthIpConfigMode {
    DHCP,
    STATIC
};

/**
 * @brief Static IP configuration for Ethernet
 */
struct EthStaticIpConfig {
    std::string ip;
    std::string gateway;
    std::string netmask;
    std::string dns1;
    std::string dns2;
};

/**
 * @brief LanManager class for managing Ethernet connectivity
 * 
 * This class provides an interface to control the Ethernet connection on ESP32-P4.
 * It wraps the existing ethernet_init functionality and adds:
 * - Static IP and DHCP configuration
 * - Connection status monitoring
 * - Event callbacks for status updates
 */
class LanManager {
public:
    /**
     * @brief Callback type for connection status changes
     */
    using StatusCallback = std::function<void(EthConnectionStatus status, const std::string& info)>;
    
    /**
     * @brief Callback type for IP address changes
     */
    using IpCallback = std::function<void(const std::string& ip, const std::string& netmask, const std::string& gateway)>;

    /**
     * @brief Get singleton instance
     */
    static LanManager& getInstance();

    /**
     * @brief Initialize the LAN manager
     * 
     * Note: This will use the existing ethernet_init() if already initialized
     * 
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t init();

    /**
     * @brief Deinitialize the LAN manager
     * 
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t deinit();

    /**
     * @brief Set IP configuration mode
     * 
     * @param mode DHCP or STATIC
     * @param config Static IP configuration (required if mode is STATIC)
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t setIpConfig(EthIpConfigMode mode, const EthStaticIpConfig* config = nullptr);

    /**
     * @brief Get current connection status
     * 
     * @return EthConnectionStatus Current status
     */
    EthConnectionStatus getStatus() const;

    /**
     * @brief Check if connected (link up and has IP)
     * 
     * @return true if connected
     */
    bool isConnected() const;

    /**
     * @brief Get current IP address
     * 
     * @return std::string Current IP address (empty if not connected)
     */
    std::string getIpAddress() const;

    /**
     * @brief Get current netmask
     * 
     * @return std::string Current netmask (empty if not connected)
     */
    std::string getNetmask() const;

    /**
     * @brief Get current gateway
     * 
     * @return std::string Current gateway (empty if not connected)
     */
    std::string getGateway() const;

    /**
     * @brief Get MAC address
     * 
     * @return std::string MAC address in format XX:XX:XX:XX:XX:XX
     */
    std::string getMacAddress() const;

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
    LanManager();
    ~LanManager();
    LanManager(const LanManager&) = delete;
    LanManager& operator=(const LanManager&) = delete;

    // Event handlers
    static void eth_event_handler(void* arg, esp_event_base_t event_base, 
                                  int32_t event_id, void* event_data);
    static void ip_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data);

    // Member variables
    bool m_initialized;
    bool m_using_existing_eth;  // True if using ethernet_init()
    EthConnectionStatus m_status;
    std::string m_current_ip;
    std::string m_current_netmask;
    std::string m_current_gateway;
    std::string m_mac_address;
    EthIpConfigMode m_ip_mode;
    EthStaticIpConfig m_static_config;
    
    StatusCallback m_status_callback;
    IpCallback m_ip_callback;
    
    esp_netif_t* m_eth_netif;
    esp_eth_handle_t m_eth_handle;
    esp_event_handler_instance_t m_eth_event_handler;
    esp_event_handler_instance_t m_ip_event_handler;
};
