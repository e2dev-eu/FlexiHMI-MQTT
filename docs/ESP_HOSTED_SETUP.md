# ESP-Hosted Setup for ESP32-P4 with ESP32-C6

## Overview

This project uses [ESP-Hosted-MCU](https://github.com/espressif/esp-hosted-mcu) to enable Wi-Fi connectivity on the ESP32-P4 via the onboard ESP32-C6 module. The ESP32-C6 acts as a wireless co-processor, communicating with the ESP32-P4 over SDIO.

## Architecture

- **Host**: ESP32-P4 (main processor, no native WiFi)
- **Co-processor**: ESP32-C6-MINI-1 (wireless module)
- **Transport**: SDIO 4-bit (pre-wired on ESP32-P4-Function-EV-Board v1.4)
- **Interface**: Wi-Fi Remote API (standard ESP-IDF Wi-Fi APIs)

## Configuration

The project is configured in `sdkconfig.defaults`:

```
# ESP-Hosted configuration for ESP32-P4 + ESP32-C6
CONFIG_SLAVE_IDF_TARGET_ESP32C6=y
CONFIG_ESP_HOSTED_P4_DEV_BOARD_FUNC_BOARD=y

# Wi-Fi Remote (ESP-Hosted) buffer configuration  
CONFIG_WIFI_RMT_STATIC_RX_BUFFER_NUM=16
CONFIG_WIFI_RMT_DYNAMIC_RX_BUFFER_NUM=64
CONFIG_WIFI_RMT_DYNAMIC_TX_BUFFER_NUM=64
CONFIG_WIFI_RMT_AMPDU_TX_ENABLED=y
CONFIG_WIFI_RMT_TX_BA_WIN=32
CONFIG_WIFI_RMT_AMPDU_RX_ENABLED=y
CONFIG_WIFI_RMT_RX_BA_WIN=32
```

## SDIO Pin Connections

The ESP32-C6 is connected to ESP32-P4 via SDIO on Slot 1:

| Signal | ESP32-P4 GPIO | ESP32-C6 GPIO |
|--------|---------------|---------------|
| CLK    | 18            | 19            |
| CMD    | 19            | 18            |
| D0     | 14            | 20            |
| D1     | 15            | 21            |
| D2     | 16            | 22            |
| D3     | 17            | 23            |
| Reset  | 54            | EN            |

## ESP32-C6 Firmware

The ESP32-C6 needs to be flashed with ESP-Hosted slave firmware. 

### Option 1: Pre-flashed Firmware

ESP32-P4-Function-EV-Board v1.4 comes with ESP-Hosted slave firmware v0.0.6 pre-flashed on the ESP32-C6.

### Option 2: Flash ESP32-C6 Manually

If you need to update or reflash the ESP32-C6:

```bash
# Create the slave project
idf.py create-project-from-example "espressif/esp_hosted:slave"
cd slave

# Configure for ESP32-C6 with SDIO
idf.py set-target esp32c6
idf.py menuconfig
# Select: Example Configuration → Transport layer → SDIO

# Build and flash (requires ESP-Prog or similar UART adapter)
idf.py build
idf.py -p /dev/ttyUSB1 flash monitor
```

See [ESP-Hosted ESP32-P4 Function EV Board Guide](https://github.com/espressif/esp-hosted-mcu/blob/main/docs/esp32_p4_function_ev_board.md) for detailed flashing instructions.

## Usage in Code

ESP-Hosted is initialized in `main.cpp`:

```cpp
#include "esp_hosted.h"

// Initialize ESP-Hosted
esp_err_t ret = esp_hosted_init();
if (ret == ESP_OK) {
    ESP_LOGI(TAG, "ESP-Hosted initialized successfully");
    
    // Get co-processor info
    esp_hosted_app_desc_t desc = {};
    if (esp_hosted_get_coprocessor_app_desc(&desc) == ESP_OK) {
        ESP_LOGI(TAG, "ESP32-C6 Firmware: %s, Version: %s", 
                 desc.project_name, desc.version);
    }
}
```

After initialization, use standard ESP-IDF Wi-Fi APIs (`esp_wifi_*`) in WirelessManager - they are transparently routed to the ESP32-C6 co-processor via RPC over SDIO.

## Benefits

1. **Standard API**: Use familiar ESP-IDF Wi-Fi APIs
2. **High Performance**: SDIO 4-bit interface provides high throughput
3. **Flexibility**: ESP32-C6 handles all wireless operations, freeing ESP32-P4 resources
4. **Future-proof**: Easy firmware updates via OTA

## References

- [ESP-Hosted-MCU GitHub](https://github.com/espressif/esp-hosted-mcu)
- [ESP-Hosted SDIO Documentation](https://github.com/espressif/esp-hosted-mcu/blob/main/docs/sdio.md)
- [ESP32-P4 Function EV Board Setup](https://github.com/espressif/esp-hosted-mcu/blob/main/docs/esp32_p4_function_ev_board.md)
- [Wi-Fi Remote Component](https://components.espressif.com/components/espressif/esp_wifi_remote)
