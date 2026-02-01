# JSON Configuration Examples

This directory contains example JSON configurations demonstrating various widgets and features of the ESP32-P4 MQTT Panel system.

## Quick Start Examples

### Basic Widget Examples

These examples demonstrate individual widget types in isolation, perfect for learning how each widget works.

#### [label_example.json](label_example.json)
Simple label widgets with different fonts, colors, and alignments. Shows text display and MQTT-driven text updates.

**Features:**
- Static text labels
- MQTT-updated labels with format strings
- Various font sizes (16, 20, 24)
- Text alignment options (left, center, right)

#### [button_example.json](button_example.json)
Interactive buttons that publish MQTT messages on click.

**Features:**
- Color-coded buttons (green, red, orange, blue)
- MQTT publishing on button press
- Command buttons (START, STOP, RESET, PAUSE)

#### [switch_example.json](switch_example.json)
Toggle switches with bidirectional MQTT control.

**Features:**
- On/Off toggle switches
- MQTT publish on state change
- MQTT subscribe for remote control
- LED indicators synced with switch state

#### [slider_example.json](slider_example.json)
Sliders and progress bars for value adjustment.

**Features:**
- Horizontal sliders
- Value labels showing current position
- Bar widgets for visual feedback
- Arc widgets (circular sliders)
- MQTT bidirectional control

#### [gauge_example.json](gauge_example.json)
Analog-style circular gauges.

**Features:**
- Speed gauge (0-200 km/h)
- Temperature gauge (-20 to 120°C)
- Interactive sliders to control gauge values
- Real-time MQTT updates

#### [image_example.json](image_example.json)
Image widgets demonstrating SD card and base64 image loading.

**Features:**
- SD card image loading
- Base64-encoded image loading via MQTT
- Dynamic image switching
- Control buttons for image selection

## Complex Demonstrations

### [tabview_demo.json](tabview_demo.json)
Multi-tab interface with widgets organized across different tabs.

**Features:**
- Tab-based navigation
- Widgets specific to each tab
- Tab switching via MQTT
- Per-tab child widget management

### [gauge_demo.json](gauge_demo.json)
Multiple gauges with interactive controls in a single view.

**Features:**
- Multiple gauge widgets
- Synchronized controls
- MQTT-driven value updates
- Real-time monitoring display

### [complete_demo.json](complete_demo.json)
Comprehensive demonstration of all widget types in action.

**Features:**
- All 14 widget types
- Labels, buttons, switches, sliders
- Gauges, arcs, bars, LEDs
- Checkboxes, dropdowns, spinners
- Containers and tabviews

### [interactive_complete_demo.json](interactive_complete_demo.json) ⭐
**The most comprehensive example** - Full-featured interactive demonstration with tabs organizing different widget categories.

**Features:**
- **Gauges Tab:** Speed and temperature gauges with interactive sliders
- **Sliders & Bars Tab:** Volume and brightness controls with bars and arcs
- **Switches Tab:** Power, WiFi, Bluetooth switches with LED indicators and checkboxes
- **Controls Tab:** Command buttons, dropdowns, and spinner
- **Images Tab:** Base64-encoded image display with MQTT control
- Professional dark theme styling
- Over 750 lines of well-organized configuration

**Best for:** Production-ready UI reference, complete feature showcase

## Usage

### Loading a Configuration

Send any JSON file to your ESP32-P4 via MQTT:

```bash
# Using mosquitto_pub
mosquitto_pub -h <broker_ip> -t "hmi/config" -f complete_demo.json

# Or with the provided script
./load_config.sh <broker_ip> complete_demo.json
```

### Testing Individual Widgets

Start with the basic examples to understand each widget type:

1. **Labels:** `label_example.json` - Learn text display
2. **Buttons:** `button_example.json` - Learn interaction
3. **Switches:** `switch_example.json` - Learn state management
4. **Sliders:** `slider_example.json` - Learn value controls
5. **Gauges:** `gauge_example.json` - Learn analog displays
6. **Images:** `image_example.json` - Learn image loading

### Building Complex UIs

Progress to the advanced examples:

1. **Tabview:** `tabview_demo.json` - Learn multi-screen layouts
2. **Complete:** `complete_demo.json` - See all widgets together
3. **Interactive:** `interactive_complete_demo.json` - Study production UI

## MQTT Topics

Each example uses specific MQTT topics for control. Common patterns:

- `demo/speed` - Gauge values
- `demo/temperature` - Temperature values
- `demo/power` - Switch states
- `demo/volume` - Slider positions
- `demo/command` - Button commands
- `demo/mode` - Dropdown selections
- `demo/image1` - Image data (SD path or base64)
- `hmi/config` - Configuration loading

## Widget Reference

For detailed documentation on each widget type, see:
- [WIDGETS.md](../../docs/WIDGETS.md) - Complete widget documentation
- [IMAGE_WIDGET_README.md](../docs/IMAGE_WIDGET_README.md) - Image widget guide
- [BASE64_IMAGE_TESTING.md](../docs/BASE64_IMAGE_TESTING.md) - Image testing guide

## JSON Structure

All configuration files follow this structure:

```json
{
  "version": 1,
  "widgets": [
    {
      "type": "widget_type",
      "id": "unique_id",
      "x": 0,
      "y": 0,
      "w": 100,
      "h": 50,
      "properties": {
        "property1": "value1",
        "mqtt_topic": "topic/name"
      }
    }
  ]
}
```

## Tips

1. **Start Simple:** Begin with single-widget examples before complex UIs
2. **Use Tabs:** For complex UIs, organize widgets into tabs (see `interactive_complete_demo.json`)
3. **MQTT Topics:** Use descriptive topic names for easier debugging
4. **Widget IDs:** Keep IDs unique and descriptive
5. **Positioning:** ESP32-P4 Function EV Board display is 1024x600 pixels
6. **Colors:** Use hex colors (e.g., `#FF0000` for red) for consistent styling
7. **Base64 Images:** Keep under 100KB for best performance (see image widget docs)

## Creating Your Own

1. Copy an existing example as a template
2. Modify widget positions and properties
3. Test with `mosquitto_pub -h <broker> -t hmi/config -f your_config.json`
4. Iterate based on display results

## File Sizes

- Simple examples: ~1-3 KB
- Complex demos: ~15-25 KB
- Interactive demo: ~50 KB (with base64 images: 50-150 KB)

Note: MQTT buffers are configured for up to 512 KB messages.

## Common Patterns

### Dashboard Layout
See `interactive_complete_demo.json` for a professional dashboard with:
- Themed sections
- Organized tabs
- Consistent spacing
- Color-coded widgets

### Monitoring System
See `gauge_demo.json` for real-time monitoring:
- Multiple gauges
- Status labels
- Alert indicators

### Control Panel
See `complete_demo.json` for device control:
- Switches and buttons
- Value adjustments
- Status feedback

## Troubleshooting

**Configuration not loading?**
- Check JSON syntax with a validator
- Verify MQTT broker is running
- Check topic name matches `hmi/config`
- Increase buffer sizes if file > 512 KB

**Widgets not displaying?**
- Verify widget positions are within 1024x600
- Check widget IDs are unique
- Review serial monitor for error messages

**MQTT not working?**
- Test broker connectivity
- Verify topic names
- Check QoS settings (0 or 1 recommended)

**Images not showing?**
- Check SD card is mounted
- Verify file paths start with `/sdcard/`
- For base64: ensure data is complete
- See [IMAGE_WIDGET_README.md](../docs/IMAGE_WIDGET_README.md)

## Support

For more information:
- Main documentation: [../../README.md](../../README.md)
- Widget specifications: [../../docs/WIDGETS.md](../../docs/WIDGETS.md)
- Initial spec: [../../docs/INITIAL_SPEC.md](../../docs/INITIAL_SPEC.md)
