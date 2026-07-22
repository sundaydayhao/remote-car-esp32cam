# Web Car Controller

`web_app/` is a standalone web frontend for the car controller.

It talks to the car over these endpoints:

- `/status`
- `/control`
- `/gimbal`
- `/heartbeat`
- `/stream`
- `/jpg`

## How to use

Serve the folder from the same origin as the car controller, or put the files behind a reverse proxy that forwards requests to the ESP32 car.

Default car address:

```text
http://192.168.4.1
```

## What is included

- Drive pad
- Speed control
- Gimbal pad
- Live stream preview
- Status polling
- Snapshot fallback

## Note

The current car firmware does not expose CORS headers, so a plain `file://` page or a separate host will not work without proxying.
