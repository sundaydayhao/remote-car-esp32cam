# Remote Car ESP32CAM

This repository contains three independent front ends for the same car firmware:

- `car_controller/`: ESP32-CAM car firmware, creates the Wi-Fi AP and serves the control API.
- `remote_controller/`: ESP32 handheld remote controller.
- `android_app/`: Android phone app.
- `web_app/`: standalone web UI source.

Each folder is meant to stay independent. Do not mix source files across them.

## Quick Start

Car firmware:

```powershell
cd car_controller
idf.py build
```

ESP32 remote controller:

```powershell
cd remote_controller
idf.py build
```

Android app:

- Open `android_app/` in Android Studio.

Web UI:

- Serve `web_app/` from the same origin as the car controller, or place it behind a reverse proxy.

## Car AP

Default AP settings:

```text
SSID: ESP32CAM-Car
Password: 12345678
Base URL: http://192.168.4.1
```

## HTTP API

The car firmware exposes:

- `GET /status`
- `GET /control`
- `GET /gimbal`
- `GET /heartbeat`
- `GET /stream`
- `GET /jpg`
- `WebSocket /audio`

## Front Ends

### Android app

The Android app uses the car HTTP API directly. It includes:

- base URL configuration
- drive controls
- speed slider
- gimbal controls
- status polling
- snapshot preview

See [android_app/README.md](android_app/README.md).

### Web UI

The web UI is a standalone source folder for a browser-based controller. It includes:

- drive controls
- speed control
- gimbal controls
- live stream preview
- status polling
- snapshot fallback

See [web_app/README.md](web_app/README.md).

## Notes

- The car currently runs on Wi-Fi only.
- The web UI must be served from the same origin as the car, or through a proxy, because the firmware does not provide CORS headers.
- `/audio` is reserved for future intercom support.
