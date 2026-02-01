# Widget Examples

This folder contains example JSON configurations demonstrating all available widgets.

## Available Examples

### Simple Widget Examples

1. **label_example.json** - Demonstrates static and dynamic labels with MQTT updates
2. **button_example.json** - Shows multiple buttons with different colors
3. **slider_example.json** - Slider with real-time value display and bar visualization
4. **gauge_example.json** - Interactive gauge with slider and button controls
5. **switch_example.json** - Switches controlling LED indicators
6. **tabview_demo.json** - Multi-tab interface (existing)
7. **complete_demo.json** - All widgets in one demo (existing)
8. **gauge_demo.json** - Multiple gauges demonstration (existing)

### Comprehensive Example

**interactive_complete_demo.json** - A complete interactive demonstration with:
- **Gauges Tab**: Two gauges (speed and temperature) with slider controls
- **Sliders & Bars Tab**: Volume and brightness controls with bars and arcs
- **Switches Tab**: Power, WiFi, and Bluetooth switches with LED indicators and checkboxes
- **Controls Tab**: Action buttons, dropdowns, and loading spinner

## How to Use

1. Connect to your ESP32-P4 device via MQTT
2. Publish the JSON configuration to the topic: `panel/config`

Example using mosquitto:
```bash
mosquitto_pub -h <mqtt_broker> -t "panel/config" -f examples/interactive_complete_demo.json
```

## Interactive Controls

All examples include interactive elements:

### Buttons
- Send MQTT messages when clicked
- Can be styled with custom colors
- Display feedback through labels

### Sliders
- Publish values as they change
- Range: customizable min/max
- Connected to bars, arcs, and gauges

### Switches
- Toggle on/off states
- Publish "1" or "0" to MQTT topics
- Control LED indicators

### Gauges
- Display analog-style values
- Update via MQTT or sliders
- Customizable ranges

## MQTT Topics Used

The examples use these demo topics:
- `demo/speed` - Speed gauge value (0-200)
- `demo/temperature` - Temperature value (-20 to 120)
- `demo/volume` - Volume level (0-100)
- `demo/brightness` - Brightness level (0-100)
- `demo/power` - Power switch state
- `demo/wifi` - WiFi switch state
- `demo/bluetooth` - Bluetooth switch state
- `demo/command` - Button commands
- `demo/mode` - Dropdown selection
- `demo/gauge` - Gauge example value

## Testing Widgets

You can manually control widgets via MQTT:

```bash
# Update gauge
mosquitto_pub -h localhost -t "demo/speed" -m "120"

# Toggle switch
mosquitto_pub -h localhost -t "demo/power" -m "1"

# Update slider/bar
mosquitto_pub -h localhost -t "demo/volume" -m "75"
```

## Widget Properties

Each widget type supports different properties. See `WIDGETS.md` for complete documentation.

## Version

All examples use version 1 of the configuration format.
