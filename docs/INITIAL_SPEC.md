# MQTT‑Driven Dynamic HMI for ESP32 (LVGL)

## Overview
This project defines a **dynamic, MQTT‑connected Human–Machine Interface (HMI)** for ESP32‑P4 using **LVGL**.  
The UI is **fully described by JSON** received via MQTT and can be **hot‑reloaded at runtime** without firmware updates.

Only a single UI element is static: a **gear (⚙️) icon** that opens a **local configuration screen** where MQTT parameters and the configuration topic can be edited.

The system is designed for **industrial panels, home automation displays, and embedded dashboards**.

---

## Key Features

- JSON‑driven UI definition
- Hot reload on configuration version change
- Widget‑based C++ architecture
- MQTT input/output binding per widget
- Automatic widget destruction and recreation
- LVGL‑native rendering
- Local fallback configuration
- ESP32‑friendly memory model

---

## High‑Level Architecture

```
+----------------------+
|   MQTT Broker        |
|  (config + data)     |
+----------+-----------+
           |
           v
+----------------------+
|   MQTT Manager       |
|  (C++ wrapper)       |
+----------+-----------+
           |
           v
+----------------------+
|  Config Manager      |
|  (JSON parsing,      |
|   versioning)        |
+----------+-----------+
           |
           v
+----------------------+
| Widget Factory &     |
| Global Registry      |
+----------+-----------+
           |
           v
+----------------------+
|  LVGL Object Tree    |
|  (Dynamic HMI)       |
+----------------------+
```

---

## JSON Configuration

### Top‑Level Structure

```json
{
  "version": 5,
  "screen": {
    "width": 480,
    "height": 320,
    "orientation": "landscape"
  },
  "theme": {
    "primary": "#1e90ff",
    "background": "#101010",
    "font": "roboto_20"
  },
  "widgets": []
}
```

### Widget Definition

```json
{
  "id": "temp_label",
  "type": "label",
  "position": { "x": 10, "y": 20 },
  "size": { "w": 150, "h": 40 },
  "properties": {
    "text": "--.- °C",
    "align": "left"
  },
  "mqtt": {
    "subscribe": "home/room/temp",
    "publish": null,
    "format": "%.1f °C"
  }
}
```

### Input Widget Example (Button)

```json
{
  "id": "heater_btn",
  "type": "button",
  "position": { "x": 200, "y": 50 },
  "size": { "w": 120, "h": 60 },
  "properties": {
    "text": "Heater"
  },
  "mqtt": {
    "publish": "home/heater/set",
    "payload": "toggle"
  }
}
```

---

## Widget System

### Base Widget Class

```cpp
class HMIWidget {
public:
    virtual void create(lv_obj_t* parent) = 0;
    virtual void destroy() = 0;
    virtual void onMqttMessage(const std::string& topic,
                               const std::string& payload) = 0;
    virtual void onConfigUpdate(const JsonObject& cfg) {}
    virtual ~HMIWidget() {}
};
```

### Widget Registry (Factory Pattern)

```cpp
using WidgetFactory = std::function<HMIWidget*()>;

class WidgetRegistry {
public:
    static void registerWidget(const std::string& type, WidgetFactory f);
    static HMIWidget* create(const std::string& type);
};
```

Example registration:

```cpp
REGISTER_WIDGET("label", LabelWidget);
REGISTER_WIDGET("button", ButtonWidget);
```

---

## Widget Lifecycle

### Configuration Reload Flow

1. New JSON arrives on configuration topic
2. Version number is checked
3. If version increased:
   - Unsubscribe all old MQTT topics
   - Destroy all widgets
   - Clear LVGL object tree
   - Recreate widgets from new JSON
   - Subscribe to required topics

Optional future optimization: **diff‑based reload**.

---

## MQTT Manager

### Responsibilities

- Broker connection & reconnection
- Topic subscription management
- Message routing to widgets
- Publishing widget output

### API Proposal

```cpp
class MQTTManager {
public:
    void connect();
    void disconnect();

    void subscribe(const std::string& topic);
    void unsubscribe(const std::string& topic);

    void publish(const std::string& topic,
                 const std::string& payload,
                 int qos = 0,
                 bool retain = false);

    void registerListener(const std::string& topic,
                          HMIWidget* widget);
};
```

Topic routing supports exact matches and wildcards.

---

## Configuration HMI (⚙️ Gear Screen)

### Stored Locally (NVS)

- MQTT broker address
- Port
- Username / password
- Configuration topic
- Device ID
- TLS enable/disable
- Screen brightness
- Debug level

### Special Rules

- This HMI is never destroyed
- Runs on a separate LVGL layer

---

## Persistence Layer

- **NVS** for MQTT connection parameters only
- **UI Configuration received from MQTT broker** (not cached locally)

### Configuration Loading

- Configuration is received from the MQTT broker on the configured topic
- If MQTT unavailable → no UI loaded (only settings screen available)
- If JSON invalid → reject update, keep previous UI

---

## Error Handling & Diagnostics

- MQTT disconnected indicator
- Configuration error popup
- Widget‑level error state
- Memory & watchdog monitoring

---

## ESP32 Performance Considerations

- Single LVGL thread
- MQTT runs in separate task
- Message queue between MQTT and UI
- Avoid heap fragmentation
- Prefer StaticJsonDocument

---

## Security (Recommended)

- MQTT over TLS
- Topic ACLs
- Configuration signature (HMAC / SHA256)
- Device ID validation

---

## Future Extensions

- Layout containers (row / column / grid)
- Multi‑page UI with swipe navigation
- Expression engine for dynamic logic
- Animations
- OTA‑aware UI versions
- Widget plugins
- Developer overlays (FPS, memory, inspector)

---

## What This Enables

This system enables:

- Firmware‑independent UI updates
- MQTT‑native visualization panels
- Highly reusable embedded HMIs

It can evolve into a **general‑purpose embedded HMI framework**.

---

## License

TBD

---

## Status

Specification – Initial Draft

