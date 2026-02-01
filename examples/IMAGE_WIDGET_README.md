# Image Widget - Quick Start

The Image Widget displays JPEG, PNG, BMP, or GIF images from either SD card storage or base64-encoded data received via MQTT.

## Features

- ✅ Load images from SD card filesystem
- ✅ Receive base64-encoded images via MQTT
- ✅ Hardware-accelerated JPEG decoding (ESP32-P4)
- ✅ Automatic format detection
- ✅ Thread-safe MQTT updates
- ✅ Memory-efficient design

## Quick Examples

### Example 1: Load from SD Card

```json
{
  "type": "image",
  "id": "logo",
  "x": 100,
  "y": 100,
  "w": 256,
  "h": 256,
  "properties": {
    "image_path": "/sdcard/logo.png",
    "mqtt_topic": "home/display/logo"
  }
}
```

Send new image via MQTT:
```bash
mosquitto_pub -h broker -t "home/display/logo" -m "/sdcard/photo.jpg"
```

### Example 2: Load from Base64

```json
{
  "type": "image",
  "id": "camera",
  "x": 300,
  "y": 100,
  "w": 320,
  "h": 240,
  "properties": {
    "mqtt_topic": "camera/snapshot"
  }
}
```

Encode and send image:
```bash
# Step 1: Encode image to base64
python3 examples/encode_image_to_base64.py snapshot.jpg

# Step 2: Send via MQTT
mosquitto_pub -h broker -t "camera/snapshot" -f snapshot.jpg.base64.txt
```

## Properties

| Property | Type | Description | Required |
|----------|------|-------------|----------|
| `image_path` | string | Initial image path on SD card (e.g., `/sdcard/logo.png`) | No |
| `mqtt_topic` | string | MQTT topic to subscribe for updates | No |

## Supported Formats

- **JPEG** (.jpg, .jpeg) - Hardware accelerated on ESP32-P4
- **PNG** (.png) - With transparency support
- **BMP** (.bmp) - Windows bitmap
- **GIF** (.gif) - Static and animated

## Loading Methods

### SD Card Loading

**Advantages:**
- No memory overhead
- Supports large images
- Fast access
- Pre-loaded content

**Use Cases:**
- Static UI elements
- Logos and branding
- Background images
- Pre-installed assets

**Setup:**
1. Copy images to SD card
2. Insert SD card into ESP32-P4
3. Reference with `/sdcard/filename.ext` in JSON or MQTT

### Base64 Loading

**Advantages:**
- Dynamic updates via MQTT
- No filesystem required
- Remote control
- Real-time content

**Use Cases:**
- Camera snapshots
- QR codes
- Dynamic content
- Small icons

**Limitations:**
- Data size ~33% larger than original
- MQTT message size limits
- Memory usage (decoded in RAM)

**Recommendations:**
| Original Size | Base64 Size | Recommendation |
|---------------|-------------|----------------|
| <20KB         | <27KB       | ✅ Excellent |
| 20-50KB       | 27-67KB     | ⚠️ Use with caution |
| 50-100KB      | 67-133KB    | ⚠️ May fail (memory) |
| >100KB        | >133KB      | ❌ Use SD card |

## Python Encoding Script

Located at `examples/encode_image_to_base64.py`:

```bash
# Basic usage
python3 examples/encode_image_to_base64.py image.jpg

# Output saved to: image.jpg.base64.txt
```

The script:
- Reads any image file
- Encodes to base64
- Prints to console
- Saves to .txt file for easy MQTT transmission

## Complete Example

See `examples/image_example.json` for a complete demonstration with:
- SD card image loading
- Base64 image loading
- Control buttons
- Instructions

## Serial Monitor Output

Successful SD card load:
```
I (12345) ImageWidget: Updating image with new data (19 bytes)
I (12346) ImageWidget: Loading image from path: /sdcard/lena_256.jpg
I (12347) ImageWidget: File exists: 28574 bytes
I (12348) ImageWidget: Setting image source: S:/lena_256.jpg
I (12349) ImageWidget: Successfully loaded image from: /sdcard/lena_256.jpg
```

Successful base64 load:
```
I (23456) ImageWidget: Received MQTT message (38100 bytes) on topic: demo/image_base64
I (23457) ImageWidget: Updating image with new data (38100 bytes)
I (23458) ImageWidget: Decoding base64 image data (38100 bytes)
I (23459) ImageWidget: Base64 decode will produce 28574 bytes
I (23460) ImageWidget: Successfully decoded 28574 bytes of image data
I (23461) ImageWidget: Base64 image loaded successfully
```

## Troubleshooting

### Image Not Displaying

1. **Check file path:** Ensure image exists at `/sdcard/filename.ext`
2. **Check serial output:** Look for error messages
3. **Verify format:** Only JPEG, PNG, BMP, GIF supported
4. **Check memory:** Large images may fail to allocate

### Base64 Decode Errors

1. **Incomplete data:** Ensure entire base64 string transmitted
2. **Whitespace:** Script removes whitespace automatically
3. **Message size:** MQTT broker may have message size limits
4. **Memory:** Free heap may be insufficient for large images

### SD Card Issues

1. **Not mounted:** Check SD card initialization in serial output
2. **Wrong path:** Use `/sdcard/` prefix, not `S:/`
3. **File permissions:** Ensure files are readable
4. **Corrupted files:** Try re-copying image to SD card

## Technical Details

- **Implementation:** `components/hmi_widgets/image_widget.cpp`
- **Header:** `components/hmi_widgets/include/image_widget.h`
- **Base64 decoder:** mbedtls/base64.h (ESP-IDF built-in)
- **LVGL filesystem:** POSIX mapping `/sdcard` → `S:`
- **Thread safety:** `lv_async_call()` for all MQTT callbacks
- **Image decoder:** LVGL auto-detects format from binary data

## Further Reading

- [WIDGETS.md](../docs/WIDGETS.md#16-image-widget) - Complete widget documentation
- [BASE64_IMAGE_TESTING.md](BASE64_IMAGE_TESTING.md) - Comprehensive testing guide
- [image_example.json](image_example.json) - JSON configuration example
- [encode_image_to_base64.py](encode_image_to_base64.py) - Encoding script

## Quick Commands

```bash
# Test SD card image
mosquitto_pub -h broker -t "demo/image" -m "/sdcard/test.jpg"

# Encode image
python3 examples/encode_image_to_base64.py test.jpg

# Send base64 image
mosquitto_pub -h broker -t "demo/image" -f test.jpg.base64.txt

# Monitor ESP32 serial output
idf.py -p /dev/ttyUSB0 monitor
```
