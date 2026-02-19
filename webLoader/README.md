# ESP32-P4 Web Loader

This folder contains a simple Web Serial flasher that runs in Chrome/Edge and can flash
one of the local `.bin` files in this directory.

## Quick start

1. Serve this folder with a local web server:

   ```bash
   cd /home/shristov/ESP32P4-MQTT-Panel/webLoader
   python3 -m http.server 8000
   ```

2. Open `http://localhost:8000` in Chrome or Edge.
3. Click **Connect**, select the serial port, then **Flash**.

## Notes

- Web Serial requires `https` or `http://localhost`.
- The default flash offset is `0x0` for a single, merged image.
- If you add new `.bin` files, update the `AVAILABLE_BINS` list in `app.js`.
