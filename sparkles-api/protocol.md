# Sparkles Serial Protocol

Bidirectional JSON frames over USB CDC serial (`/dev/ttyACM0`, 115200 baud).
Each frame is a single line of JSON terminated by `\n`.

---

## Pi → ESP32 (Commands)

Every command frame has at minimum `{"cmd": "<name>", ...params}`.

| cmd | params | description |
|-----|--------|-------------|
| `animate_toggle` | — | start/stop animation loop |
| `animation_off` | — | stop all animations |
| `blink` | `boardId: int` | blink one board |
| `blink_all` | — | blink all boards |
| `sync` | `index: int` | sync one board |
| `sync_all` | — | sync all boards |
| `submit_positions` | `boardId, xpos, ypos` | set board position |
| `set_time` | `year, month, day, hours, minutes, seconds` | set RTC |
| `set_sleep_time` | `hours, minutes, seconds` | schedule sleep |
| `set_wakeup_time` | `hours, minutes, seconds` | schedule wakeup |
| `set_midi_params` | `minVal, maxVal, minSat, maxSat, midiHue, midiSaturation, rangeMin, rangeMax, minDb, maxDb, mode, distance, distanceSwitch, distanceMode` | MIDI params |
| `set_darkroom_params` | `strobeMin, strobeMax, redlightMin, redlightMax, candlelightMin, candlelightMax, redLightEnabled, candleLightEnabled` | darkroom params |
| `get_address_list` | — | request address list |
| `get_midi_params` | — | request MIDI params |
| `get_darkroom_params` | — | request darkroom params |
| `get_system_info` | — | request system info |
| `calibration_start` | — | start calibration |
| `calibration_cancel` | — | cancel calibration |
| `calibration_reset` | — | reset calibration |
| `calibration_continue` | `x: float, y: float` | continue with clap position |
| `calibration_end` | — | end calibration |
| `calibration_test` | — | test calibration |
| `calibration_calibrate` | `boardId: int` | calibrate one board |
| `dist_cal_start` | — | start distance calibration |
| `dist_cal_continue` | — | continue distance calibration |
| `dist_cal_end` | — | end distance calibration |
| `dist_cal_cancel` | — | cancel distance calibration |
| `dist_cal_abort` | — | abort distance calibration |
| `ota_update` | — | start OTA update |
| `toggle_test_mode` | — | toggle test mode |
| `toggle_logging` | — | toggle serial logging |
| `reannounce` | — | broadcast reannounce |
| `reset_system` | — | reset system (wipes client list) |
| `factory_reset` | — | factory reset & reboot |

---

## ESP32 → Pi (Events)

Every event frame has `{"event": "<name>", ...data}`.

| event | data | description |
|-------|------|-------------|
| `address_list` | `numDevices, addresses: [{id,address,status,batteryPercentage,distance,xpos,ypos,lastUpdateTime,timerOffset,delay}]` | full address list |
| `update_board` | same address object | single board updated |
| `animate_status` | `status: bool` | animation loop on/off |
| `calibration_status` | `status: int, [clapId, clapTime]` | calibration progress |
| `distance_status` | `status: int, [clapId, clapTime]` | distance cal progress |
| `client_clap` | `clapId, boardId, clapDistance` | clap received from client |
| `num_devices` | `numDevices: int` | device count changed |
| `sync_status` | `status: str` | sync progress |
| `midi_params` | `minVal, maxVal, ...` | MIDI params response |
| `darkroom_params` | `strobeMin, strobeMax, ...` | darkroom params response |
| `system_info` | `systemTime, sleepSet, sleepIn, sleepAtH, sleepAtM, sleepAtS, sleepDuration` | system info response |
| `test_mode` | `testMode: bool` | test mode toggled |
| `logging` | `logging: bool` | logging toggled |
| `ack` | `cmd: str, ok: bool, [msg: str]` | generic command acknowledgement |
