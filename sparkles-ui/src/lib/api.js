// =============================================
// Sparkles API — all endpoints as named exports
// =============================================

const BASE = '';

// ---- Device list ----
export const getAddressList = () =>
  fetch(`${BASE}/getAddressList`).then(r => r.json());

// ---- Global commands ----
export const commandSyncAll = () =>
  fetch(`${BASE}/commandSyncAll`).then(r => r.json());

export const commandBlinkAll = () =>
  fetch(`${BASE}/commandBlinkAll`).then(r => r.json());

export const commandBatteryBlinkAll = () =>
  fetch(`${BASE}/commandBatteryBlinkAll`).then(r => r.json());

export const commandStrobeAll = (params) => {
  const qs = new URLSearchParams(params).toString();
  return fetch(`${BASE}/commandStrobeAll?${qs}`).then(r => r.json());
};

export const commandAnimate = () =>
  fetch(`${BASE}/commandAnimate`).then(r => r.json());

export const commandAnimationOff = () =>
  fetch(`${BASE}/commandAnimationOff`).then(r => r.json());

// ---- Per-device commands ----
export const submitPositions = (boardId, xpos, ypos, zpos) =>
  fetch(`${BASE}/submitPositions?xpos=${xpos}&ypos=${ypos}&zpos=${zpos}&boardId=${boardId}`).then(r => r.json());

export const commandBlink = (boardId) =>
  fetch(`${BASE}/commandBlink?boardId=${boardId}`).then(r => r.json());

export const commandMessage = (boardId) =>
  fetch(`${BASE}/commandMessage?boardId=${boardId}`).then(r => r.json());

export const commandSync = (boardId) =>
  fetch(`${BASE}/commandSync?boardId=${boardId}`).then(r => r.json());

// ---- System info & settings ----
export const getSystemInfo = () =>
  fetch(`${BASE}/getSystemInfo`).then(r => r.json());

export const setTime = (year, month, day, hour, minute, second) =>
  fetch(`${BASE}/setTime?year=${year}&month=${month}&day=${day}&hour=${hour}&minute=${minute}&second=${second}`).then(r => r.json());

export const setSleepTime = (hours, minutes, seconds) =>
  fetch(`${BASE}/setSleepTime?hours=${hours}&minutes=${minutes}&seconds=${seconds}`).then(r => r.json());

export const setWakeupTime = (hours, minutes, seconds) =>
  fetch(`${BASE}/setWakeupTime?hours=${hours}&minutes=${minutes}&seconds=${seconds}`).then(r => r.json());

export const toggleLogging = () =>
  fetch(`${BASE}/toggleLogging`).then(r => r.json());

export const toggleTestMode = () =>
  fetch(`${BASE}/toggleTestMode`).then(r => r.json());

export const commandOTAUpdate = () =>
  fetch(`${BASE}/commandOTAUpdate`).then(r => r.json());

export const reannounce = () =>
  fetch(`${BASE}/reannounce`).then(r => r.json());

export const resetSystem = () =>
  fetch(`${BASE}/resetSystem`).then(r => r.json());

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
