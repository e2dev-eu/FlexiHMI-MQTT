# Tabview Demo - Testing Guide

This demo showcases the Tabview widget with three tabs containing different control panels.

## Demo Structure

**Tabs:**
- **Controls**: Master controls with power switch, volume slider, brightness arc, mode dropdown, and checkboxes
- **Status**: System status indicators with CPU/Memory/Disk bars, LEDs, and sensor readings
- **RGB Lighting**: RGB lighting controls with RGBW sliders, effect dropdown, and scene buttons

## Publishing the Configuration

```bash
# Publish the tabview demo configuration
mosquitto_pub -h <BROKER_IP> -t hmi/config -r -f examples/tabview_demo.json
```

## Testing Tab Switching

### Via MQTT

```bash
# Switch to Status tab (by name)
mosquitto_pub -h <BROKER_IP> -t demo/active_tab -m "Status"

# Switch to RGB Lighting tab (by name)
mosquitto_pub -h <BROKER_IP> -t demo/active_tab -m "RGB Lighting"

# Switch to Controls tab (by index)
mosquitto_pub -h <BROKER_IP> -t demo/active_tab -m "0"

# Switch to Status tab (by index)
mosquitto_pub -h <BROKER_IP> -t demo/active_tab -m "1"

# Monitor tab changes (user swipes will publish here)
mosquitto_sub -h <BROKER_IP> -t demo/active_tab -v
```

### Via Touch
- Tap on tab buttons at the top
- Swipe left/right to change tabs
- Observe MQTT messages published when tabs change

## Testing Widgets in Each Tab

### Controls Tab

```bash
# Toggle master power
mosquitto_pub -h <BROKER_IP> -t demo/power -m "ON"
mosquitto_pub -h <BROKER_IP> -t demo/power -m "OFF"

# Set volume
mosquitto_pub -h <BROKER_IP> -t demo/volume -m "75"

# Set brightness (arc widget)
mosquitto_pub -h <BROKER_IP> -t demo/brightness -m "50"

# Change mode
mosquitto_pub -h <BROKER_IP> -t demo/mode -m "Manual"
mosquitto_pub -h <BROKER_IP> -t demo/mode -m "2"  # Schedule

# Toggle notifications checkbox
mosquitto_pub -h <BROKER_IP> -t demo/notifications -m "false"

# Toggle auto update
mosquitto_pub -h <BROKER_IP> -t demo/auto_update -m "true"
```

### Status Tab

```bash
# Update CPU usage
mosquitto_pub -h <BROKER_IP> -t demo/cpu -m "85"

# Update memory usage
mosquitto_pub -h <BROKER_IP> -t demo/memory -m "72"

# Update disk usage
mosquitto_pub -h <BROKER_IP> -t demo/disk -m "55"

# Update temperature
mosquitto_pub -h <BROKER_IP> -t demo/temperature -m "28.5"

# Update humidity
mosquitto_pub -h <BROKER_IP> -t demo/humidity -m "65"

# Control status LEDs
mosquitto_pub -h <BROKER_IP> -t demo/led1 -m "255"  # Network - full brightness
mosquitto_pub -h <BROKER_IP> -t demo/led2 -m "0"    # MQTT - off
mosquitto_pub -h <BROKER_IP> -t demo/led3 -m "128"  # Storage - 50%

# Show/hide loading spinner
mosquitto_pub -h <BROKER_IP> -t demo/loading -m "show"
mosquitto_pub -h <BROKER_IP> -t demo/loading -m "hide"
```

### RGB Lighting Tab

```bash
# Toggle RGB power
mosquitto_pub -h <BROKER_IP> -t demo/rgb/power -m "ON"

# Set RGB values
mosquitto_pub -h <BROKER_IP> -t demo/rgb/red -m "200"
mosquitto_pub -h <BROKER_IP> -t demo/rgb/green -m "100"
mosquitto_pub -h <BROKER_IP> -t demo/rgb/blue -m "50"
mosquitto_pub -h <BROKER_IP> -t demo/rgb/white -m "0"

# Change effect
mosquitto_pub -h <BROKER_IP> -t demo/rgb/effect -m "Rainbow"
mosquitto_pub -h <BROKER_IP> -t demo/rgb/effect -m "3"  # By index
```

## Automated Test Script

```bash
#!/bin/bash
BROKER="192.168.100.200"

echo "Publishing tabview demo config..."
mosquitto_pub -h $BROKER -t hmi/config -r -f examples/tabview_demo.json
sleep 3

echo "Testing tab switching..."
for tab in "Controls" "Status" "RGB Lighting" "0"; do
    echo "  -> Switching to: $tab"
    mosquitto_pub -h $BROKER -t demo/active_tab -m "$tab"
    sleep 2
done

echo "Testing Controls tab widgets..."
mosquitto_pub -h $BROKER -t demo/active_tab -m "0"
sleep 1
mosquitto_pub -h $BROKER -t demo/power -m "ON"
mosquitto_pub -h $BROKER -t demo/volume -m "80"
mosquitto_pub -h $BROKER -t demo/brightness -m "90"
sleep 2

echo "Testing Status tab widgets..."
mosquitto_pub -h $BROKER -t demo/active_tab -m "1"
sleep 1
mosquitto_pub -h $BROKER -t demo/cpu -m "45"
mosquitto_pub -h $BROKER -t demo/memory -m "67"
mosquitto_pub -h $BROKER -t demo/temperature -m "25.3"
mosquitto_pub -h $BROKER -t demo/led1 -m "255"
mosquitto_pub -h $BROKER -t demo/loading -m "show"
sleep 3
mosquitto_pub -h $BROKER -t demo/loading -m "hide"

echo "Testing RGB tab widgets..."
mosquitto_pub -h $BROKER -t demo/active_tab -m "2"
sleep 1
mosquitto_pub -h $BROKER -t demo/rgb/red -m "255"
mosquitto_pub -h $BROKER -t demo/rgb/green -m "0"
mosquitto_pub -h $BROKER -t demo/rgb/blue -m "128"
sleep 2

echo "Demo complete!"
```

## Expected Behavior

1. **Tab Switching**: Smooth animated transitions between tabs
2. **Widget State**: Each tab maintains its widget states
3. **MQTT Feedback**: Tab changes via touch publish to `demo/active_tab`
4. **MQTT Control**: External messages switch tabs without republishing
5. **Child Widgets**: All widgets in each tab function independently
6. **Relative Coordinates**: Widget positions are relative to tab content area

## Troubleshooting

- **Widgets not appearing**: Check that JSON has correct tab names in "children" object
- **Tab switches don't work**: Verify MQTT topic subscription is active
- **Content offset**: Remember child coordinates are relative to tab, not screen
- **MQTT loops**: Tabview has feedback prevention, should not create loops
