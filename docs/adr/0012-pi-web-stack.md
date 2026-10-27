# 12. FastAPI with SSE, and a static SvelteKit app behind nginx

- Status: Accepted
- Date: 2026-05-26 (`d046c47`), nginx 2026-06-09 (`a877099`)

## Context

The Pi needs an HTTP API that wraps the serial protocol (ADR 10), live updates in the browser, and a UI that works on phones and on the kiosk touchscreen, all on a box with no internet in the field.

## Decision

- API: FastAPI on uvicorn (`sparkles-api/main.py`). Mostly a thin wrapper: each endpoint writes one JSON line and waits for the matching event.
- Live updates: Server-Sent Events. `/events` forwards every serial event as a named SSE event, with a keep-alive every 15 s. Long jobs (compile, latency tests, health checks) stream on their own SSE endpoints. The serial reader thread fans out to one asyncio queue per subscriber.
- UI: SvelteKit with `adapter-static`, an SPA with an `index.html` fallback. `update.sh` builds it on the Pi. The build is not committed.
- FastAPI serves the built UI itself through an SPA catch-all (`9b7ebb8`). nginx listens on :80 and proxies everything to uvicorn on :8080, with buffering off and long timeouts for SSE.
- Local daemons call `localhost:8080` directly.

## Consequences

- One process holds the serial port, the auth and the UI: one service to restart.
- SSE is one-way, which matches the protocol. The UI listens for both the new snake_case event names and the camelCase ones from the ESP32 web server.
- Only one browser tab keeps an SSE connection open (`BroadcastChannel`), to stay under the browser's per-host connection limit.
- Dependencies are pinned to exact versions (`5711877`). Offline, the Pi skips `npm install`, so a new UI dependency needs one online update.

## Tried and dropped

- uvicorn binding :80 itself with `CAP_NET_BIND_SERVICE` (`44530db`). Replaced by nginx (`a877099`).
