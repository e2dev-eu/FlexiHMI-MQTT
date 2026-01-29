# HMI Widget System Documentation

This document describes the HMI widget system for the ESP32-P4 MQTT Panel, including currently implemented widgets and potential future widgets based on LVGL components.

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
- Printf-style formatting for numeric values
- MQTT subscription for dynamic updates
- Custom text color support

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
    "format": "Temp: %.1f°C",
    "mqtt_topic": "sensors/temperature",
    "color": "#FFFFFF"
  }
}
```

**Properties:**
- `text` (string): Initial text to display
- `format` (string, optional): Printf-style format for MQTT payload
- `mqtt_topic` (string, optional): Topic to subscribe for updates
- `color` (string, optional): Text color in hex format (e.g., "#FFFFFF")

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
    "publish_topic": "home/light/command",
    "publish_payload": "TOGGLE",
    "color": "#2196F3"
  }
}
```

**Properties:**
- `text` (string): Button label text
- `publish_topic` (string): MQTT topic to publish to on click
- `publish_payload` (string): Payload to send when clicked
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
    "retained": true,
    "color": "#4CAF50"
  }
}
```

**Properties:**
- `state` (boolean): Initial state (true = ON, false = OFF)
- `mqtt_topic` (string): Topic for publish/subscribe
- `retained` (boolean, optional): Publish as retained message (default: true)
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
    "retained": false,
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
- `retained` (boolean, optional): Publish as retained message (default: true)
- `color` (string, optional): Slider indicator/knob color in hex format

---

## Planned Widgets

The following widgets could be implemented based on LVGL components:

> **Note:** MQTT data formats for planned widgets are conceptual and subject to change during implementation.

### 6. Chart Widget

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

### 7. Arc Widget

**Description:** Circular arc control for gauges or rotary inputs.

**LVGL Component:** `lv_arc`

**Implementation Concept:**
- Circular slider/gauge
- Angle range configuration
- MQTT bidirectional control
- Perfect for thermostats or volume controls

**MQTT Data Format:**
- **Publishes to:** `mqtt_topic` (numeric value as string)
- **Subscribes to:** `mqtt_topic` (numeric value within min/max range)
- **Example:** `"75"` (for volume level 0-100)

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
    "start_angle": 135,
    "end_angle": 45,
    "mqtt_topic": "audio/volume",
    "color": "#9C27B0"
  }
}
```

---

### 8. Bar Widget

**Description:** Progress bar or level indicator.

**LVGL Component:** `lv_bar`

**Implementation Concept:**
- Horizontal or vertical bar
- Read-only progress indicator
- MQTT subscription for value updates
- Good for battery levels, download progress

**MQTT Data Format:**
- **Subscribes to:** `mqtt_topic`
- **Expected payload:** Numeric value (e.g., `"75"` for 75%)
- **Does not publish** (read-only indicator)

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
    "orientation": "horizontal",
    "color": "#4CAF50"
  }
}
```

---

### 9. Dropdown Widget

**Description:** Dropdown selection list.

**LVGL Component:** `lv_dropdown`

**Implementation Concept:**
- Multiple choice selection
- MQTT publish on selection change
- MQTT subscribe to set selection
- String-based options list

**MQTT Data Format:**
- **Publishes to:** `mqtt_topic` (selected option string)
- **Subscribes to:** `mqtt_topic` (option string or index)
- **Payload examples:** `"Heat"` or `"1"` (index-based)
- **Published example:** `"Cool"` (when user selects Cool mode)

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
    "color": "#03A9F4"
  }
}
```

---

### 10. Checkbox Widget

**Description:** Checkbox for boolean selection.

**LVGL Component:** `lv_checkbox`

**Implementation Concept:**
- Checkbox with text label
- MQTT bidirectional control
- Similar to switch but with different UI

**MQTT Data Format:**
- **Publishes to:** `mqtt_topic` (`"true"` or `"false"`)
- **Subscribes to:** `mqtt_topic`
- **Expected payload:** `"true"`, `"false"`, `"1"`, `"0"`, `"checked"`, `"unchecked"`

**JSON Example:**
```json
{
  "type": "checkbox",
  "id": "notifications",
  "x": 50,
  "y": 200,
  "w": 200,
  "h": 30,
  "properties": {
    "text": "Enable Notifications",
    "checked": true,
    "mqtt_topic": "settings/notifications",
    "color": "#2196F3"
  }
}
```

---

### 11. Textarea Widget

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

### 12. Roller Widget

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

### 13. Image Widget

**Description:** Display static or dynamic images.

**LVGL Component:** `lv_image` (formerly `lv_img`)

**Implementation Concept:**
- Display images from flash or MQTT
- Support for PNG, JPG, BMP
- MQTT topic for image URL or base64
- Zoom and rotation support

**MQTT Data Format:**
- **Subscribes to:** `mqtt_topic`
- **Expected payload:** File path (e.g., `"/spiffs/image.png"`) or base64-encoded image data
- **Example:** `"data:image/png;base64,iVBORw0KGgo..."` or `"http://server/image.jpg"`
- **Does not publish**

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
    "src": "/spiffs/logo.png",
    "mqtt_topic": "camera/snapshot",
    "zoom": 256,
    "angle": 0
  }
}
```

---

### 14. Spinner Widget

**Description:** Loading/activity spinner.

**LVGL Component:** `lv_spinner`

**Implementation Concept:**
- Animated spinner for loading states
- MQTT control for show/hide
- Configurable speed and arc length

**MQTT Data Format:**
- **Subscribes to:** `mqtt_topic`
- **Expected payload:** `"show"`, `"hide"`, `"1"`, `"0"`, `"true"`, `"false"`
- **Does not publish**

**JSON Example:**
```json
{
  "type": "spinner",
  "id": "loading_indicator",
  "x": 200,
  "y": 200,
  "w": 50,
  "h": 50,
  "properties": {
    "speed": 1000,
    "arc_length": 60,
    "mqtt_topic": "ui/loading",
    "color": "#2196F3"
  }
}
```

---

### 15. LED Widget

**Description:** Simple LED indicator (on/off/color).

**LVGL Component:** `lv_led`

**Implementation Concept:**
- Simple circular LED indicator
- MQTT control for on/off/brightness
- Color changes based on state

**MQTT Data Format:**
- **Subscribes to:** `mqtt_topic`
- **Expected payload:** `"ON"`, `"OFF"`, or brightness `"0-255"`
- **Example:** `"255"` (full brightness), `"0"` (off), `"128"` (50% brightness)
- **Does not publish**

**JSON Example:**
```json
{
  "type": "led",
  "id": "status_led",
  "x": 50,
  "y": 50,
  "w": 30,
  "h": 30,
  "properties": {
    "brightness": 255,
    "mqtt_topic": "device/status",
    "color_on": "#00FF00",
    "color_off": "#808080"
  }
}
```

---

### 16. Meter Widget

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

### 17. Keyboard Widget

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

### 20. Tabview Widget

**Description:** Tabbed interface for organizing multiple views.

**LVGL Component:** `lv_tabview`

**Implementation Concept:**
- Multiple tabs with content
- Swipe between tabs
- Each tab can contain child widgets
- MQTT control for active tab

**MQTT Data Format:**
- **Publishes to:** `mqtt_topic` (active tab index or name)
- **Subscribes to:** `mqtt_topic` (tab index or name to switch to)
- **Payload examples:** `"0"`, `"1"`, `"2"` (index) or `"Controls"`, `"Status"` (tab name)
- **Trigger:** On tab change (user swipe/tap or MQTT message)

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
    "mqtt_topic": "ui/active_tab"
  },
  "children": {
    "Controls": [],
    "Status": [],
    "Settings": []
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
| `retained` | boolean | Publish as retained message (default: true) |

#### MQTT Data Formats by Widget Type

**Label Widget:**
- **Subscribes to:** `mqtt_topic`
- **Expected payload:** String or numeric value
- **Example:** `"25.5"` or `"Hello World"`
- **Format support:** If `format` property is provided (e.g., `"Temp: %.1f°C"`), payload is parsed as float and formatted
- **Does not publish**

**Button Widget:**
- **Publishes to:** `publish_topic`
- **Payload sent:** Value of `publish_payload` property (string)
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
                "retained": true,
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
                "retained": true,
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
                "retained": true,
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
                "retained": true,
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
                "retained": false,
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
                "publish_topic": "home/rgb/preset",
                "publish_payload": "SAVE",
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
