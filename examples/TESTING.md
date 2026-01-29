# Testing Guide for Complete Demo

This guide explains how to test all widgets using the `complete_demo.json` configuration.

## Prerequisites

1. ESP32-P4 device running the MQTT Panel firmware
2. MQTT broker accessible on your network (e.g., Mosquitto)
3. MQTT client for testing (e.g., mosquitto_pub, MQTT Explorer, or MQTT.fx)

## Loading the Demo Configuration

### Publish via MQTT
```bash
mosquitto_pub -h <BROKER_IP> -t hmi/config -r -f examples/complete_demo.json
```

## Widget Testing Guide

### Left Panel - Controls

#### 1. Master Power Switch
**Widget:** Switch  
**Topic:** `demo/power`  
**Test Commands:**
```bash
# Turn ON
mosquitto_pub -h <BROKER_IP> -t demo/power -m "ON"

# Turn OFF  
mosquitto_pub -h <BROKER_IP> -t demo/power -m "OFF"

# Also accepts: "1"/"0", "true"/"false"
```

**Expected Behavior:**
- Switch toggles between ON/OFF states
- Publishes state change to `demo/power` when toggled locally
- Updates from external MQTT messages without republishing

#### 2. Volume Slider
**Widget:** Slider  
**Topic:** `demo/volume`  
**Range:** 0-100  
**Test Commands:**
```bash
# Set volume to 75
mosquitto_pub -h <BROKER_IP> -t demo/volume -m "75"

# Set to minimum
mosquitto_pub -h <BROKER_IP> -t demo/volume -m "0"

# Set to maximum
mosquitto_pub -h <BROKER_IP> -t demo/volume -m "100"
```

**Expected Behavior:**
- Slider moves to specified position
- Value label updates below slider
- Publishes value when moved locally

#### 3. Brightness Arc
**Widget:** Arc (circular slider)  
**Topic:** `demo/brightness`  
**Range:** 0-100  
**Test Commands:**
```bash
# Set brightness to 50
mosquitto_pub -h <BROKER_IP> -t demo/brightness -m "50"

# Full brightness
mosquitto_pub -h <BROKER_IP> -t demo/brightness -m "100"
```

**Expected Behavior:**
- Arc indicator rotates to specified value
- Orange color indicator
- Bidirectional control (publish/subscribe)

#### 4. Mode Selector
**Widget:** Dropdown  
**Topic:** `demo/mode`  
**Options:** Auto, Manual, Schedule, Off  
**Test Commands:**
```bash
# Select by name
mosquitto_pub -h <BROKER_IP> -t demo/mode -m "Manual"

# Select by index
mosquitto_pub -h <BROKER_IP> -t demo/mode -m "1"

# Available indices: 0=Auto, 1=Manual, 2=Schedule, 3=Off
```

**Expected Behavior:**
- Dropdown displays selected option
- Publishes selection when changed locally
- Updates from external selection

#### 5. Checkboxes
**Widgets:** Checkbox × 2  
**Topics:** `demo/notifications`, `demo/auto_update`  
**Test Commands:**
```bash
# Enable notifications
mosquitto_pub -h <BROKER_IP> -t demo/notifications -m "true"

# Disable auto update
mosquitto_pub -h <BROKER_IP> -t demo/auto_update -m "false"

# Also accepts: "1"/"0", "checked"/"unchecked"
```

**Expected Behavior:**
- Checkbox state toggles
- Publishes state change when clicked locally

#### 6. Apply Settings Button
**Widget:** Button  
**Topic:** `demo/apply`  
**Payload:** "APPLY"  
**Test:**
- Click button on screen
- Monitor MQTT topic for message

**Expected Behavior:**
- Publishes "APPLY" to `demo/apply` when clicked
- No visual feedback beyond click

---

### Middle Panel - Status Indicators

#### 7. Progress Bars (CPU, Memory, Disk)
**Widgets:** Bar × 3  
**Topics:** `demo/cpu`, `demo/memory`, `demo/disk`  
**Range:** 0-100  
**Test Commands:**
```bash
# Set CPU usage to 45%
mosquitto_pub -h <BROKER_IP> -t demo/cpu -m "45"

# Set memory to 67%
mosquitto_pub -h <BROKER_IP> -t demo/memory -m "67"

# Set disk to 82%
mosquitto_pub -h <BROKER_IP> -t demo/disk -m "82"
```

**Expected Behavior:**
- Bar fills to specified percentage
- CPU = Green, Memory = Orange, Disk = Red
- Read-only (does not publish)

#### 8. Temperature & Humidity Labels
**Widgets:** Label × 2  
**Topics:** `demo/temperature`, `demo/humidity`  
**Test Commands:**
```bash
# Update temperature (uses format string)
mosquitto_pub -h <BROKER_IP> -t demo/temperature -m "23.5"

# Update humidity
mosquitto_pub -h <BROKER_IP> -t demo/humidity -m "65"
```

**Expected Behavior:**
- Temperature displays: "Temp: 23.5°C"
- Humidity displays: "Humidity: 65%"
- Formatted according to format property

#### 9. Uptime Label
**Widget:** Label  
**Topic:** `demo/uptime`  
**Test Commands:**
```bash
# Update uptime
mosquitto_pub -h <BROKER_IP> -t demo/uptime -m "3 days, 5:32:10"
```

**Expected Behavior:**
- Displays text as-is (no formatting)

#### 10. Status LEDs
**Widgets:** LED × 3  
**Topics:** `demo/led1`, `demo/led2`, `demo/led3`  
**Test Commands:**
```bash
# Turn LED1 (Network) ON
mosquitto_pub -h <BROKER_IP> -t demo/led1 -m "ON"

# Set LED2 (MQTT) to 50% brightness
mosquitto_pub -h <BROKER_IP> -t demo/led2 -m "128"

# Turn LED3 (Storage) OFF
mosquitto_pub -h <BROKER_IP> -t demo/led3 -m "0"

# Brightness range: 0-255
```

**Expected Behavior:**
- LED brightness changes
- Colors: Green (Network), Blue (MQTT), Orange (Storage)
- Read-only (does not publish)

#### 11. Loading Spinner
**Widget:** Spinner  
**Topic:** `demo/loading`  
**Test Commands:**
```bash
# Show spinner
mosquitto_pub -h <BROKER_IP> -t demo/loading -m "show"

# Hide spinner
mosquitto_pub -h <BROKER_IP> -t demo/loading -m "hide"

# Also accepts: "1"/"0", "true"/"false"
```

**Expected Behavior:**
- Spinner appears/disappears
- Animated rotation when visible

---

### Right Panel - RGB Lighting

#### 12. RGB Power Switch
**Widget:** Switch  
**Topic:** `demo/rgb/power`  
**Test Commands:**
```bash
# Turn RGB ON
mosquitto_pub -h <BROKER_IP> -t demo/rgb/power -m "ON"

# Turn RGB OFF
mosquitto_pub -h <BROKER_IP> -t demo/rgb/power -m "OFF"
```

**Expected Behavior:**
- Similar to master power switch
- Red indicator color

#### 13. RGBW Sliders
**Widgets:** Slider × 4  
**Topics:** `demo/rgb/red`, `demo/rgb/green`, `demo/rgb/blue`, `demo/rgb/white`  
**Range:** 0-255  
**Test Commands:**
```bash
# Set all colors for purple
mosquitto_pub -h <BROKER_IP> -t demo/rgb/red -m "255"
mosquitto_pub -h <BROKER_IP> -t demo/rgb/green -m "0"
mosquitto_pub -h <BROKER_IP> -t demo/rgb/blue -m "128"
mosquitto_pub -h <BROKER_IP> -t demo/rgb/white -m "0"
```

**Expected Behavior:**
- Each slider shows corresponding color
- Values 0-255
- Bidirectional control

#### 14. Effect Selector
**Widget:** Dropdown  
**Topic:** `demo/rgb/effect`  
**Options:** Solid, Fade, Strobe, Rainbow, Chase  
**Test Commands:**
```bash
# Select Rainbow effect
mosquitto_pub -h <BROKER_IP> -t demo/rgb/effect -m "Rainbow"

# Or by index
mosquitto_pub -h <BROKER_IP> -t demo/rgb/effect -m "3"
```

**Expected Behavior:**
- Dropdown updates to selected effect

#### 15. Scene Buttons
**Widgets:** Button × 3  
**Topics:** `demo/rgb/scene`, `demo/rgb/reset`  
**Payloads:** "SAVE", "LOAD", "RESET"  
**Test:**
- Click buttons on screen
- Monitor MQTT topics

**Expected Behavior:**
- Save Scene: Publishes "SAVE" to `demo/rgb/scene`
- Load Scene: Publishes "LOAD" to `demo/rgb/scene`
- Reset: Publishes "RESET" to `demo/rgb/reset`

---

## Automated Testing Script

Create a test script to cycle through all widgets:

```bash
#!/bin/bash
BROKER="192.168.1.100"  # Change to your broker IP

echo "Testing all widgets..."

# Test switches
echo "Testing switches..."
mosquitto_pub -h $BROKER -t demo/power -m "ON"
sleep 1
mosquitto_pub -h $BROKER -t demo/power -m "OFF"

# Test sliders
echo "Testing sliders..."
for i in {0..100..10}; do
    mosquitto_pub -h $BROKER -t demo/volume -m "$i"
    mosquitto_pub -h $BROKER -t demo/brightness -m "$i"
    sleep 0.2
done

# Test dropdown
echo "Testing dropdown..."
for mode in "Auto" "Manual" "Schedule" "Off"; do
    mosquitto_pub -h $BROKER -t demo/mode -m "$mode"
    sleep 1
done

# Test checkboxes
echo "Testing checkboxes..."
mosquitto_pub -h $BROKER -t demo/notifications -m "true"
mosquitto_pub -h $BROKER -t demo/auto_update -m "false"
sleep 1
mosquitto_pub -h $BROKER -t demo/notifications -m "false"
mosquitto_pub -h $BROKER -t demo/auto_update -m "true"

# Test bars
echo "Testing progress bars..."
for i in {0..100..5}; do
    mosquitto_pub -h $BROKER -t demo/cpu -m "$i"
    mosquitto_pub -h $BROKER -t demo/memory -m "$((100-i))"
    mosquitto_pub -h $BROKER -t demo/disk -m "$((i/2))"
    sleep 0.1
done

# Test labels
echo "Testing labels..."
mosquitto_pub -h $BROKER -t demo/temperature -m "25.3"
mosquitto_pub -h $BROKER -t demo/humidity -m "68"
mosquitto_pub -h $BROKER -t demo/uptime -m "1 day, 2:30:45"

# Test LEDs
echo "Testing LEDs..."
mosquitto_pub -h $BROKER -t demo/led1 -m "255"
mosquitto_pub -h $BROKER -t demo/led2 -m "128"
mosquitto_pub -h $BROKER -t demo/led3 -m "64"
sleep 2
mosquitto_pub -h $BROKER -t demo/led1 -m "0"
mosquitto_pub -h $BROKER -t demo/led2 -m "0"
mosquitto_pub -h $BROKER -t demo/led3 -m "0"

# Test spinner
echo "Testing spinner..."
mosquitto_pub -h $BROKER -t demo/loading -m "show"
sleep 3
mosquitto_pub -h $BROKER -t demo/loading -m "hide"

# Test RGB
echo "Testing RGB controls..."
mosquitto_pub -h $BROKER -t demo/rgb/power -m "ON"
mosquitto_pub -h $BROKER -t demo/rgb/red -m "255"
mosquitto_pub -h $BROKER -t demo/rgb/green -m "0"
mosquitto_pub -h $BROKER -t demo/rgb/blue -m "0"
sleep 1
mosquitto_pub -h $BROKER -t demo/rgb/red -m "0"
mosquitto_pub -h $BROKER -t demo/rgb/green -m "255"
sleep 1
mosquitto_pub -h $BROKER -t demo/rgb/green -m "0"
mosquitto_pub -h $BROKER -t demo/rgb/blue -m "255"

echo "Testing complete!"
```

## Monitoring All Topics

Subscribe to all demo topics:

```bash
mosquitto_sub -h <BROKER_IP> -t "demo/#" -v
```

This will show all published messages from the widgets.

## Troubleshooting

### Widgets Not Appearing
- Check ESP32 serial output for parsing errors
- Verify JSON is valid using a JSON validator
- Check MQTT buffer size (should be 8192 bytes)

### Widgets Not Responding to MQTT
- Verify MQTT broker connection
- Check topic names match exactly (case-sensitive)
- Ensure retained messages are cleared if needed: `mosquitto_pub -h <BROKER_IP> -t demo/power -r -n`

### Values Not Updating
- Check serial logs for "Updated [widget] from MQTT" messages
- Verify payload format matches expected type
- For sliders/arcs, ensure values are within min/max range

### Feedback Loops
- Should not occur - feedback loop prevention is implemented
- If widgets republish their own messages, check logs for "m_updating_from_mqtt" flag

## Expected Serial Output

When configuration loads:
```
I (12345) ConfigManager: Parsing JSON configuration...
I (12346) ConfigManager: Found 41 widgets in configuration
I (12347) ContainerWidget: Created container widget: main_container at (0,0) size (1024x600)
I (12348) ContainerWidget: Created container widget: left_panel at (10,10) size (330x580)
I (12349) LabelWidget: Created label widget: left_title at (5,5) size (310x30)
I (12350) SwitchWidget: Created switch widget: master_power at (10,45)
...
I (12450) ConfigManager: Configuration applied successfully, 41 widgets created
I (12451) ConfigManager: Bringing UI icons to foreground
```

When MQTT messages received:
```
I (15000) SliderWidget: Updated slider volume to value 75 from MQTT
I (15100) SwitchWidget: Updated switch master_power to state ON from MQTT
I (15200) LEDWidget: Updated LED led1 to brightness 255 from MQTT
```

## Widget Count

Total widgets in complete_demo.json: **41**
- Containers: 4
- Labels: 12
- Switches: 2
- Sliders: 5
- Buttons: 5
- Arc: 1
- Dropdowns: 2
- Checkboxes: 2
- Bars: 3
- LEDs: 3
- Spinner: 1
