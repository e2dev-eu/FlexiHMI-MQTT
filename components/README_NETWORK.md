# Network Managers for ESP32-P4

This directory contains network management components for the ESP32-P4-Function-EV-Board.

## Components

### 1. WirelessManager

Manages Wi-Fi connectivity through the ESP32-C6 module (connected via SDIO).

**Features:**
- Network scanning
- Connect/disconnect to Wi-Fi networks
- Support for DHCP and static IP configuration
- Connection status callbacks
- IP address change callbacks
- RSSI (signal strength) monitoring

**Usage Example:**

```cpp
#include "wireless_manager.h"

// Get singleton instance
WirelessManager& wifi = WirelessManager::getInstance();

// Initialize
wifi.init();

// Set callbacks
wifi.setStatusCallback([](WifiConnectionStatus status, const std::string& info) {
    ESP_LOGI("APP", "Wi-Fi status: %s", info.c_str());
});

wifi.setIpCallback([](const std::string& ip, const std::string& netmask, const std::string& gateway) {
    ESP_LOGI("APP", "Wi-Fi IP: %s", ip.c_str());
});

// Scan for networks
std::vector<WifiNetworkInfo> networks;
if (wifi.scan(networks) == ESP_OK) {
    for (const auto& net : networks) {
        ESP_LOGI("APP", "Found: %s (RSSI: %d dBm)", net.ssid.c_str(), net.rssi);
    }
}

// Connect to network
wifi.connect("MySSID", "MyPassword");

// Check status
if (wifi.isConnected()) {
    ESP_LOGI("APP", "Connected to: %s", wifi.getCurrentSsid().c_str());
    ESP_LOGI("APP", "IP: %s", wifi.getIpAddress().c_str());
}

// Configure static IP
StaticIpConfig static_ip;
static_ip.ip = "192.168.1.100";
static_ip.gateway = "192.168.1.1";
static_ip.netmask = "255.255.255.0";
static_ip.dns1 = "8.8.8.8";
static_ip.dns2 = "8.8.4.4";
wifi.setIpConfig(IpConfigMode::STATIC, &static_ip);

// Or use DHCP
wifi.setIpConfig(IpConfigMode::DHCP);

// Disconnect
wifi.disconnect();
```

### 2. LanManager

Manages Ethernet connectivity on the ESP32-P4.

**Features:**
- Works with existing ethernet_init() implementation
- Support for DHCP and static IP configuration
- Link status monitoring
- Connection status callbacks
- IP address change callbacks
- MAC address retrieval

**Usage Example:**

```cpp
#include "lan_manager.h"

// Get singleton instance
LanManager& lan = LanManager::getInstance();

// Initialize (uses existing ethernet_init if available)
lan.init();

// Set callbacks
lan.setStatusCallback([](EthConnectionStatus status, const std::string& info) {
    ESP_LOGI("APP", "Ethernet status: %s", info.c_str());
});

lan.setIpCallback([](const std::string& ip, const std::string& netmask, const std::string& gateway) {
    ESP_LOGI("APP", "Ethernet IP: %s", ip.c_str());
});

// Check status
if (lan.isConnected()) {
    ESP_LOGI("APP", "Ethernet connected");
    ESP_LOGI("APP", "IP: %s", lan.getIpAddress().c_str());
    ESP_LOGI("APP", "MAC: %s", lan.getMacAddress().c_str());
}

// Configure static IP
EthStaticIpConfig static_ip;
static_ip.ip = "192.168.1.50";
static_ip.gateway = "192.168.1.1";
static_ip.netmask = "255.255.255.0";
static_ip.dns1 = "8.8.8.8";
static_ip.dns2 = "8.8.4.4";
lan.setIpConfig(EthIpConfigMode::STATIC, &static_ip);

// Or use DHCP
lan.setIpConfig(EthIpConfigMode::DHCP);
```

## ESP32-C6 Wireless Initialization

The ESP32-P4-Function-EV-Board includes an ESP32-C6-MINI-1 module for Wi-Fi and Bluetooth connectivity. The ESP32-C6 is connected to the ESP32-P4 via SDIO interface.

### Recommended Setup Approaches

#### Option 1: ESP-Hosted Framework (Recommended for Production)

For production use, it's recommended to use the [ESP-Hosted-MCU](https://github.com/espressif/esp-hosted-mcu) framework, which provides a complete solution for using ESP32-C6 as a wireless co-processor.

**Benefits:**
- Full-featured Wi-Fi and Bluetooth support
- Optimized for low power consumption
- Well-tested and maintained by Espressif
- Supports SPI, SDIO, and UART interfaces

#### Option 2: Native ESP-IDF Wi-Fi API (Current Implementation)

The WirelessManager uses the standard ESP-IDF Wi-Fi API. This approach assumes the ESP32-C6 is running firmware that exposes a standard Wi-Fi interface to the ESP32-P4.

**Note:** You may need to flash appropriate firmware to the ESP32-C6 module to enable the SDIO interface. Refer to the board documentation and ESP-Hosted examples for details.

## Integration

To use these managers in your project, add them as dependencies in your component's CMakeLists.txt:

```cmake
idf_component_register(
    ...
    REQUIRES
        wireless_manager
        lan_manager
)
```

## Hardware Configuration

### ESP32-C6 Connection
- Interface: SDIO
- Module: ESP32-C6-MINI-1
- Located on the ESP32-P4-Function-EV-Board

### Ethernet Connection
- PHY: IP101GR
- Interface: RMII
- Configured in the existing ethernet component

## Notes

1. **WirelessManager** provides a high-level interface for Wi-Fi control. The actual SDIO communication with ESP32-C6 is handled by the ESP-IDF Wi-Fi driver or ESP-Hosted framework.

2. **LanManager** wraps the existing Ethernet functionality and adds IP configuration features without disrupting the current implementation.

3. Both managers use singleton patterns for easy access throughout the application.

4. Event callbacks are executed in the context of the event loop task. Keep callback processing lightweight or defer heavy work to another task.

## Future Enhancements

- [ ] Add support for Wi-Fi AP mode (WirelessManager)
- [ ] Add support for Wi-Fi STA+AP mode
- [ ] Add Bluetooth/BLE support (requires ESP32-C6 firmware configuration)
- [ ] Add WPA3 and enterprise security modes
- [ ] Add power management features
- [ ] Add network statistics (bytes sent/received, packet loss, etc.)
- [ ] Add automatic reconnection logic with backoff
- [ ] Add network priority and multi-network support
