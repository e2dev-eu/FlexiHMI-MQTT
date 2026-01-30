# Settings UI Redesign - Tabbed Interface

## Overview
The configuration/status UI has been redesigned from separate settings and status windows into a unified tabbed interface accessed via a single gear icon in the bottom-right corner.

## Changes Made

### 1. **Unified Gear Icon**
- **Location**: Bottom-right corner of the screen
- **Size**: 60x60 pixels
- **Replaces**: Previously had gear icon (top-right) + info icon
- **Access**: Single tap opens tabbed settings window

### 2. **Tabbed Settings Window**
The settings screen now uses LVGL's tabview widget with 4 tabs:

#### **Tab 1: MQTT Configuration**
- Broker URI input
- Username input  
- Password input (masked)
- Client ID input
- Config Topic input
- Save & Apply button
- Reconnects MQTT on save

#### **Tab 2: LAN Configuration**
- DHCP Enable/Disable switch
- Static IP configuration (disabled when DHCP is on):
  * IP Address
  * Netmask
  * Gateway
- Connection status display (shows current IP)
- Apply button to save and reconfigure network

#### **Tab 3: WiFi Configuration**
- Scan button (with refresh icon)
- Scrollable AP list:
  * Signal strength color-coded:
    - Green: > -50 dBm (excellent)
    - Yellow: -50 to -70 dBm (good)
    - Red: < -70 dBm (weak)
  * WiFi icon for secured networks
- Password input (appears when AP selected)
- Connect button
- Connection status display

#### **Tab 4: About**
- Application version (from esp_app_desc)
- Build date and time
- ESP-IDF version
- Hardware list:
  * ESP32-P4 Function EV Board
  * 800x600 IPS LCD Display
  * GT911 Touch Controller
  * Ethernet PHY (W5500)
  * ESP32-C6 WiFi (SDIO)
- Features list:
  * MQTT Broker Connection
  * Dynamic Widget System
  * Dual Network Support
  * LVGL UI Framework

## Modified Files

### Core Implementation
- **components/settings_ui/include/settings_ui.h**
  * Added `WifiAP` struct for scan results
  * Added tab UI object pointers
  * Added tab creation methods
  * Added WiFi/LAN status update methods

- **components/settings_ui/settings_ui.cpp**
  * Repositioned gear icon to LV_ALIGN_BOTTOM_RIGHT
  * Replaced single settings screen with tabview
  * Implemented 4 tab creation functions
  * Added WiFi scan/connect functionality
  * Added LAN DHCP/static IP configuration
  * Integrated with WirelessManager and LanManager

- **components/settings_ui/CMakeLists.txt**
  * Added dependencies: `wireless_manager`, `lan_manager`, `esp_app_format`

### Main Application
- **main/main.cpp**
  * Removed StatusInfoUI include and initialization
  * Removed separate info icon
  * Status updates now integrated into settings tabs

## API Changes

### New Public Methods
```cpp
// Update status displays (called from network managers)
void updateEthStatus(const std::string& status);
void updateWifiStatus(const std::string& status);
void updateWifiScanResults(const std::vector<WifiAP>& aps);
```

### Removed Components
- **status_info_ui** - No longer initialized (icon removed)
- Info icon has been consolidated into the single gear icon interface

## User Experience

### Before
1. Gear icon (top-right) → MQTT settings only
2. Info icon (top-left) → Network status readonly display

### After
1. Gear icon (bottom-right) → Complete tabbed interface:
   - MQTT configuration
   - LAN configuration with DHCP/Static IP
   - WiFi scanning and connection
   - System information

## Technical Details

### LVGL Components Used
- `lv_tabview` - Main tab container
- `lv_switch` - DHCP enable/disable toggle
- `lv_list` - Scrollable WiFi AP list
- `lv_textarea` - All text input fields
- `lv_button` - Action buttons (scan, connect, save, apply)
- `lv_label` - Status displays and information text
- `lv_keyboard` - On-screen keyboard for text entry

### Integration Points
- **WirelessManager**: WiFi scan results, connection status
- **LanManager**: Ethernet configuration, DHCP/Static IP management
- **MQTTManager**: Broker connection and configuration
- **ConfigManager**: NVS storage for persistent settings

## Build Statistics
- Binary size: 1.5 MB (1,538,352 bytes)
- Partition usage: 74% used, 26% free (550 KB available)
- Successfully builds with ESP-IDF v5.5.2

## Testing Recommendations

1. **Tab Navigation**: Verify smooth switching between all 4 tabs
2. **MQTT Tab**: Test save with various broker configurations
3. **LAN Tab**: Test DHCP switch enables/disables static IP fields properly
4. **WiFi Tab**: 
   - Test WiFi scan populates list
   - Verify signal strength color coding
   - Test AP selection and password input
   - Verify connection status updates
5. **About Tab**: Verify all information displays correctly
6. **Keyboard**: Test on-screen keyboard appears/dismisses on text input focus
7. **Gear Icon**: Verify position in bottom-right, proper size

## Future Enhancements

Possible additions:
- WiFi connection strength indicator on status bar
- Network reconnection status notifications
- Configuration export/import functionality
- Multiple WiFi AP profiles
- MQTT QoS configuration options
