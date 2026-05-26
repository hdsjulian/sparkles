# Sparkles FastAPI Server

FastAPI replacement for `WebServer.cpp`, running on a Raspberry Pi and communicating
with the ESP32 master over USB serial.

## Setup

```bash
cd sparkles-api
python -m venv .venv && source .venv/bin/activate
pip install -r requirements.txt
```

## Run

```bash
uvicorn main:app --host 0.0.0.0 --port 8080
```

Override the serial port (default `/dev/ttyACM0`):

```bash
SPARKLES_PORT=/dev/ttyUSB0 uvicorn main:app --host 0.0.0.0 --port 8080
```

## Serial protocol

See [protocol.md](protocol.md) for the full bidirectional JSON frame spec.

The server runs in **offline mode** if the serial port is unavailable — all API
routes respond but commands are silently dropped.

## Arduino firmware changes needed

The ESP32 master needs a serial handler that:
1. Reads newline-terminated JSON frames from `Serial`
2. Dispatches them by `"cmd"` field to the corresponding `MessageHandler` method
3. Sends response frames back as `Serial.println(jsonString)`

See `serial_handler_sketch.md` for a reference implementation you can add to
`src/Master-Device/main.cpp` without touching any existing files/libs.
