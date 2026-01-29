# ESP32-P4 LVGL9 Panel

ESP32-P4-Function-EV-Board 1.4 project with LVGL 9.2, display (EK79007), and touchscreen support.

## Hardware

- **Board**: ESP32-P4-Function-EV-Board 1.4
- **Display**: 1024x600 MIPI DSI display (EK79007 controller)
- **Touch**: GT911 capacitive touch controller
- **Interface**: MIPI DSI for display, I2C for touch

## Features

- **LVGL 9.2** with official ESP-IDF integration
- Official ESP32-P4-Function-EV-Board BSP component
- C++ main application called from C entry point
- Separate HMI task for UI updates
- Thread-safe display access with mutex
- Demo UI with label and button
- EK79007 display driver with MIPI DSI (2 lanes @ 1000 Mbps)

## Project Structure

```
ESP32P4-MQTT-Panel/
├── CMakeLists.txt              # Root CMake file
├── sdkconfig.defaults          # ESP32-P4 configuration (LVGL 9)
├── main/
│   ├── CMakeLists.txt
│   ├── idf_component.yml      # LVGL 9.2 and BSP dependencies
│   ├── main.c                  # C entry point, BSP initialization
│   └── main.cpp                # C++ main with HMI task
└── managed_components/         # Downloaded from component registry
    ├── lvgl__lvgl/             # LVGL 9.2.2
    ├── espressif__esp32_p4_function_ev_board/  # Official BSP
    ├── espressif__esp_lcd_ek79007/             # EK79007 display driver
    ├── espressif__esp_lcd_touch_gt911/         # GT911 touch driver
    └── espressif__esp_lvgl_port/               # LVGL ESP32 port
```

## Building

1. Set up ESP-IDF environment:
```bash
get_idf
```

2. Build the project (dependencies will be downloaded automatically):
```bash
idf.py build
```

3. Flash to the board:
```bash
idf.py flash monitor
```

## Adding Components

To add ESP-IDF managed components, edit [main/idf_component.yml](main/idf_component.yml) and add dependencies. For custom components:

1. Create a new directory in `components/`
2. Add CMakeLists.txt with `idf_component_register()`
3. The component will be automatically included in the build

## HMI Task

The HMI runs in a separate FreeRTOS task with priority 4 and 8KB stack. Display access is protected by the LVGL port's built-in locking mechanism (`bsp_display_lock`/`unlock`).

## Display Configuration

- **Resolution**: 1024x600
- **Controller**: EK79007
- **Interface**: MIPI DSI (2 lanes @ 1000 Mbps)
- **Color depth**: 16-bit (RGB565) 
- **Backlight**: GPIO 26
- **Reset**: GPIO 27
- **Buffer**: Double buffering with DMA

## Touch Configuration

- **Controller**: GT911
- **Interface**: I2C (SCL: GPIO 8, SDA: GPIO 7)
- **Interrupt**: GPIO 3
- **Resolution**: Up to 1024x600 points
