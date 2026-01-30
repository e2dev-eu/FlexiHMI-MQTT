# Custom Fonts Setup

## Prerequisites
```bash
npm install -g lv_font_conv
```

## Download Fonts
1. **Inter**: Download from https://rsms.me/inter/ 
   - Get: Inter-Regular.ttf, Inter-Medium.ttf
   
2. **Material Symbols**: Download from https://github.com/google/material-design-icons
   - Get: MaterialSymbolsOutlined[FILL,GRAD,opsz,wght].ttf
   - Or download directly: https://fonts.google.com/icons

Place downloaded font files in this directory.

## Convert Fonts

Run these commands from this directory:

```bash
# Inter Regular 14px (small text)
lv_font_conv --font Inter-Regular.ttf --size 14 --format lvgl --bpp 4 \
  --range 0x20-0x7F --output inter_14.c

# Inter Regular 18px (normal text)
lv_font_conv --font Inter-Regular.ttf --size 18 --format lvgl --bpp 4 \
  --range 0x20-0x7F --output inter_18.c

# Inter Medium 24px (headers)
lv_font_conv --font Inter-Medium.ttf --size 24 --format lvgl --bpp 4 \
  --range 0x20-0x7F --output inter_24.c

# Material Symbols 20px (small icons)
lv_font_conv --font MaterialSymbolsOutlined-VariableFont_FILL,GRAD,opsz,wght.ttf --size 20 --format lvgl --bpp 4 \
  --range 0xE63E,0xE1D8,0xE1CE,0xE897,0xE5D5 \
  --output material_symbols_20.c

# Material Symbols 32px (large icons - gear button)
lv_font_conv --font MaterialSymbolsOutlined-VariableFont_FILL,GRAD,opsz,wght.ttf --size 32 --format lvgl --bpp 4 \
  --range 0xE8B8 \
  --output material_symbols_32.c
```

## Icon Reference (Material Symbols Unicode Codepoints)
- wifi - 0xE63E (used in code as: "\uE63E")
- signal_cellular_alt (medium) - 0xE1D8 (used in code as: "\uE1D8")
- signal_cellular_alt_1_bar (weak) - 0xE1CE (used in code as: "\uE1CE")
- lock - 0xE897 (used in code as: "\uE897")
- refresh - 0xE5D5 (used in code as: "\uE5D5")
- settings - 0xE8B8 (used in code as: "\uE8B8")

After running these commands, rebuild the project with `idf.py build`.
