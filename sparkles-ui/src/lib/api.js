// =============================================
// Sparkles API — all endpoints as named exports
// =============================================

const BASE = '';

// ---- Device list ----
export const getAddressList = () =>
  fetch(`${BASE}/getAddressList`).then(r => r.json());

export const removeDevice = (index) =>
  fetch(`${BASE}/removeDevice?index=${index}`).then(r => r.json());

export const removeAllDevices = () =>
  fetch(`${BASE}/removeAllDevices`).then(r => r.json());

// ---- Global commands ----
export const commandSyncAll = () =>
  fetch(`${BASE}/commandSyncAll`).then(r => r.json());

// Parallel resync (batches of 5), much faster than Sync All for anything
// beyond a couple of boards — Sync All runs strictly one board at a time,
// 3 full passes, ~2s per board even when it's already responding.
export const commandSyncFast = () =>
  fetch(`${BASE}/commandSyncFast`).then(r => r.json());

export const commandBlinkAll = () =>
  fetch(`${BASE}/commandBlinkAll`).then(r => r.json());

export const commandBatteryBlinkAll = () =>
  fetch(`${BASE}/commandBatteryBlinkAll`).then(r => r.json());

export const commandStrobeAll = (params) => {
  const qs = new URLSearchParams(params).toString();
  return fetch(`${BASE}/commandStrobeAll?${qs}`).then(r => r.json());
};

export const setLogLevel = (level) =>
  fetch(`${BASE}/commandSetLogLevel?level=${level}`).then(r => r.json());

export const commandAnimate = () =>
  fetch(`${BASE}/commandAnimate`).then(r => r.json());

export const commandAnimationOff = () =>
  fetch(`${BASE}/commandAnimationOff`).then(r => r.json());

// ---- Per-device commands ----
export const submitPositions = (boardId, xpos, ypos, zpos) =>
  fetch(`${BASE}/submitPositions?xpos=${xpos}&ypos=${ypos}&zpos=${zpos}&boardId=${boardId}`).then(r => r.json());

export const commandBlink = (boardId) =>
  fetch(`${BASE}/commandBlink?boardId=${boardId}`).then(r => r.json());

export const commandTimerTest = () =>
  fetch(`${BASE}/commandTimerTest`).then(r => r.json());

export const commandMessage = (boardId) =>
  fetch(`${BASE}/commandMessage?boardId=${boardId}`).then(r => r.json());

export const commandSync = (index) =>
  fetch(`${BASE}/commandSync?index=${index}`).then(r => r.json());

export const setMaintenanceMode = (active) =>
  fetch(`${BASE}/setMaintenanceMode?active=${active}`, { method: 'POST' }).then(r => r.json());

export const commandShimmer = (boardId = -1) =>
  fetch(`${BASE}/commandShimmer?boardId=${boardId}`).then(r => r.json());

export const commandBioluminescence = (params) => {
  const qs = new URLSearchParams(params).toString();
  return fetch(`${BASE}/commandBioluminescence?${qs}`).then(r => r.json());
};

export const commandBreath = (params) => {
  const qs = new URLSearchParams(params).toString();
  return fetch(`${BASE}/commandBreath?${qs}`).then(r => r.json());
};

export const commandCandleAll = (params) => {
  const qs = new URLSearchParams(params).toString();
  return fetch(`${BASE}/commandCandleAll?${qs}`).then(r => r.json());
};

// ---- System info & settings ----
export const getSystemInfo = () =>
  fetch(`${BASE}/getSystemInfo`).then(r => r.json());

export const getAnimateStatus = () =>
  fetch(`${BASE}/getAnimateStatus`).then(r => r.json());

export const setTime = (year, month, day, hour, minute, second) =>
  fetch(`${BASE}/setTime?year=${year}&month=${month}&day=${day}&hours=${hour}&minutes=${minute}&seconds=${second}`).then(r => r.json());

export const setSleepTime = (hours, minutes, seconds) =>
  fetch(`${BASE}/setSleepTime?hours=${hours}&minutes=${minutes}&seconds=${seconds}`).then(r => r.json());

export const setWakeupTime = (hours, minutes, seconds) =>
  fetch(`${BASE}/setWakeupTime?hours=${hours}&minutes=${minutes}&seconds=${seconds}`).then(r => r.json());

export const sleepUntil = (hours, minutes, seconds = 0, skipResync = false) =>
  fetch(`${BASE}/sleepUntil?hours=${hours}&minutes=${minutes}&seconds=${seconds}&skipResync=${skipResync}`).then(r => r.json());

export const sleepUntilCancel = () =>
  fetch(`${BASE}/sleepUntilCancel`).then(r => r.json());

export const sleepNow = () =>
  fetch(`${BASE}/sleepNow`).then(r => r.json());

export const wakeNow = () =>
  fetch(`${BASE}/wakeNow`).then(r => r.json());

export const toggleLogging = () =>
  fetch(`${BASE}/toggleLogging`).then(r => r.json());

export const toggleTestMode = (spacing = 1.0) =>
  fetch(`${BASE}/toggleTestMode?spacing=${spacing}`).then(r => r.json());

export const commandOTAUpdate = () =>
  fetch(`${BASE}/commandOTAUpdate`).then(r => r.json());

export const reannounce = () =>
  fetch(`${BASE}/reannounce`).then(r => r.json());

export const resetSystem = () =>
  fetch(`${BASE}/resetSystem`).then(r => r.json());

export const resetClients = () =>
  fetch(`${BASE}/resetClients`).then(r => r.json());

export const factoryReset = () =>
  fetch(`${BASE}/factoryReset`).then(r => r.json());

// ---- Animations ----
export const setSyncAsyncParams = (params) => {
  const qs = new URLSearchParams(params).toString();
  return fetch(`${BASE}/setSyncAsyncParams?${qs}`).then(r => r.json());
};

// ---- MIDI ----
export const getMidiParams = () =>
  fetch(`${BASE}/getMidiParams`).then(r => r.json());

export const setMidiParams = (params) => {
  const qs = new URLSearchParams(params).toString();
  return fetch(`${BASE}/setMidiParams?${qs}`).then(r => r.json());
};

// ---- Sleep test ----
export const runSleepTest = (params) => {
  const qs = new URLSearchParams(params).toString();
  return fetch(`${BASE}/sleepTest?${qs}`).then(r => r.json());
};

export const cancelSleepTest = () =>
  fetch(`${BASE}/sleepTestCancel`).then(r => r.json());

export const getSleepTestReport = () =>
  fetch(`${BASE}/sleepTestReport`).then(r => r.json());

// ---- Lamp colors (persisted on the raspi, stamped into music messages) ----
export const getColors = () =>
  fetch(`${BASE}/colors`).then(r => r.json());

export const setColors = (params) => {
  const qs = new URLSearchParams(params).toString();
  return fetch(`${BASE}/setColors?${qs}`).then(r => r.json());
};

// ---- Darkroom ----
export const getDarkroomParams = () =>
  fetch(`${BASE}/getDarkroomParams`).then(r => r.json());

export const setDarkroomParams = (params) => {
  const qs = new URLSearchParams(params).toString();
  return fetch(`${BASE}/setDarkroomParams?${qs}`).then(r => r.json());
};

// ---- Calibration ----
export const commandStartCalibration = () =>
  fetch(`${BASE}/commandStartCalibration`).then(r => r.json());

export const commandCancelCalibration = () =>
  fetch(`${BASE}/commandCancelCalibration`).then(r => r.json());

export const commandContinueCalibration = (x, y) =>
  fetch(`${BASE}/commandContinueCalibration?x=${x}&y=${y}`).then(r => r.json());

export const commandResetCalibration = () =>
  fetch(`${BASE}/commandResetCalibration`).then(r => r.json());

export const commandEndCalibration = () =>
  fetch(`${BASE}/commandEndCalibration`).then(r => r.json());

export const commandStartDistanceCalibration = () =>
  fetch(`${BASE}/commandStartDistanceCalibration`).then(r => r.json());

export const commandContinueDistanceCalibration = () =>
  fetch(`${BASE}/commandContinueDistanceCalibration`).then(r => r.json());

export const commandAbortDistanceCalibration = () =>
  fetch(`${BASE}/commandAbortDistanceCalibration`).then(r => r.json());

export const commandTestCalibration = () =>
  fetch(`${BASE}/commandTestCalibration`).then(r => r.json());


export const getAppSettings = () =>
  fetch(`${BASE}/appSettings`).then(r => r.json());

export const setAppSettings = (body) =>
  fetch(`${BASE}/appSettings`, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) }).then(r => r.json());

export const commandTestSleepCycle = (sleepDurationS = 15, phaseDurationS = 60) =>
  new EventSource(`/commandTestSleepCycle?sleep_duration_s=${sleepDurationS}&phase_duration_s=${phaseDurationS}`);
