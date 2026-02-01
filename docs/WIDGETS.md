# HMI Widget System Documentation

This document describes the HMI widget system for the ESP32-P4 MQTT Panel, including all 14 currently implemented widgets and potential future widgets based on LVGL components.

## System Capabilities
- **MQTT Buffer:** 32KB with automatic chunked message handling
- **Large Configurations:** Supports JSON configs up to 32KB
- **Thread Safety:** Queue-based config updates, async LVGL calls
- **Color Support:** Hex color format (#RRGGBB) for all visual elements
- **Bidirectional MQTT:** Widgets publish state changes and subscribe to updates
- **Feedback Prevention:** Automatic loop prevention on MQTT updates

## Table of Contents
- [Currently Implemented Widgets](#currently-implemented-widgets)
- [Planned Widgets](#planned-widgets)
- [Widget Properties Reference](#widget-properties-reference)

---

## Currently Implemented Widgets

### 1. Label Widget

**Description:** Displays static or dynamic text. Can be updated via MQTT messages.

**LVGL Component:** `lv_label`

**Implementation Features:**
- Text display with word wrapping
- Printf-style formatting for string substitution
- MQTT subscription for dynamic updates
- Custom text color support
- Text alignment and font size control

**JSON Example:**
```json
{
  "type": "label",
  "id": "temp_label",
  "x": 10,
  "y": 50,
  "w": 200,
  "h": 30,
  "properties": {
    "text": "Temperature: --",
    "format": "Temp: %s°C",
    "mqtt_topic": "sensors/temperature",
    "color": "#FFFFFF",
    "align": "center",
    "font_size": 16
  }
}
```

**Properties:**
- `text` (string): Initial text to display
- `format` (string, optional): Printf-style format string with %s placeholder for MQTT payload (e.g., "Value: %s")
- `mqtt_topic` (string, optional): Topic to subscribe for updates
- `color` (string, optional): Text color in hex format (e.g., "#FFFFFF")
- `align` (string, optional): Text alignment - "left", "center", or "right" (default: "left")
- `font_size` (number, optional): Font size in points (10, 12, 14, 16, 18, 20, 24, 28, 32, 36, 48)

**MQTT Format Notes:**
- The `format` property uses `%s` to substitute the MQTT payload as a string
- Example: Format `"Speed: %s km/h"` with payload `"120"` displays "Speed: 120 km/h"
- For numeric formatting, send pre-formatted strings via MQTT

---

### 2. Button Widget

**Description:** Interactive button that publishes MQTT messages when clicked.

**LVGL Component:** `lv_button` + `lv_label`

**Implementation Features:**
- Click event handling
- MQTT message publishing on click
- Custom button color support
- Text label on button

**JSON Example:**
```json
{
  "type": "button",
  "id": "light_toggle",
  "x": 50,
  "y": 100,
  "w": 150,
  "h": 60,
  "properties": {
    "text": "Toggle Light",
    "mqtt_topic": "home/light/command",
    "mqtt_payload": "TOGGLE",
    "mqtt_retained": false,
    "color": "#2196F3"
  }
}
```

**Properties:**
- `text` (string): Button label text
- `mqtt_topic` (string): MQTT topic to publish to on click
- `mqtt_payload` (string): Payload to send when clicked
- `mqtt_retained` (boolean, optional): Publish as retained message (default: false)
- `color` (string, optional): Button background color in hex format

---

### 3. Container Widget

**Description:** Layout container for grouping child widgets with relative positioning.

**LVGL Component:** `lv_obj` (base object)

**Implementation Features:**
- Parent-child widget hierarchy
- Relative coordinate system for children
- Customizable background color, border, and padding
- Supports nested containers

**JSON Example:**
```json
{
  "type": "container",
  "id": "control_panel",
  "x": 20,
  "y": 20,
  "w": 400,
  "h": 300,
  "properties": {
    "bg_color": "#1E1E1E",
    "border_width": 2,
    "padding": 10
  },
  "children": [
    {
      "type": "label",
      "id": "panel_title",
      "x": 10,
      "y": 10,
      "w": 200,
      "h": 30,
      "properties": {
        "text": "Control Panel"
      }
    }
  ]
}
```

**Properties:**
- `bg_color` (string, optional): Background color in hex format
- `border_width` (number, optional): Border width in pixels
- `padding` (number, optional): Internal padding in pixels
- `children` (array, optional): Array of child widgets with relative coordinates

---

### 4. Switch Widget

**Description:** Toggle switch with bidirectional MQTT communication.

**LVGL Component:** `lv_switch`

**Implementation Features:**
- Toggle state (ON/OFF)
- MQTT publish on state change
- MQTT subscribe for external updates
- Retained message support
- Feedback loop prevention
- Custom indicator color

**JSON Example:**
```json
{
  "type": "switch",
  "id": "fan_switch",
  "x": 100,
  "y": 150,
  "w": 60,
  "h": 30,
  "properties": {
    "state": false,
    "mqtt_topic": "home/fan/state",
    "mqtt_retained": true,
    "color": "#4CAF50"
  }
}
```

**Properties:**
- `state` (boolean): Initial state (true = ON, false = OFF)
- `mqtt_topic` (string): Topic for publish/subscribe
- `mqtt_retained` (boolean, optional): Publish as retained message (default: true)
- `color` (string, optional): Switch indicator color in hex format

---

### 5. Slider Widget

**Description:** Slider control for numeric values with bidirectional MQTT communication.

**LVGL Component:** `lv_slider` + `lv_label`

**Implementation Features:**
- Adjustable range (min/max)
- Value label display
- Optional title label
- MQTT publish on value change
- MQTT subscribe for external updates
- Retained message support
- Feedback loop prevention
- Custom slider color

**JSON Example:**
```json
{
  "type": "slider",
  "id": "brightness",
  "x": 50,
  "y": 200,
  "w": 300,
  "h": 20,
  "properties": {
    "label": "Brightness",
    "min": 0,
    "max": 255,
    "value": 128,
    "mqtt_topic": "home/light/brightness",
    "mqtt_retained": false,
    "color": "#FFEB3B"
  }
}
```

**Properties:**
- `label` (string, optional): Title label above slider
- `min` (number): Minimum value (default: 0)
- `max` (number): Maximum value (default: 100)
- `value` (number): Initial value
- `mqtt_topic` (string): Topic for publish/subscribe
- `mqtt_retained` (boolean, optional): Publish as retained message (default: true)
- `color` (string, optional): Slider indicator/knob color in hex format

---

### 6. Arc Widget

**Description:** Circular arc control for gauges or rotary inputs.

**LVGL Component:** `lv_arc`

**Implementation Features:**
- Circular slider/gauge
- Configurable value range (min/max)
- MQTT bidirectional control
- Feedback loop prevention
- Custom arc color
- Touch interaction support

**JSON Example:**
```json
{
  "type": "arc",
  "id": "volume_control",
  "x": 100,
  "y": 100,
  "w": 150,
  "h": 150,
  "properties": {
    "min": 0,
    "max": 100,
    "value": 50,
    "mqtt_topic": "audio/volume",
    "mqtt_retained": true,
    "color": "#9C27B0"
  }
}
```

**Properties:**
- `min` (number): Minimum value (default: 0)
- `max` (number): Maximum value (default: 100)
- `value` (number): Initial value
- `mqtt_topic` (string): Topic for publish/subscribe
- `mqtt_retained` (boolean, optional): Publish as retained message
- `color` (string, optional): Arc indicator color in hex format

---

### 7. Dropdown Widget

**Description:** Dropdown selection list.

**LVGL Component:** `lv_dropdown`

**Implementation Features:**
- Multiple choice selection
- String-based options list
- MQTT publish on selection change
- MQTT subscribe to set selection
- Retained message support
- Custom dropdown color

**JSON Example:**
```json
{
  "type": "dropdown",
  "id": "mode_selector",
  "x": 100,
  "y": 150,
  "w": 180,
  "h": 40,
  "properties": {
    "options": ["Auto", "Heat", "Cool", "Fan"],
    "selected": 0,
    "mqtt_topic": "hvac/mode",
    "mqtt_retained": true,
    "color": "#03A9F4"
  }
}
```

**Properties:**
- `options` (array): List of string options
- `selected` (number): Initial selected index (0-based)
- `mqtt_topic` (string): Topic for publish/subscribe
- `mqtt_retained` (boolean, optional): Publish as retained message
- `color` (string, optional): Dropdown color in hex format

---

### 8. Checkbox Widget

**Description:** Checkbox for boolean selection with text label.

**LVGL Component:** `lv_checkbox`

**Implementation Features:**
- Checkbox with text label
- MQTT bidirectional control
- True/false state publishing
- Retained message support
- Custom checkbox color

**JSON Example:**
```json
{
  "type": "checkbox",
  "id": "enable_notifications",
  "x": 50,
  "y": 300,
  "w": 250,
  "h": 40,
  "properties": {
    "text": "Enable Notifications",
    "checked": true,
    "mqtt_topic": "settings/notifications",
    "mqtt_retained": true,
    "color": "#4CAF50"
  }
}
```

**Properties:**
- `text` (string): Label text next to checkbox
- `checked` (boolean): Initial checked state
- `mqtt_topic` (string): Topic for publish/subscribe
- `mqtt_retained` (boolean, optional): Publish as retained message
- `color` (string, optional): Checkbox color in hex format

---

### 9. Bar Widget

**Description:** Progress bar or level indicator (read-only).

**LVGL Component:** `lv_bar`

**Implementation Features:**
- Horizontal or vertical bar
- Read-only progress indicator
- MQTT subscription for value updates
- Configurable min/max range
- Custom bar color
- Perfect for battery levels, progress indicators

**JSON Example:**
```json
{
  "type": "bar",
  "id": "battery_level",
  "x": 50,
  "y": 50,
  "w": 200,
  "h": 20,
  "properties": {
    "min": 0,
    "max": 100,
    "value": 75,
    "mqtt_topic": "device/battery",
    "color": "#4CAF50"
  }
}
```

**Properties:**
- `min` (number): Minimum value (default: 0)
- `max` (number): Maximum value (default: 100)
- `value` (number): Initial value
- `mqtt_topic` (string): Topic to subscribe for updates
- `color` (string, optional): Bar indicator color in hex format

---

### 10. LED Widget

**Description:** Status LED indicator with brightness control.

**LVGL Component:** `lv_led`

**Implementation Features:**
- ON/OFF state with brightness
- MQTT subscription for state updates
- Configurable ON/OFF colors
- Brightness levels 0-255
- Perfect for status indicators

**JSON Example:**
```json
{
  "type": "led",
  "id": "status_led",
  "x": 100,
  "y": 100,
  "w": 30,
  "h": 30,
  "properties": {
    "brightness": 255,
    "mqtt_topic": "device/status",
    "color_on": "#00FF00",
    "color_off": "#404040"
  }
}
```

**Properties:**
- `brightness` (number): Initial brightness 0-255 (0=OFF)
- `mqtt_topic` (string): Topic to subscribe for updates
- `color_on` (string, optional): LED color when ON (hex format)
- `color_off` (string, optional): LED color when OFF (hex format)

---

### 11. Spinner Widget

**Description:** Loading/activity spinner animation.

**LVGL Component:** `lv_spinner`

**Implementation Features:**
- Animated spinning arc
- MQTT control to show/hide
- Custom spinner color
- Perfect for loading indicators

**JSON Example:**
```json
{
  "type": "spinner",
  "id": "loading_indicator",
  "x": 200,
  "y": 150,
  "w": 50,
  "h": 50,
  "properties": {
    "mqtt_topic": "ui/loading",
    "color": "#2196F3"
  }
}
```

**Properties:**
- `mqtt_topic` (string): Topic to subscribe for show/hide commands
- `color` (string, optional): Spinner color in hex format

---

### 12. Tabview Widget

**Description:** Tabbed interface for organizing multiple views.

**LVGL Component:** `lv_tabview`

**Implementation Features:**
- Multiple tabs with content panels
- Swipe between tabs with animation
- Each tab can contain child widgets
- MQTT bidirectional control for active tab
- Feedback loop prevention
- Tab name or index based switching

**JSON Example:**
```json
{
  "type": "tabview",
  "id": "main_tabs",
  "x": 0,
  "y": 0,
  "w": 1024,
  "h": 600,
  "properties": {
    "tabs": ["Controls", "Status", "Settings"],
    "active_tab": 0,
    "mqtt_topic": "ui/active_tab",
    "mqtt_retained": true,
    "bg_color": "#1E1E1E",
    "tab_bg_color": "#0D1117",
    "active_tab_color": "#58A6FF",
    "tab_text_color": "#C9D1D9"
  },
  "children": {
    "Controls": [
      {
        "type": "label",
        "id": "controls_title",
        "x": 10,
        "y": 10,
        "w": 300,
        "h": 40,
        "properties": {
          "text": "Control Panel",
          "color": "#FFFFFF"
        }
      }
    ],
    "Status": [],
    "Settings": []
  }
}
```

**Properties:**
- `tabs` (array): List of tab names (strings)
- `active_tab` (number): Initial active tab index (default: 0)
- `mqtt_topic` (string): Topic for publish/subscribe
- `mqtt_retained` (boolean, optional): Publish as retained message
- `bg_color` (string, optional): Background color of content area in hex format
- `tab_bg_color` (string, optional): Background color of tab bar in hex format
- `active_tab_color` (string, optional): Color of active tab indicator in hex format
- `tab_text_color` (string, optional): Text color of tab buttons in hex format
- `children` (object): Object with tab names as keys, each containing array of child widgets

**Note:** Child widgets in tabs use coordinates relative to the tab content area.

---

### 13. Gauge Widget

**Description:** Circular gauge/meter with rotating needle indicator, perfect for displaying analog-style measurements like speed, pressure, temperature, or any ranged values.

**LVGL Component:** `lv_scale` with custom needle rendering

**Implementation Features:**
- Circular scale with configurable range (min/max values)
- Animated needle indicator
- Customizable angle range (240° arc by default)
- Major and minor tick marks with labels
- MQTT subscription for real-time value updates
- Smooth needle movement
- Perfect for dashboard displays

**JSON Example:**
```json
{
  "type": "gauge",
  "id": "speedometer",
  "x": 100,
  "y": 100,
  "w": 250,
  "h": 250,
  "properties": {
    "min_value": 0,
    "max_value": 200,
    "value": 75,
    "mqtt_topic": "vehicle/speed"
  }
}
```

**Properties:**
- `min_value` (integer, default: 0): Minimum value on the gauge scale
- `max_value` (integer, default: 100): Maximum value on the gauge scale
- `value` (integer, default: 0): Initial gauge value
- `mqtt_topic` (string, optional): Topic to subscribe for value updates

**MQTT Data Format:**
- **Subscribes to:** `mqtt_topic`
- **Expected payload:** Integer value (e.g., `"75"`)
- **Behavior:** Updates gauge needle position to reflect the new value
- **Value clamping:** Values outside min/max range are automatically clamped

**Usage Example:**
```bash
# Update speed gauge to 120
mosquitto_pub -h localhost -t "vehicle/speed" -m "120"

# Update temperature gauge to 25
mosquitto_pub -h localhost -t "sensor/temperature" -m "25"

# Update battery level to 85%
mosquitto_pub -h localhost -t "battery/level" -m "85"
```

---

### 14. Image Widget

**Description:** Displays images from SD card storage. Supports JPEG, PNG, and BMP formats. Images can be updated dynamically via MQTT.

**LVGL Component:** `lv_image`

**Implementation Features:**
- Load images from SD card (`/sdcard/...`)
- JPEG, PNG, BMP format support
- ESP32-P4 hardware JPEG decoder acceleration
- MQTT-based dynamic image updates
- Automatic image scaling to fit widget size

**JSON Example:**
```json
{
  "type": "image",
  "id": "product_photo",
  "x": 50,
  "y": 100,
  "w": 300,
  "h": 300,
  "properties": {
    "image_path": "/sdcard/images/product.jpg",
    "mqtt_topic": "display/product_image"
  }
}
```

**Properties:**
- `image_path` (string): Path to image file on SD card (e.g., "/sdcard/images/logo.jpg")
- `mqtt_topic` (string, optional): Topic to subscribe for image path updates

**MQTT Behavior:**
- **Subscribes to:** `mqtt_topic`
- **Expected payload:** Image file path (e.g., `"/sdcard/images/photo.png"`)
- **Behavior:** Loads and displays the image from the specified SD card path
- **Supported formats:** JPEG (.jpg, .jpeg), PNG (.png), BMP (.bmp)

**Usage Example:**
```bash
# Change displayed image
mosquitto_pub -h localhost -t "display/product_image" -m "/sdcard/images/product2.jpg"

# Show logo
mosquitto_pub -h localhost -t "display/logo" -m "/sdcard/images/company_logo.png"

# Display photo
mosquitto_pub -h localhost -t "gallery/current" -m "/sdcard/photos/vacation.jpg"
```

**SD Card Setup:**
- Images must be stored on SD card
- Recommended path structure: `/sdcard/images/`
- Ensure SD card is mounted before loading images
- Supported image sizes limited by available RAM

---

## Planned Widgets

The following widgets could be implemented based on LVGL components:

> **Note:** MQTT data formats for planned widgets are conceptual and subject to change during implementation.

### 14. Chart Widget

**Description:** Line or bar chart for visualizing time-series data.

**LVGL Component:** `lv_chart`

**Implementation Concept:**
- Multiple data series support
- Auto-scrolling time window
- MQTT updates for new data points
- Configurable Y-axis range
- Line or bar chart types

**MQTT Data Format:**
- **Subscribes to:** `mqtt_topic`
- **Expected payload:** Numeric value (e.g., `"25.3"`)
- **Behavior:** Each received value is appended to chart, oldest point removed when limit reached

**JSON Example:**
```json
{
  "type": "chart",
  "id": "temp_history",
  "x": 50,
  "y": 250,
  "w": 400,
  "h": 200,
  "properties": {
    "chart_type": "line",
    "points": 20,
    "y_min": 0,
    "y_max": 50,
    "mqtt_topic": "sensors/temperature",
    "color": "#FF5722"
  }
}
```

---

### 14. Textarea Widget

**Description:** Multi-line text input field.

**LVGL Component:** `lv_textarea`

**Implementation Concept:**
- Text input with on-screen keyboard
- MQTT publish on text change
- MQTT subscribe to set text
- Password mode support

**MQTT Data Format:**
- **Publishes to:** `mqtt_topic` (text string)
- **Subscribes to:** `mqtt_topic` (text string)
- **Example:** `"Hello World"` or `"User input text..."`
- **Trigger:** On text change or enter key

**JSON Example:**
```json
{
  "type": "textarea",
  "id": "message_input",
  "x": 50,
  "y": 100,
  "w": 300,
  "h": 100,
  "properties": {
    "placeholder": "Enter message...",
    "max_length": 200,
    "one_line": false,
    "mqtt_topic": "user/message",
    "color": "#FFFFFF"
  }
}
```

---

### 15. Roller Widget

**Description:** Scrollable roller for value selection (like iOS picker).

**LVGL Component:** `lv_roller`

**Implementation Concept:**
- Infinite scroll selection
- Numeric or string options
- MQTT bidirectional control
- Good for time/date selection

**MQTT Data Format:**
- **Publishes to:** `mqtt_topic` (selected option string or index)
- **Subscribes to:** `mqtt_topic`
- **Payload examples:** `"12"` (selected hour) or index `"12"`

**JSON Example:**
```json
{
  "type": "roller",
  "id": "hour_selector",
  "x": 100,
  "y": 150,
  "w": 80,
  "h": 120,
  "properties": {
    "options": ["00", "01", "02", "03", "04", "05", "06", "07", "08", "09", "10", "11", "12", "13", "14", "15", "16", "17", "18", "19", "20", "21", "22", "23"],
    "selected": 12,
    "mqtt_topic": "alarm/hour",
    "color": "#FF9800"
  }
}
```

---

### 16. Image Widget

**Description:** Display static or dynamic images from SD card or base64-encoded data.

**LVGL Component:** `lv_image` (formerly `lv_img`)

**Implementation Concept:**
- Load images from SD card filesystem
- Receive base64-encoded images via MQTT
- Support for PNG, JPEG, BMP, GIF
- Automatic format detection
- Memory-efficient handling
- ESP32-P4 hardware JPEG decoder support

**MQTT Data Format:**
- **Subscribes to:** `mqtt_topic`
- **Expected payload:** 
  - SD card file path: `"/sdcard/image.jpg"` (automatically converted to LVGL path `S:/image.jpg`)
  - Base64-encoded image data: Raw base64 string (e.g., `/9j/4AAQSkZJRgABAQAAAQABAAD...`)
- **Does not publish** (display only)

**Properties:**
- `image_path`: Initial image path on SD card (optional)
- `mqtt_topic`: MQTT topic to subscribe for dynamic image updates

**Image Loading Methods:**

1. **SD Card Loading:**
   - Store images on SD card (e.g., `/sdcard/logo.png`)
   - Reference in JSON config or send path via MQTT
   - Efficient for static images or pre-loaded content
   - No memory overhead - images loaded directly from filesystem

2. **Base64 Loading:**
   - Encode image: `python3 encode_image_to_base64.py image.jpg`
   - Send via MQTT: `mosquitto_pub -t "demo/image" -m "<base64_data>"`
   - Ideal for dynamic content, small images, or remote updates
   - Image data decoded to memory (limited by available RAM)

**SD Card Setup:**
- SD card mounted at `/sdcard` (configured in main.cpp)
- LVGL filesystem maps `/sdcard` → `S:` drive
- Supported formats: JPEG (hardware accelerated), PNG, BMP, GIF

**JSON Example:**
```json
{
  "type": "image",
  "id": "camera_feed",
  "x": 200,
  "y": 100,
  "w": 320,
  "h": 240,
  "properties": {
    "image_path": "/sdcard/logo.png",
    "mqtt_topic": "camera/snapshot"
  }
}
```

**Usage Examples:**

```bash
# Load image from SD card via MQTT
mosquitto_pub -t "camera/snapshot" -m "/sdcard/photo.jpg"

# Load base64-encoded image via MQTT
# First, encode your image
python3 examples/encode_image_to_base64.py my_image.jpg

# Then send the base64 output
mosquitto_pub -t "camera/snapshot" -m "<paste_base64_here>"
```

**Notes:**
- Small images (<100KB) work best for base64 transmission over MQTT
- Large images should be stored on SD card for performance
- Base64 encoding increases data size by ~33%
- ESP32-P4 hardware JPEG decoder provides fast rendering
- LVGL automatically detects image format from data


---

### 17. Meter Widget

**Description:** Analog-style meter/gauge with scale and needle.

**LVGL Component:** `lv_meter`

**Implementation Concept:**
- Speedometer-style gauge
- Multiple scales and indicators
- MQTT updates for value changes
- Tick marks and labels

**MQTT Data Format:**
- **Subscribes to:** `mqtt_topic`
- **Expected payload:** Numeric value (e.g., `"65"` for 65 km/h)
- **Does not publish** (read-only display)

**JSON Example:**
```json
{
  "type": "meter",
  "id": "speed_gauge",
  "x": 100,
  "y": 100,
  "w": 200,
  "h": 200,
  "properties": {
    "min": 0,
    "max": 200,
    "value": 65,
    "units": "km/h",
    "mqtt_topic": "vehicle/speed",
    "color": "#F44336"
  }
}
```

---

### 18. Keyboard Widget

**Description:** On-screen keyboard for text input.

**LVGL Component:** `lv_keyboard`

**Implementation Concept:**
- Full QWERTY keyboard
- Attach to textarea widgets
- Multiple modes (text, number, special)
- Auto-show/hide based on focus

**JSON Example:**
```json
{
  "type": "keyboard",
  "id": "main_keyboard",
  "x": 0,
  "y": 400,
  "w": 1024,
  "h": 200,
  "properties": {
    "mode": "text",
    "target_id": "message_input"
  }
}
```

---

### 18. Calendar Widget

**Description:** Calendar for date selection.

**LVGL Component:** `lv_calendar`

**Implementation Concept:**
- Month/year navigation
- Date selection
- MQTT publish selected date
- Highlighted dates support

**MQTT Data Format:**
- **Publishes to:** `mqtt_topic`
- **Payload sent:** ISO date format `"YYYY-MM-DD"` (e.g., `"2026-01-29"`)
- **Subscribes to:** `mqtt_topic` (to set selected date)
- **Expected payload:** `"2026-01-29"` or Unix timestamp

**JSON Example:**
```json
{
  "type": "calendar",
  "id": "date_picker",
  "x": 100,
  "y": 50,
  "w": 300,
  "h": 300,
  "properties": {
    "year": 2026,
    "month": 1,
    "day": 29,
    "mqtt_topic": "scheduler/date",
    "color": "#3F51B5"
  }
}
```

---

### 19. Colorwheel Widget

**Description:** Color picker wheel.

**LVGL Component:** `lv_colorwheel`

**Implementation Concept:**
- HSV color selection
- MQTT publish RGB values
- Perfect for RGB LED control

**MQTT Data Format:**
- **Publishes to:** `mqtt_topic`
- **Payload formats:** 
  - RGB: `"255,128,0"` (comma-separated R,G,B values)
  - Hex: `"#FF8000"`
  - JSON: `{"r":255,"g":128,"b":0}`
- **Subscribes to:** `mqtt_topic` (accepts same formats)
- **Configurable via:** `format` property (`"rgb"`, `"hex"`, or `"json"`)

**JSON Example:**
```json
{
  "type": "colorwheel",
  "id": "rgb_picker",
  "x": 150,
  "y": 100,
  "w": 200,
  "h": 200,
  "properties": {
    "mqtt_topic": "lights/rgb",
    "format": "rgb",
    "initial_color": "#FF0000"
  }
}
```

---

## Widget Properties Reference

### Common Properties (All Widgets)

| Property | Type | Required | Description |
|----------|------|----------|-------------|
| `type` | string | Yes | Widget type identifier |
| `id` | string | Yes | Unique widget identifier |
| `x` | number | Yes | X coordinate (pixels) |
| `y` | number | Yes | Y coordinate (pixels) |
| `w` | number | Yes | Width (pixels) |
| `h` | number | Yes | Height (pixels) |
| `properties` | object | No | Widget-specific properties |

### MQTT Properties

| Property | Type | Description |
|----------|------|-------------|
| `mqtt_topic` | string | Topic for publish/subscribe |
| `mqtt_retained` | boolean | Publish as retained message (default: true) |

#### MQTT Data Formats by Widget Type

**Label Widget:**
- **Subscribes to:** `mqtt_topic`
- **Expected payload:** String value (e.g., `"25.5"`, `"Hello World"`, `"120"`)
- **Format support:** If `format` property is provided (e.g., `"Temp: %s°C"`), payload is substituted into the %s placeholder
- **Example:** Format `"Speed: %s km/h"` with payload `"120"` displays "Speed: 120 km/h"
- **Does not publish**

**Button Widget:**
- **Publishes to:** `mqtt_topic`
- **Payload sent:** Value of `mqtt_payload` property (string)
- **Example:** `"TOGGLE"`, `"ON"`, `"PRESSED"`
- **Trigger:** On click/press event
- **Does not subscribe**

**Switch Widget:**
- **Publishes to:** `mqtt_topic`
- **Payload sent:** `"ON"` or `"OFF"` (string)
- **Subscribes to:** `mqtt_topic` (auto-subscribes for bidirectional control)
- **Expected payload:** `"ON"`, `"OFF"`, `"1"`, `"0"`, `"true"`, `"false"` (case-insensitive)
- **Trigger:** On state change (user interaction or MQTT message)
- **Feedback prevention:** Won't republish when receiving own messages

**Slider Widget:**
- **Publishes to:** `mqtt_topic`
- **Payload sent:** Numeric value as string (e.g., `"127"`, `"255"`)
- **Subscribes to:** `mqtt_topic` (auto-subscribes for bidirectional control)
- **Expected payload:** Integer value within min/max range (e.g., `"150"`)
- **Trigger:** On value change (user interaction or MQTT message)
- **Feedback prevention:** Won't republish when receiving own messages

**Arc Widget:**
- **Publishes to:** `mqtt_topic`
- **Payload sent:** Numeric value as string (e.g., `"75"`)
- **Subscribes to:** `mqtt_topic` (auto-subscribes for bidirectional control)
- **Expected payload:** Integer value within min/max range (e.g., `"50"`)
- **Trigger:** On value change (user interaction or MQTT message)
- **Feedback prevention:** Won't republish when receiving own messages

**Dropdown Widget:**
- **Publishes to:** `mqtt_topic`
- **Payload sent:** Selected option string (e.g., `"Heat"`, `"Cool"`)
- **Subscribes to:** `mqtt_topic` (auto-subscribes for bidirectional control)
- **Expected payload:** Option string matching one in options list (e.g., `"Auto"`) or numeric index (e.g., `"0"`)
- **Trigger:** On selection change (user interaction or MQTT message)
- **Feedback prevention:** Won't republish when receiving own messages

**Checkbox Widget:**
- **Publishes to:** `mqtt_topic`
- **Payload sent:** `"true"` or `"false"` (string)
- **Subscribes to:** `mqtt_topic` (auto-subscribes for bidirectional control)
- **Expected payload:** `"true"`, `"false"`, `"1"`, `"0"`, `"checked"`, `"unchecked"` (case-insensitive)
- **Trigger:** On state change (user interaction or MQTT message)
- **Feedback prevention:** Won't republish when receiving own messages

**Bar Widget:**
- **Subscribes to:** `mqtt_topic`
- **Expected payload:** Integer value within min/max range (e.g., `"75"`)
- **Does not publish** (read-only indicator)

**LED Widget:**
- **Subscribes to:** `mqtt_topic`
- **Expected payload:** `"ON"`, `"OFF"`, or brightness value `"0-255"` (e.g., `"128"` for 50% brightness)
- **Does not publish** (status indicator only)

**Spinner Widget:**
- **Subscribes to:** `mqtt_topic`
- **Expected payload:** `"show"`, `"hide"`, `"1"`, `"0"`, `"true"`, `"false"` (case-insensitive)
- **Does not publish** (UI indicator only)

**Tabview Widget:**
- **Publishes to:** `mqtt_topic`
- **Payload sent:** Active tab name as string (e.g., `"Controls"`, `"Status"`)
- **Subscribes to:** `mqtt_topic` (auto-subscribes for bidirectional control)
- **Expected payload:** Tab name (e.g., `"Settings"`) or numeric index (e.g., `"2"`)
- **Trigger:** On tab change (user swipe/tap or MQTT message)
- **Feedback prevention:** Won't republish when receiving own messages

**Container Widget:**
- **Does not use MQTT** (layout only)

### Visual Properties

| Property | Type | Description |
|----------|------|-------------|
| `color` | string | Color in hex format (e.g., "#FF0000") |
| `bg_color` | string | Background color |
| `text_color` | string | Text color |
| `border_width` | number | Border width in pixels |
| `padding` | number | Internal padding in pixels |

### Widget Hierarchy

Widgets can be nested using the `children` array property in container widgets. Child widget coordinates are relative to their parent container.

**Example Nested Structure:**
```json
{
  "type": "container",
  "id": "main_panel",
  "x": 0,
  "y": 0,
  "w": 1024,
  "h": 600,
  "properties": {
    "bg_color": "#000000"
  },
  "children": [
    {
      "type": "container",
      "id": "left_panel",
      "x": 10,
      "y": 10,
      "w": 400,
      "h": 580,
      "properties": {
        "bg_color": "#1E1E1E"
      },
      "children": [
        {
          "type": "label",
          "id": "title",
          "x": 10,
          "y": 10,
          "w": 380,
          "h": 40,
          "properties": {
            "text": "Controls",
            "color": "#FFFFFF"
          }
        }
      ]
    }
  ]
}
```

---

## Implementation Notes

### Thread Safety

All widget updates from MQTT callbacks use `lv_async_call()` to ensure thread-safe LVGL operations. Direct LVGL API calls from non-LVGL tasks will cause crashes.

### Feedback Loop Prevention

Widgets that support bidirectional MQTT communication (Switch, Slider) implement feedback loop prevention using the `m_updating_from_mqtt` flag. This prevents widgets from republishing messages they just received.

### Retained Messages

Widgets default to publishing retained MQTT messages (`retained: true`) to ensure state persistence across MQTT broker restarts. This can be disabled per widget.

### MQTT Message Examples

**Switch Widget - User toggles switch ON:**
```
Topic: home/fan/state
Payload: ON
Retained: true (if configured)
```

**Switch Widget - External MQTT message to turn OFF:**
```bash
mosquitto_pub -h broker.local -t home/fan/state -m "OFF"
# Widget will update to OFF state without republishing
```

**Slider Widget - User moves slider to 200:**
```
Topic: home/rgb/red
Payload: 200
Retained: true (if configured)
```

**Slider Widget - External MQTT message to set value:**
```bash
mosquitto_pub -h broker.local -t home/rgb/red -m "128"
# Slider will move to position 128 without republishing
```

**Label Widget - Update temperature display:**
```bash
mosquitto_pub -h broker.local -t sensors/temperature -m "23.5"
# If format is "Temp: %.1f°C", displays: "Temp: 23.5°C"
```

**Button Widget - User clicks button:**
```
Topic: home/light/command
Payload: TOGGLE
Retained: false (buttons typically don't use retained)
```

### Color Format

Colors are specified in hex format with a `#` prefix:
- `"#RRGGBB"` format (e.g., `"#FF0000"` for red)
- 6 hex digits for RGB values (0-255 each)

### Coordinate System

- **Absolute coordinates:** Used for top-level widgets (relative to screen)
- **Relative coordinates:** Used for child widgets (relative to parent container)
- Origin (0,0) is top-left corner

### Widget Lifecycle

1. **Creation:** JSON parsed → `create()` method called → LVGL objects created
2. **Update:** MQTT message received → `lv_async_call()` → Widget updated
3. **Destruction:** Config change → `destroy()` method → LVGL objects deleted

---

## Complete JSON Configuration Example

```json
{
  "version": "1.0",
  "widgets": [
    {
      "type": "container",
      "id": "main_container",
      "x": 0,
      "y": 0,
      "w": 1024,
      "h": 600,
      "properties": {
        "bg_color": "#0A0A0A",
        "padding": 0
      },
      "children": [
        {
          "type": "container",
          "id": "left_panel",
          "x": 10,
          "y": 10,
          "w": 500,
          "h": 580,
          "properties": {
            "bg_color": "#1E1E1E",
            "border_width": 2,
            "padding": 15
          },
          "children": [
            {
              "type": "label",
              "id": "title",
              "x": 10,
              "y": 10,
              "w": 470,
              "h": 40,
              "properties": {
                "text": "RGB Control Panel",
                "color": "#FFFFFF"
              }
            },
            {
              "type": "switch",
              "id": "power",
              "x": 10,
              "y": 60,
              "w": 60,
              "h": 30,
              "properties": {
                "state": false,
                "mqtt_topic": "home/rgb/power",
                "mqtt_retained": true,
                "color": "#4CAF50"
              }
            },
            {
              "type": "slider",
              "id": "red",
              "x": 10,
              "y": 120,
              "w": 460,
              "h": 30,
              "properties": {
                "label": "Red",
                "min": 0,
                "max": 255,
                "value": 255,
                "mqtt_topic": "home/rgb/red",
                "mqtt_retained": true,
                "color": "#FF0000"
              }
            },
            {
              "type": "slider",
              "id": "green",
              "x": 10,
              "y": 200,
              "w": 460,
              "h": 30,
              "properties": {
                "label": "Green",
                "min": 0,
                "max": 255,
                "value": 0,
                "mqtt_topic": "home/rgb/green",
                "mqtt_retained": true,
                "color": "#00FF00"
              }
            },
            {
              "type": "slider",
              "id": "blue",
              "x": 10,
              "y": 280,
              "w": 460,
              "h": 30,
              "properties": {
                "label": "Blue",
                "min": 0,
                "max": 255,
                "value": 0,
                "mqtt_topic": "home/rgb/blue",
                "mqtt_retained": true,
                "color": "#0000FF"
              }
            },
            {
              "type": "slider",
              "id": "brightness",
              "x": 10,
              "y": 360,
              "w": 460,
              "h": 30,
              "properties": {
                "label": "Brightness",
                "min": 0,
                "max": 100,
                "value": 50,
                "mqtt_topic": "home/rgb/brightness",
                "mqtt_retained": false,
                "color": "#FFFFFF"
              }
            },
            {
              "type": "button",
              "id": "save_preset",
              "x": 150,
              "y": 450,
              "w": 180,
              "h": 60,
              "properties": {
                "text": "Save Preset",
                "mqtt_topic": "home/rgb/preset",
                "mqtt_payload": "SAVE",
                "color": "#2196F3"
              }
            }
          ]
        },
        {
          "type": "container",
          "id": "right_panel",
          "x": 520,
          "y": 10,
          "w": 494,
          "h": 580,
          "properties": {
            "bg_color": "#1E1E1E",
            "border_width": 2,
            "padding": 15
          },
          "children": [
            {
              "type": "label",
              "id": "status_title",
              "x": 10,
              "y": 10,
              "w": 464,
              "h": 40,
              "properties": {
                "text": "System Status",
                "color": "#FFFFFF"
              }
            },
            {
              "type": "label",
              "id": "temperature",
              "x": 10,
              "y": 70,
              "w": 464,
              "h": 30,
              "properties": {
                "text": "Temperature: --",
                "format": "Temperature: %.1f°C",
                "mqtt_topic": "sensors/temperature",
                "color": "#FFB300"
              }
            },
            {
              "type": "label",
              "id": "humidity",
              "x": 10,
              "y": 110,
              "w": 464,
              "h": 30,
              "properties": {
                "text": "Humidity: --",
                "format": "Humidity: %.0f%%",
                "mqtt_topic": "sensors/humidity",
                "color": "#29B6F6"
              }
            }
          ]
        }
      ]
    }
  ]
}
```

---

## Future Enhancements

- **Animation Support:** Fade in/out, slide transitions
- **Gesture Recognition:** Swipe, pinch, rotate
- **Themes:** Pre-defined color schemes and styles
- **Widget Templates:** Reusable widget configurations
- **Conditional Visibility:** Show/hide widgets based on MQTT state
- **Data Binding:** Automatic widget updates from MQTT topics
- **Widget Groups:** Focus management and keyboard navigation

---

**Last Updated:** January 29, 2026  
**Version:** 1.0
