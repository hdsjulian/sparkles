# 26. Muse headband over BLE into the music path

- Status: Experimental, only on `feature/muse-headband`
- Date: 2026-09-16 (`d86b82f`)

## Context

A new piece: a visitor wears a Muse 2 EEG headband, and the lamps come up as they genuinely settle. It must not disturb the rest of the system, which is in use.

## Decision

- `sparkles-api/muse.py` talks to the Muse 2 over BLE directly with bleak, no LSL, and decodes EEG, PPG and IMU itself.
- `muse_bridge.py` turns the settle score into brightness and sends `aubio_shimmer` frames carrying only a value, so the serial bridge stamps the hue like any other music frame. "nothing in the running system had to change"
- FastAPI starts `muse.py | muse_bridge.py` as a pipeline in its own process group, driven by `/brain/*` and an admin-only Brain page. Commands reach `muse.py` on stdin. Strays left by a service restart are swept on startup.
- Reliability:
  - a keepalive every 5 s beats the headband's ~7.5 s idle timeout
  - every control command waits for its own reply
  - stdout goes through a bounded writer thread, so a slow pipe can't take BLE down
  - a `flock` allows one session at a time
  - a lost BlueZ connect race is retried at once
- It ignores position and distance: "the deliberately dumb version for bringing the thing up on one or two lamps".
- The work stays on its own branch until it's finished.

## Consequences

- Some setup lives only on the Pi: bleak and numpy installed by hand, `ConnectionSupervisionTimeout=500` in `/etc/bluetooth/main.conf`.
- `test_muse.py` covers it with a fake BleakClient.
- The branch also carries the October chirp calibration work (ADRs 20 to 22), which has nothing to do with the Muse.
