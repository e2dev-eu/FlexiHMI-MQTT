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
Analog-style circular gauges with standardized range properties.

**Features:**
- Speed gauge (0-200 range with `min`/`max` properties)
- Temperature gauge (-20 to 120 range)
- Interactive sliders to control gauge values
- Real-time MQTT updates
- Demonstrates consistent property naming across all range-based widgets

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

### Image Widget with Base64 - Step by Step

#### Method 1: Using Base64 in Initial Configuration

**Step 1:** Encode your image to base64
```bash
cd examples
python3 encode_image_to_base64.py images/iot.png
# This creates images/iot.png.base64.txt
```

**Step 2:** Copy the base64 string (open the .base64.txt file)

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
        "image_path": "iVBORw0KGgoAAAANSUhEUgAAAIAAAACA...BASE64_DATA_FROM_iot.png.base64.txt...",
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

**Step 1:** Encode your image
```bash
cd examples
python3 encode_image_to_base64.py images/logo.png
# Creates images/logo.png.base64.txt
```

**Step 2:** Send base64 data via MQTT (replaces existing image)
```bash
# Load base64 data into variable
BASE64_DATA=$(cat images/logo.png.base64.txt)

# Send to image widget's MQTT topic
mosquitto_pub -h 192.168.100.200 -t "demo/image_base64" -m "$BASE64_DATA"
```

**Step 3:** Switch between different images dynamically
```bash
# Display IoT icon
BASE64_IOT=$(cat images/iot.png.base64.txt)
mosquitto_pub -h 192.168.100.200 -t "demo/image_base64" -m "$BASE64_IOT"

# Display Lena image
mosquitto_pub -h 192.168.100.200 -t "demo/image_sdcard" -m "/sdcard/lenna256.png"

# Display Logo
BASE64_LOGO=$(cat images/logo.png.base64.txt)
mosquitto_pub -h 192.168.100.200 -t "demo/image_base64" -m "$BASE64_LOGO"
```

#### Method 3: Batch Processing Multiple Images

**Script to encode multiple images:**
```bash
#!/bin/bash
cd examples/images

for img in *.png *.jpg; do
  echo "Encoding $img..."
  python3 ../encode_image_to_base64.py "$img"
done

echo "All images encoded!"
ls -lh *.base64.txt
```

**Send multiple images to different widgets:**
```bash
# Define your MQTT broker
BROKER="192.168.100.200"

# Send to image widget on demo tab (image_example.json)
BASE64_IOT=$(cat examples/images/iot.png.base64.txt)
mosquitto_pub -h $BROKER -t "demo/image_base64" -m "$BASE64_IOT"

# Send to image widgets on interactive demo (different topics)
mosquitto_pub -h $BROKER -t "demo/image1" -m "/sdcard/lenna256.png"
mosquitto_pub -h $BROKER -t "demo/image2" -m "/sdcard/logo.png"
mosquitto_pub -h $BROKER -t "demo/image3" -m "/sdcard/iot.png"
```

#### Best Practices for Base64 Images

1. **Image Size Recommendations:**
   - Small icons: 32x32 to 64x64 (< 5 KB)
   - Medium images: 128x128 (< 20 KB)
   - Large images: 256x256 (< 50 KB)
   - Max recommended: 300x300 (< 100 KB)

2. **Format Selection:**
   - **PNG:** Best for icons, logos, transparency (larger file size)
   - **JPEG:** Best for photos, no transparency (smaller file size)
   - **GIF:** Animated images supported

3. **Optimize Before Encoding:**
```bash
# Install ImageMagick if needed
sudo apt-get install imagemagick

# Resize and optimize
convert input.jpg -resize 128x128 -quality 85 output.jpg
python3 encode_image_to_base64.py output.jpg
```

4. **Testing Your Images:**
```bash
# Check encoded file size
ls -lh images/*.base64.txt

# Quick test - display in image widget
mosquitto_pub -h 192.168.100.200 -t "demo/image1" -f images/test.png.base64.txt
```

#### Troubleshooting Base64 Images

**Image not displaying?**
```bash
# 1. Check file size (should be < 100KB for base64)
ls -lh images/yourimage.png

# 2. Verify base64 encoding worked
head -c 100 images/yourimage.png.base64.txt
# Should see base64 characters: A-Z, a-z, 0-9, +, /, =

# 3. Test with a known-good image
mosquitto_pub -h 192.168.100.200 -t "demo/image1" -m "/sdcard/test.jpg"

# 4. Check MQTT message size
wc -c images/yourimage.png.base64.txt
# Should be under 100000 bytes (100KB)
```

**Payload too large?**
```bash
# Option 1: Reduce image resolution
convert large.png -resize 128x128 small.png

# Option 2: Increase JPEG compression
convert photo.jpg -quality 75 compressed.jpg

# Option 3: Use SD card instead
cp yourimage.jpg /path/to/sdcard/
mosquitto_pub -h 192.168.100.200 -t "demo/image1" -m "/sdcard/yourimage.jpg"
```

#### Complete Example Workflow

```bash
# 1. Navigate to examples directory
cd /path/to/ESP32P4-MQTT-Panel/examples

# 2. Prepare your image (resize if needed)
convert iot.png -resize 128x128 images/iot_small.png

# 3. Encode to base64
python3 encode_image_to_base64.py images/iot_small.png

# 4. Verify encoding
echo "File size: $(wc -c < images/iot_small.png.base64.txt) bytes"
echo "First 50 chars: $(head -c 50 images/iot_small.png.base64.txt)"

# 5. Send to device
BASE64_DATA=$(cat images/iot_small.png.base64.txt)
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
2. **Use Tabs:** For complex UIs, organize widgets into tabs (see `interactive_complete_demo.json`)
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
