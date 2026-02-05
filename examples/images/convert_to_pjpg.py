#!/usr/bin/env python3
"""
Convert JPEG/PNG images to PJPG format for ESP32-P4 hardware-accelerated decoding.
PNG RGBA inputs are encoded as RGB JPEG + optional alpha JPEG.

Usage:
    ./convert_to_pjpg.py input.jpg output.pjpg
    ./convert_to_pjpg.py input.png output.pjpg
    ./convert_to_pjpg.py input.png output.pjpg --stdout
    ./convert_to_pjpg.py input.png output.pjpg --no-base64
"""

import sys
import os
import struct
import base64
import io

try:
    from PIL import Image
except ImportError:
    Image = None

def create_pjpg(rgb_jpeg_data, width, height, alpha_jpeg_data=None):
    """
    Create PJPG format from JPEG data.
    
    PJPG Format Structure (little-endian fields):
    - 7 bytes magic: "_PJPG__"
    - 1 byte version: 0x00
    - 2 bytes width
    - 2 bytes height
    - 4 bytes RGB JPEG size
    - 4 bytes Alpha JPEG size (0 if none)
    - 2 bytes reserved (0)
    - RGB JPEG data
    - Alpha JPEG data (optional)
    
    Args:
        jpeg_data: Raw JPEG binary data
        width: Image width in pixels
        height: Image height in pixels
    
    Returns:
        bytes: PJPG format data
    """
    
    magic = b"_PJPG__"
    version = 0
    alpha_jpeg_data = alpha_jpeg_data or b""
    rgb_size = len(rgb_jpeg_data)
    alpha_size = len(alpha_jpeg_data)
    reserved = 0

    header = struct.pack("<7sBHHIIH", magic, version, width, height, rgb_size, alpha_size, reserved)
    return header + rgb_jpeg_data + alpha_jpeg_data


def encode_jpeg_from_image(image, quality=85, grayscale=False):
    if grayscale:
        image = image.convert("L")
        subsampling = 0  # not used for grayscale
    else:
        image = image.convert("RGB")
        subsampling = 2  # 4:2:0 for HW decoder compatibility
    buf = io.BytesIO()
    image.save(
        buf,
        format="JPEG",
        quality=quality,
        optimize=False,
        progressive=False,
        subsampling=subsampling,
    )
    return buf.getvalue()


def strip_jpeg_metadata(jpeg_data, keep_app0=True):
    """Strip APPn/COM markers to maximize HW decoder compatibility."""
    if not jpeg_data or jpeg_data[:2] != b"\xFF\xD8":
        return jpeg_data
    size = len(jpeg_data)
    out = bytearray()
    out += b"\xFF\xD8"
    i = 2
    while i < size:
        if jpeg_data[i] != 0xFF:
            out += jpeg_data[i:]
            break
        while i < size and jpeg_data[i] == 0xFF:
            i += 1
        if i >= size:
            break
        marker = jpeg_data[i]
        i += 1
        if marker == 0xD9:
            out += b"\xFF\xD9"
            break
        if marker == 0xDA:
            if i + 1 >= size:
                break
            length = (jpeg_data[i] << 8) | jpeg_data[i + 1]
            out += b"\xFF" + bytes([marker]) + jpeg_data[i:i + length]
            i += length
            out += jpeg_data[i:]
            break
        if 0xD0 <= marker <= 0xD7:
            out += b"\xFF" + bytes([marker])
            continue
        if i + 1 >= size:
            break
        length = (jpeg_data[i] << 8) | jpeg_data[i + 1]
        seg_end = i + length
        if seg_end > size:
            break
        keep = True
        if marker == 0xFE:
            keep = False
        if 0xE0 <= marker <= 0xEF:
            if not (keep_app0 and marker == 0xE0):
                keep = False
        if keep:
            out += b"\xFF" + bytes([marker]) + jpeg_data[i:i + length]
        i = seg_end
    return bytes(out)


def parse_jpeg_sof(jpeg_data):
    """Return (marker, precision, width, height, components, sampling_factors) or None."""
    i = 0
    size = len(jpeg_data)
    while i + 9 < size:
        if jpeg_data[i] == 0xFF:
            marker = jpeg_data[i + 1]
            if marker in (0xC0, 0xC1, 0xC2):
                length = (jpeg_data[i + 2] << 8) | jpeg_data[i + 3]
                if i + 2 + length > size:
                    return None
                precision = jpeg_data[i + 4]
                height = (jpeg_data[i + 5] << 8) | jpeg_data[i + 6]
                width = (jpeg_data[i + 7] << 8) | jpeg_data[i + 8]
                comps = jpeg_data[i + 9]
                sampling = []
                pos = i + 10
                for _ in range(comps):
                    if pos + 2 >= size:
                        return None
                    cid = jpeg_data[pos]
                    samp = jpeg_data[pos + 1]
                    h = (samp >> 4) & 0x0F
                    v = samp & 0x0F
                    sampling.append((cid, h, v))
                    pos += 3
                return (marker, precision, width, height, comps, sampling)
        i += 1
    return None


def is_baseline_420(jpeg_data):
    info = parse_jpeg_sof(jpeg_data)
    if not info:
        return False
    marker, precision, _, _, comps, sampling = info
    # SOF0 only (baseline)
    if marker != 0xC0:
        return False
    if precision != 8:
        return False
    # Expect 3 components, Y:2x2, Cb:1x1, Cr:1x1
    if comps != 3 or len(sampling) != 3:
        return False
    # Order usually Y(1), Cb(2), Cr(3)
    y = sampling[0]
    cb = sampling[1]
    cr = sampling[2]
    return (y[1], y[2]) == (2, 2) and (cb[1], cb[2]) == (1, 1) and (cr[1], cr[2]) == (1, 1)


def is_baseline_gray(jpeg_data):
    info = parse_jpeg_sof(jpeg_data)
    if not info:
        return False
    marker, precision, _, _, comps, sampling = info
    if marker != 0xC0:
        return False
    if precision != 8:
        return False
    if comps != 1 or len(sampling) != 1:
        return False
    return True


def get_jpeg_dimensions(jpeg_data):
    """
    Extract width and height from JPEG data by parsing SOF markers.
    
    Args:
        jpeg_data: Raw JPEG binary data
    
    Returns:
        tuple: (width, height) or None if parsing fails
    """
    i = 0
    while i < len(jpeg_data) - 9:
        if jpeg_data[i] == 0xFF:
            marker = jpeg_data[i + 1]
            # SOF markers: C0-CF (except C4, C8, CC)
            if ((marker >= 0xC0 and marker <= 0xC3) or 
                (marker >= 0xC5 and marker <= 0xC7) or
                (marker >= 0xC9 and marker <= 0xCB) or
                (marker >= 0xCD and marker <= 0xCF)):
                # SOF structure: FF Cx [length:2] [precision:1] [height:2] [width:2]
                height = (jpeg_data[i + 5] << 8) | jpeg_data[i + 6]
                width = (jpeg_data[i + 7] << 8) | jpeg_data[i + 8]
                return (width, height)
        i += 1
    return None


def convert_image_to_pjpg(input_path, output_path, create_base64=True, to_stdout=False, quality=85):
    """
    Convert JPEG file to PJPG format.
    
    Args:
        input_path: Path to input JPEG file
        output_path: Path to output PJPG file
        create_base64: If True, also create a base64 encoded text file
    """
    
    ext = os.path.splitext(input_path)[1].lower()

    if ext in [".jpg", ".jpeg"]:
        print(f"Reading JPEG: {input_path}")
        if Image is None:
            print("ERROR: Pillow is required to re-encode JPEGs for HW compatibility. Install with: pip install Pillow")
            return False

        image = Image.open(input_path).convert("RGB")
        width, height = image.size
        print(f"  Dimensions: {width}x{height}")
        rgb_jpeg_data = encode_jpeg_from_image(image, quality=quality, grayscale=False)
        rgb_jpeg_data = strip_jpeg_metadata(rgb_jpeg_data)
        if not is_baseline_420(rgb_jpeg_data):
            print("ERROR: Re-encoded JPEG is still not baseline 4:2:0 (8-bit)")
            return False

        print(f"  JPEG size: {len(rgb_jpeg_data)} bytes")
        pjpg_data = create_pjpg(rgb_jpeg_data, width, height)

    elif ext == ".png":
        if Image is None:
            print("ERROR: Pillow is required for PNG input. Install with: pip install Pillow")
            return False

        print(f"Reading PNG: {input_path}")
        image = Image.open(input_path).convert("RGB")
        width, height = image.size
        print(f"  Dimensions: {width}x{height}")

        # Encode RGB JPEG (baseline 4:2:0)
        rgb_jpeg_data = encode_jpeg_from_image(image, quality=quality, grayscale=False)
        rgb_jpeg_data = strip_jpeg_metadata(rgb_jpeg_data)
        if not is_baseline_420(rgb_jpeg_data):
            print("ERROR: RGB JPEG is not baseline 4:2:0; hardware decoder requires baseline 4:2:0")
            return False
        print(f"  RGB JPEG size: {len(rgb_jpeg_data)} bytes")

        pjpg_data = create_pjpg(rgb_jpeg_data, width, height)
    else:
        print(f"ERROR: Unsupported input format: {ext} (supported: .jpg, .jpeg, .png)")
        return False
    
    # Write PJPG file
    print(f"Writing PJPG: {output_path}")
    with open(output_path, 'wb') as f:
        f.write(pjpg_data)
    print(f"  PJPG size: {len(pjpg_data)} bytes")
    
    # Create base64 version (default)
    if create_base64 or to_stdout:
        base64_data = base64.b64encode(pjpg_data).decode('ascii')
        if create_base64:
            base64_path = output_path + '.base64.txt'
            print(f"Creating base64: {base64_path}")
            with open(base64_path, 'w') as f:
                f.write(base64_data)
            print(f"  Base64 size: {len(base64_data)} bytes")
        if to_stdout:
            print(base64_data)
    
    print("✓ Conversion complete")
    return True


def main():
    if len(sys.argv) < 3:
        print("Usage: convert_to_pjpg.py <input.jpg|input.png> <output.pjpg> [--stdout] [--no-base64]")
        print("")
        print("Converts JPEG/PNG to PJPG format for ESP32-P4 hardware-accelerated decoding")
        print("")
        print("Options:")
        print("  --stdout     Print base64 to stdout")
        print("  --no-base64  Do not create base64 file")
        sys.exit(1)
    
    input_path = sys.argv[1]
    output_path = sys.argv[2]
    create_base64 = '--no-base64' not in sys.argv
    to_stdout = '--stdout' in sys.argv
    
    if not os.path.exists(input_path):
        print(f"ERROR: Input file not found: {input_path}")
        sys.exit(1)
    
    success = convert_image_to_pjpg(input_path, output_path, create_base64, to_stdout)
    sys.exit(0 if success else 1)


if __name__ == '__main__':
    main()
