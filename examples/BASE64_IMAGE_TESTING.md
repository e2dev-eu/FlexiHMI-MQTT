# Base64 Image Testing Guide

This guide explains how to test the Image Widget's base64 encoding feature.

## Overview

The Image Widget supports two methods for loading images:
1. **SD Card** - Load images from the filesystem (e.g., `/sdcard/photo.jpg`)
2. **Base64** - Receive base64-encoded image data via MQTT

## Prerequisites

- ESP32-P4 with image_example.json loaded
- MQTT broker accessible
- Python 3.x with standard library
- Test images (JPEG, PNG, BMP, or GIF)

## Step 1: Encode Images to Base64

Use the provided Python script to encode your images:

```bash
cd examples
python3 encode_image_to_base64.py your_image.jpg
```

This will:
- Read the image file
- Encode it to base64
- Print the base64 string to console
- Save it to `your_image.jpg.base64.txt`

### Example Output

```
Encoding image: lena_256.jpg
File size: 28574 bytes
Base64 size: 38100 bytes

================================================================================
BASE64 ENCODED IMAGE DATA:
================================================================================
/9j/4AAQSkZJRgABAQAAAQABAAD/2wBDAAgGBgcGBQgHBwcJCQgKDBQNDAsLDBkSEw8UHRofHh0a...
================================================================================

Base64 data also saved to: lena_256.jpg.base64.txt
```

## Step 2: Send Base64 Image via MQTT

### Using mosquitto_pub

Copy the base64 string and send it:

```bash
# Read the base64 file and send via MQTT
mosquitto_pub -h <broker_ip> -t "demo/image_base64" -f lena_256.jpg.base64.txt
```

Or send directly:

```bash
mosquitto_pub -h <broker_ip> -t "demo/image_base64" -m "<paste_base64_string_here>"
```

### Using Python MQTT Client

```python
import paho.mqtt.client as mqtt
import base64

# Read and encode image
with open('lena_256.jpg', 'rb') as f:
    image_data = f.read()
    base64_data = base64.b64encode(image_data).decode('utf-8')

# Connect and publish
client = mqtt.Client()
client.connect("broker_ip", 1883, 60)
client.publish("demo/image_base64", base64_data)
client.disconnect()
```

## Step 3: Verify on ESP32-P4

Monitor the serial output to see the image loading:

```
I (12345) ImageWidget: Received MQTT message (38100 bytes) on topic: demo/image_base64
I (12346) ImageWidget: Updating image with new data (38100 bytes)
I (12347) ImageWidget: Decoding base64 image data (38100 bytes)
I (12348) ImageWidget: Base64 decode will produce 28574 bytes
I (12349) ImageWidget: Successfully decoded 28574 bytes of image data
I (12350) ImageWidget: Base64 image loaded successfully
```

The image should appear on the display.

## Testing Checklist

- [ ] Test JPEG image via base64
- [ ] Test PNG image via base64
- [ ] Test BMP image via base64
- [ ] Test small image (<10KB)
- [ ] Test medium image (10-50KB)
- [ ] Test large image (>50KB) - may fail due to memory
- [ ] Switch between SD card and base64 images
- [ ] Send multiple base64 images in sequence
- [ ] Test invalid base64 data (should log error)
- [ ] Test empty payload (should log error)

## Expected Behavior

### Success Cases
- Valid base64 data → Image displays correctly
- SD card path → Image loads from filesystem
- Switching sources → Previous image replaced

### Error Cases
- Invalid base64 → Error logged, previous image retained
- File not found → Error logged, previous image retained
- Out of memory → Error logged during allocation

## Image Size Recommendations

| Image Size | Base64 Size | Recommendation |
|------------|-------------|----------------|
| <20KB      | <27KB       | ✅ Excellent for base64 |
| 20-50KB    | 27-67KB     | ⚠️ Use with caution |
| 50-100KB   | 67-133KB    | ⚠️ May cause memory issues |
| >100KB     | >133KB      | ❌ Use SD card instead |

**Note:** Base64 encoding increases data size by ~33%

## Performance Comparison

### SD Card Loading
- **Pros:** No memory overhead, supports large images, fast access
- **Cons:** Requires pre-loaded images, no dynamic content
- **Use for:** Static UI elements, logos, backgrounds

### Base64 Loading
- **Pros:** Dynamic updates via MQTT, no filesystem needed, remote control
- **Cons:** Memory usage, MQTT message size limits, slower transmission
- **Use for:** Camera snapshots, QR codes, dynamic content, small icons

## Troubleshooting

### Image Not Displaying

1. **Check Serial Output:**
   ```
   I (12345) ImageWidget: Decoding base64 image data...
   ```
   If you see this, decoding is working.

2. **Memory Issues:**
   ```
   E (12345) ImageWidget: Failed to allocate 100000 bytes for decoded image
   ```
   Solution: Use smaller image or SD card method.

3. **Invalid Base64:**
   ```
   E (12345) ImageWidget: Base64 decode failed: -10240
   ```
   Solution: Re-encode image, ensure no truncation.

4. **MQTT Message Too Large:**
   - Broker may reject messages >128KB
   - Solution: Increase broker limit or use smaller images

### Decoding Fails

- Verify base64 string is complete (no truncation)
- Check for line breaks or whitespace in base64 data
- Ensure image format is supported (JPEG, PNG, BMP, GIF)

### Display Issues

- Image might be too large for allocated widget size
- LVGL will scale/clip automatically
- Adjust widget width/height in JSON config

## Advanced Testing

### Rapid Image Updates

Test switching images quickly:

```bash
for i in {1..10}; do
    mosquitto_pub -h broker -t "demo/image_base64" -f image${i}.base64.txt
    sleep 1
done
```

### Mixed Source Testing

Alternate between SD card and base64:

```bash
# Load from SD card
mosquitto_pub -h broker -t "demo/image_sdcard" -m "/sdcard/photo1.jpg"
sleep 2

# Load from base64
mosquitto_pub -h broker -t "demo/image_base64" -f photo2.base64.txt
sleep 2

# Back to SD card
mosquitto_pub -h broker -t "demo/image_sdcard" -m "/sdcard/photo3.jpg"
```

## Sample Images

Recommended test images:
- **lena_256.jpg** - Classic 256x256 test image
- **logo.png** - Transparent logo
- **qrcode.png** - QR code (works great with base64!)
- **icon_32x32.bmp** - Small icon

## Script Reference

### encode_image_to_base64.py

```bash
# Basic usage
python3 encode_image_to_base64.py image.jpg

# With output redirection
python3 encode_image_to_base64.py image.jpg > output.txt

# Batch encoding
for img in *.jpg; do
    python3 encode_image_to_base64.py "$img"
done
```

## Memory Considerations

The ESP32-P4 has limited RAM. Consider:

- **PSRAM:** If available, larger images possible
- **Heap fragmentation:** Sequential base64 loads may fragment heap
- **Buffer size:** mbedtls requires contiguous memory for decoding

Monitor free heap:

```c
ESP_LOGI(TAG, "Free heap: %d bytes", esp_get_free_heap_size());
```

## Best Practices

1. **Optimize images** before encoding:
   - Resize to exact display dimensions
   - Use JPEG quality 70-85%
   - Compress PNG files

2. **Test with actual use case:**
   - Don't just test one image
   - Test the sequence of operations your app will use
   - Monitor memory over time

3. **Implement fallbacks:**
   - Keep default image on SD card
   - Handle decode failures gracefully
   - Log all errors for debugging

4. **Network considerations:**
   - MQTT QoS=1 recommended for reliability
   - Consider chunking very large images
   - Implement retry logic in sender

## See Also

- [WIDGETS.md](../docs/WIDGETS.md) - Full widget documentation
- [image_example.json](image_example.json) - Example configuration
- [encode_image_to_base64.py](encode_image_to_base64.py) - Encoding script
