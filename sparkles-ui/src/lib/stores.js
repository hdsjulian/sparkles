import { writable } from 'svelte/store';

// Map<boardId, deviceObject>
export const devices = writable(new Map());

// Whether animation is currently running
export const animating = writable(false);

// Number of active devices reported by SSE
export const numDevices = writable(0);

// Sync status string from SSE
export const syncStatus = writable('');

// Animate status string from SSE
export const animateStatus = writable('');

// Human-readable name of the animation last broadcast (e.g. "sync_async_blink")
export const currentAnimationName = writable('');

// Calibration status object from SSE { status, boardId, x, y }
export const calibrationStatus = writable(null);

// Distance calibration status from SSE
export const distanceStatus = writable(null);

// Chirp position calibration status from SSE { status, slot, x, y, chirp, boards }
export const positionStatus = writable(null);

// Last clientClap event
export const clientClap = writable(null);

// Error from a remove_device/remove_all_devices rejection (e.g. sync in progress)
export const deviceListError = writable('');

// Map<boardId, healthObject> — last client_health reply per board, whether it
// came from a full health check or a single manual ping
export const deviceHealth = writable(new Map());

// State of the running (or last) health check. pending holds the boards still
// being retried, missing the ones that never answered.
export const healthCheck = writable({
  running: false,
  total: 0,
  pinged: 0,
  responded: 0,
  noResponse: 0,
  pass: 0,
  passes: 0,
  current: null,
  missing: [],
  done: false,
  error: ''
});

// Last brain_status frame from the Muse test rig
// { settle, value, phase, session, contact, bpm, battery }
export const brainStatus = writable(null);
