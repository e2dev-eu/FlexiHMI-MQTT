# JSON Configuration Examples

This directory contains example JSON configurations demonstrating various widgets and features of the FlexiHMI MQTT system.

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

#### [gauge_example_1.json](gauge_example_1.json)
Analog-style circular gauges with standardized range properties.

**Features:**
- Speed gauge (0-200 range with `min`/`max` properties)
- Temperature gauge (-20 to 120 range)
- Interactive sliders to control gauge values
- Real-time MQTT updates
- Demonstrates consistent property naming across all range-based widgets

#### [image_example.json](image_example.json)
Image widgets demonstrating SD card and base64 QOI image loading.

**Features:**
- SD card image loading
- Base64-encoded image loading via MQTT
- Dynamic image switching
- Control buttons for image selection

#### [line_chart_example.json](line_chart_example.json)
Real-time line chart driven by MQTT samples.

**Features:**
- `line_chart` widget with configurable range and history length
- Slider and buttons publishing numeric samples to chart topic
- Live label showing latest sample value
- Ready-to-test topic: `demo/line_chart`

## Complex Demonstrations

### [tabview_demo.json](tabview_demo.json)
Multi-tab interface with widgets organized across different tabs.

**Features:**
- Tab-based navigation
- Widgets specific to each tab
- Tab switching via MQTT
- Per-tab child widget management

### [gauge_example_0.json](gauge_example_0.json)
Alternative gauge-focused demonstration.

**Features:**
- Multiple gauge widgets
- Synchronized controls
- MQTT-driven value updates
- Real-time monitoring display

### [single_screen_example.json](single_screen_example.json)
Single-screen demonstration with multiple widget types.

**Features:**
- Mixed widgets on one screen
- Labels, buttons, switches, sliders
- Gauges, arcs, bars, LEDs
- Checkboxes, dropdowns, spinners
- Containers and tabviews

### [interactive_example.json](interactive_example.json) 
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
mosquitto_pub -h <broker_ip> -t "hmi/config" -f single_screen_example.json

# Or with the provided script
./send_mqtt_json.sh <broker_ip> hmi/config single_screen_example.json
```

### Line Chart Live Data (Sinusoidal)

Use the helper script in `examples/` to continuously publish sinusoidal values for the line chart demo.

```bash
# From project root
python3 examples/publish_sine_chart.py --broker 192.168.100.200
```

Defaults:
- Topic: `demo/line_chart`
- Sample rate: `2` samples/second
- Period: `18` samples

### Image Widget with Base64 - Step by Step

#### Method 1: Using Base64 in Initial Configuration

```bash
cd examples/images
python3 convert.py iot.png iot.qoi
# This creates iot.qoi and iot.qoi.base64.txt
```

**Step 2:** Copy the base64 string (open the .qoi.base64.txt file)

**Step 3:** Create or modify your JSON config with the base64 data:
```json
{
  "version": 1,
  "widgets": [
    {
      "type": "image",
      "id": "my_image",
      "x": 100,
      "y": 100,
      "w": 128,
      "h": 128,
      "properties": {
        "image_path": "cW9pZg...BASE64_DATA_FROM_iot.qoi.base64.txt...",
        "mqtt_topic": "demo/image_base64"
      }
    }
  ]
}
```

**Step 4:** Load the configuration
```bash
mosquitto_pub -h 192.168.100.200 -t "hmi/config" -f your_config.json
```

#### Method 2: Sending Base64 via MQTT (Dynamic Updates)

```bash
cd examples/images
python3 convert.py logo.png logo.qoi
# Creates logo.qoi and logo.qoi.base64.txt
```

```bash
# Load base64 data into variable
BASE64_DATA=$(cat logo.qoi.base64.txt)

# Send to image widget's MQTT topic
mosquitto_pub -h 192.168.100.200 -t "demo/image_base64" -m "$BASE64_DATA"
```

```bash
# Display IoT icon
BASE64_IOT=$(cat iot.qoi.base64.txt)
mosquitto_pub -h 192.168.100.200 -t "demo/image_base64" -m "$BASE64_IOT"

# Display Logo
BASE64_LOGO=$(cat logo.qoi.base64.txt)
mosquitto_pub -h 192.168.100.200 -t "demo/image_base64" -m "$BASE64_LOGO"
```

#### Method 3: Batch Processing Multiple Images

**Script to convert multiple images:**
```bash
#!/bin/bash
cd examples/images
for img in *.png *.jpg; do
  echo "Converting $img..."
  python3 convert.py "$img" "${img%.*}.qoi"
done

echo "All images converted!"
ls -lh *.qoi *.qoi.base64.txt
```

**Send multiple images to different widgets:**
```bash
# Define your MQTT broker
BASE64_IOT=$(cat examples/images/iot.qoi.base64.txt)

# Send to image widget on demo tab (image_example.json)
mosquitto_pub -h $BROKER -t "demo/image1" -m "/sdcard/lena.qoi"
mosquitto_pub -h $BROKER -t "demo/image2" -m "/sdcard/industrial.qoi"
mosquitto_pub -h $BROKER -t "demo/image3" -m "/sdcard/iot.qoi"
```

#### Best Practices for Base64 Images

1. **Image Size Recommendations:**
   - Small icons: 32x32 to 64x64 (< 5 KB)
   - Medium images: 128x128 (< 20 KB)
   - Large images: 256x256 (< 50 KB)
   - Max recommended: 300x300 (< 100 KB)

2. **Optimize Before Encoding:**
```bash
# Install ImageMagick if needed
sudo apt-get install imagemagick

# Resize and optimize
convert input.png -resize 128x128 output.png
python3 convert.py output.png output.qoi
```

3. **Testing Your Images:**
```bash
# Check encoded file size
ls -lh images/*.qoi.base64.txt

# Quick test - display in image widget
mosquitto_pub -h 192.168.100.200 -t "demo/image1" -f images/test.qoi.base64.txt
```

#### Troubleshooting Base64 Images

**Image not displaying?**
```bash
# 1. Check file size (should be < 100KB for base64)
ls -lh images/yourimage.qoi

# 2. Verify base64 encoding worked
head -c 100 images/yourimage.qoi.base64.txt
# Should see base64 characters: A-Z, a-z, 0-9, +, /, =

# 3. Test with a known-good image
mosquitto_pub -h 192.168.100.200 -t "demo/image1" -m "/sdcard/test.qoi"

# 4. Check MQTT message size
wc -c images/yourimage.qoi.base64.txt
# Should be under 100000 bytes (100KB)
```

**Payload too large?**
```bash
# Option 1: Reduce image resolution
convert large.png -resize 128x128 small.png
python3 convert.py small.png small.qoi

# Option 2: Use SD card instead (if available)
cp yourimage.qoi /path/to/sdcard/
mosquitto_pub -h 192.168.100.200 -t "demo/image1" -m "/sdcard/yourimage.qoi"
```

#### Complete Example Workflow

```bash
# 1. Navigate to examples/images directory
cd /path/to/FlexiHMI-MQTT/examples/images

# 2. Prepare your image (resize if needed)
convert iot.png -resize 128x128 iot_small.png

# 3. Encode to QOI + base64
python3 convert.py iot_small.png iot_small.qoi

# 4. Verify encoding
echo "File size: $(wc -c < iot_small.qoi.base64.txt) bytes"
echo "First 50 chars: $(head -c 50 iot_small.qoi.base64.txt)"

# 5. Send to device
BASE64_DATA=$(cat iot_small.qoi.base64.txt)
mosquitto_pub -h 192.168.100.200 -t "demo/image_base64" -m "$BASE64_DATA"

# 6. Watch serial monitor to confirm
# Should see: "Successfully decoded XXXX bytes of image data"
```

### Testing Individual Widgets

Start with the basic examples to understand each widget type:

1. **Labels:** `label_example.json` - Learn text display
2. **Buttons:** `button_example.json` - Learn interaction
3. **Switches:** `switch_example.json` - Learn state management
4. **Sliders:** `slider_example.json` - Learn value controls
5. **Gauges:** `gauge_example_1.json` - Learn analog displays
6. **Line Charts:** `line_chart_example.json` - Learn real-time trend plotting
7. **Images:** `image_example.json` - Learn image loading

### Building Complex UIs

Progress to the advanced examples:

1. **Tabview:** `tabview_demo.json` - Learn multi-screen layouts
2. **Single Screen:** `single_screen_example.json` - See many widgets together
3. **Interactive:** `interactive_example.json` - Study production UI

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

### Property Naming Conventions

All widgets follow consistent property naming:
- **Range properties:** `min`, `max` (Slider, Arc, Bar, Gauge)
- **MQTT properties:** `mqtt_topic`, `mqtt_payload`, `mqtt_retained`
- **Color properties:** `color`, `bg_color`, `text_color`, etc.
- **Position/Size:** `x`, `y`, `w`, `h` (all widgets)

**Example - Gauge with Range:**
```json
{
  "type": "gauge",
  "id": "speedometer",
  "x": 100,
  "y": 100,
  "w": 250,
  "h": 250,
  "properties": {
    "min": 0,
    "max": 200,
    "value": 75,
    "mqtt_topic": "vehicle/speed"
  }
}
```

## Tips

1. **Start Simple:** Begin with single-widget examples before complex UIs
2. **Use Tabs:** For complex UIs, organize widgets into tabs (see `interactive_example.json`)
3. **MQTT Topics:** Use descriptive topic names for easier debugging
4. **Widget IDs:** Keep IDs unique and descriptive
5. **Positioning:** ESP32-P4 Function EV Board display is 1024x600 pixels
6. **Colors:** Use hex colors (e.g., `#FF0000` for red) for consistent styling
7. **Base64 Images:** Keep under 100KB for best performance (see image widget docs)
8. **Property Names:** All range-based widgets (Slider, Arc, Bar, Gauge) use `min`/`max` properties consistently
9. **Buffer Size:** System supports JSON configs up to 512KB (standard) or 1MB (authenticated)

## Creating Your Own

1. Copy an existing example as a template
2. Modify widget positions and properties
3. Test with `mosquitto_pub -h <broker> -t hmi/config -f your_config.json`
4. Iterate based on display results

## File Sizes

- Simple examples: ~1-3 KB
- Complex demos: ~15-25 KB
**MQTT Buffer Capacity:**
- Standard configurations: Up to 512 KB
- Authenticated connections: Up to 1 MB
- Automatic chunked message handling for large configs

Note: MQTT buffers are configured for up to 512 KB messages.

## Common Patterns

### Dashboard Layout
See `interactive_example.json` for a professional dashboard with:
- Themed sections
- Organized tabs
- Consistent spacing
- Color-coded widgets

### Monitoring System
See `gauge_example_0.json` for real-time monitoring:
- Multiple gauges
- Status labels
- Alert indicators

### Control Panel
See `single_screen_example.json` for device control:
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
- See [../../docs/WIDGETS.md](../../docs/WIDGETS.md)

## Support

For more information:
- Main documentation: [../../README.md](../../README.md)
- Widget specifications: [../../docs/WIDGETS.md](../../docs/WIDGETS.md)
