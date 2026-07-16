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

// Last clientClap event
export const clientClap = writable(null);

// Error from a remove_device/remove_all_devices rejection (e.g. sync in progress)
export const deviceListError = writable('');
