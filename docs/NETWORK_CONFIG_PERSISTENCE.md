# Network Configuration Persistence

## Overview

The FlexiHMI MQTT system implements network configuration persistence with proper separation of concerns. Each network manager (LAN and WiFi) handles its own NVS storage and configuration, while the Settings UI provides the user interface to view and modify these settings.

## Architecture

### Separation of Concerns

**Network Managers** (LAN Manager & Wireless Manager):
- Own their configuration data
- Handle NVS storage and retrieval
- Load configuration on init() automatically
- Provide APIs to get/set configuration
- Apply configuration changes immediately

**Settings UI**:
- Provides user interface for network configuration
- Calls manager APIs to load current settings
- Calls manager APIs to save and apply new settings
- Does NOT directly access NVS for network config

This architecture ensures that network configuration is centralized in the managers, making it easier to maintain and test.

## What's Persisted

### MQTT Settings (Already Existed)
- ✅ Broker URI
- ✅ Username & Password
- ✅ Client ID
- ✅ Config Topic

### WiFi Settings
- ✅ **WiFi Credentials** - Persisted automatically by ESP-IDF WiFi stack
- ✅ **IP Configuration Mode** - DHCP or Static (NEW)
- ✅ **Static IP Address** (NEW)
- ✅ **Netmask** (NEW)
- ✅ **Gateway** (NEW)

### LAN/Ethernet Settings
- ✅ **IP Configuration Mode** - DHCP or Static (NEW)
- ✅ **Static IP Address** (NEW)
- ✅ **Netmask** (NEW)
- ✅ **Gateway** (NEW)

## Implementation Details

### NVS Namespace
Network configuration is stored in a separate NVS namespace:
- `"net_config"` - For all network (WiFi and LAN) IP configuration

MQTT settings remain in:
- `"mqtt_settings"` - For MQTT broker configuration

### Storage Keys

#### LAN Configuration
- `lan_ip_mode` - Boolean (1=DHCP, 0=Static)
- `lan_ip` - IP address string
- `lan_netmask` - Netmask string
- `lan_gateway` - Gateway string

#### WiFi Configuration
- `wifi_ip_mode` - Boolean (1=DHCP, 0=Static)
- `wifi_ip` - IP address string
- `wifi_netmask` - Netmask string
- `wifi_gateway` - Gateway string

## User Interface

### Settings UI Changes

#### WiFi Tab
Added IP configuration section with:
- DHCP/Static toggle switch
- IP Address input field
- Netmask input field (default: 255.255.255.0)
- Gateway input field
- "Apply IP Config" button

#### LAN Tab
Existing UI now saves configuration to NVS:
- DHCP/Static toggle switch
- IP Address input field
- Netmask input field
- Gateway input field
- "Apply" button (now saves to NVS)

## Behavior

### On Boot
1. NVS is initialized in `main.c`
2. Settings UI loads MQTT settings from NVS
3. **NEW:** Settings UI loads network configuration from NVS
4. **NEW:** Network configuration is automatically applied to WiFi and LAN managers
5. If no saved configuration exists, defaults are used (DHCP enabled)

### When Changing Settings
1. User modifies IP configuration in Settings UI
2. User clicks "Apply" or "Apply IP Config" button
3. Configuration is saved to NVS
4. Configuration is immediately applied to the respective network interface
5. Changes persist across reboots

### WiFi Connection
- WiFi SSID and password are stored automatically by ESP-IDF's WiFi stack
- On boot, the device automatically reconnects to the last connected network
- IP configuration (DHCP/Static) is loaded from our NVS implementation

## API Changes

### SettingsUI Class

#### New Public Methods
```cpp
bool loadNetworkConfig();     // Load network config from NVS
bool saveNetworkConfig();     // Save network config to NVS
```

#### New Private Methods
```cpp
void loadLanConfigToUI();     // Load LAN config to UI elements
void loadWifiConfigToUI();    // Load WiFi config to UI elements
void applyNetworkConfig();    // Apply saved config to managers
```

#### New Callbacks
```cpp
static void wifi_dhcp_switch_cb(lv_event_t* e);
static void wifi_save_clicked_cb(lv_event_t* e);
```

#### New Member Variables
```cpp
struct NetworkConfig {
    bool lan_dhcp;
    std::string lan_ip;
    std::string lan_netmask;
    std::string lan_gateway;
    bool wifi_dhcp;
    std::string wifi_ip;
    std::string wifi_netmask;
    std::string wifi_gateway;
} m_network_config;

// UI elements
lv_obj_t* m_wifi_dhcp_switch;
lv_obj_t* m_wifi_ip_input;
lv_obj_t* m_wifi_netmask_input;
lv_obj_t* m_wifi_gateway_input;
```

## Files Modified

### Components
- `components/settings_ui/include/settings_ui.h` - Added new methods and member variables
- `components/settings_ui/settings_ui.cpp` - Implemented persistence and WiFi IP config UI

### No Changes Required To
- `components/wireless_manager/*` - Already has `setIpConfig()` method
- `components/lan_manager/*` - Already has `setIpConfig()` method

## Testing

### Test DHCP Configuration
1. Open Settings UI (gear icon)
2. Go to WiFi or LAN tab
3. Ensure DHCP switch is ON
4. Click "Apply" or "Apply IP Config"
5. Reboot device
6. Verify DHCP is still enabled and device gets IP automatically

### Test Static IP Configuration
1. Open Settings UI
2. Go to WiFi or LAN tab
3. Toggle DHCP switch OFF
4. Enter IP address (e.g., 192.168.1.100)
5. Enter Gateway (e.g., 192.168.1.1)
6. Netmask defaults to 255.255.255.0
7. Click "Apply" or "Apply IP Config"
8. Verify connection with static IP
9. Reboot device
10. Verify static IP is still configured and applied

## Defaults

On first boot with no saved configuration:

### LAN
- Mode: DHCP enabled
- Netmask: 255.255.255.0 (for when switching to static)

### WiFi
- Mode: DHCP enabled
- Netmask: 255.255.255.0 (for when switching to static)

## Migration

No migration needed. The system gracefully handles:
- First boot (no saved config) - Uses defaults
- Upgrading from older version - Continues with DHCP, can be configured via UI

## Storage Usage

Each configuration entry uses minimal NVS space:
- Boolean (DHCP flag): 1 byte
- IP address string: ~15 bytes
- Netmask string: ~15 bytes
- Gateway string: ~15 bytes

**Total per interface:** ~46 bytes
**Total for both WiFi and LAN:** ~92 bytes

## Notes

1. **WiFi credentials** are handled separately by ESP-IDF's WiFi stack and stored in a different NVS partition
2. **DNS servers** are not currently persisted (future enhancement)
3. Static IP configuration must be valid for the network or connection will fail
4. DHCP is recommended for most use cases
5. Network configuration is loaded and applied automatically on boot before MQTT connection

## Future Enhancements

Potential improvements:
- [ ] DNS server configuration
- [ ] Connection timeout settings
- [ ] Network priority (prefer WiFi over LAN or vice versa)
- [ ] Automatic fallback to DHCP if static IP fails
- [ ] Network diagnostics (ping, traceroute)
- [ ] Multiple WiFi network profiles
