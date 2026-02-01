#!/usr/bin/env python3
"""
Script to encode images to base64 format for MQTT transmission to ESP32-P4 Image Widget.
Usage:
    python3 encode_image_to_base64.py <image_file>
    
Example:
    python3 encode_image_to_base64.py lena_256.jpg
    
This will output the base64-encoded image data that can be sent via MQTT.
You can also save it to a file or copy to clipboard for testing.
"""

import base64
import sys
import os

def encode_image_to_base64(image_path):
    """Read an image file and encode it to base64."""
    try:
        with open(image_path, 'rb') as image_file:
            image_data = image_file.read()
            base64_data = base64.b64encode(image_data).decode('utf-8')
            return base64_data
    except FileNotFoundError:
        print(f"Error: File '{image_path}' not found.")
        return None
    except Exception as e:
        print(f"Error reading file: {e}")
        return None

def main():
    if len(sys.argv) < 2:
        print("Usage: python3 encode_image_to_base64.py <image_file>")
        print("\nExample:")
        print("  python3 encode_image_to_base64.py my_image.jpg")
        print("\nSupported formats: JPEG, PNG, BMP, GIF")
        sys.exit(1)
    
    image_path = sys.argv[1]
    
    if not os.path.exists(image_path):
        print(f"Error: File '{image_path}' does not exist.")
        sys.exit(1)
    
    print(f"Encoding image: {image_path}")
    print(f"File size: {os.path.getsize(image_path)} bytes")
    
    base64_data = encode_image_to_base64(image_path)
    
    if base64_data:
        print(f"Base64 size: {len(base64_data)} bytes")
        print("\n" + "="*80)
        print("BASE64 ENCODED IMAGE DATA:")
        print("="*80)
        print(base64_data)
        print("="*80)
        
        # Optionally save to file
        output_file = image_path + ".base64.txt"
        with open(output_file, 'w') as f:
            f.write(base64_data)
        print(f"\nBase64 data also saved to: {output_file}")
        
        print("\nTo send via MQTT:")
        print(f"  mosquitto_pub -h <broker> -t 'esp32/image1/mqtt_image' -m '{base64_data[:60]}...'")
        print("\nOr use the base64 data in your JSON configuration:")
        print('  "mqtt_image": "' + base64_data[:60] + '..."')

if __name__ == "__main__":
    main()
