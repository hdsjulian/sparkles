import {
  devices,
  animating,
  numDevices,
  syncStatus,
  animateStatus,
  calibrationStatus,
  distanceStatus,
  clientClap
} from './stores.js';

/**
 * Set up the SSE connection to /events.
 * Updates all relevant stores as events arrive.
 * Returns a cleanup function that closes the EventSource.
 */
export function setupSSE() {
  // SSE not available during SSR
  if (typeof EventSource === 'undefined') {
    return () => {};
  }

  const es = new EventSource('/events');

  es.addEventListener('new_board', (e) => {
    try {
      const board = JSON.parse(e.data);
      devices.update(map => {
        const next = new Map(map);
        next.set(board.boardId, board);
        return next;
      });
    } catch (err) {
      console.warn('SSE new_board parse error:', err);
    }
  });

  es.addEventListener('update_board', (e) => {
    try {
      const board = JSON.parse(e.data);
      devices.update(map => {
        const next = new Map(map);
        if (next.has(board.boardId)) {
          next.set(board.boardId, { ...next.get(board.boardId), ...board });
        } else {
          next.set(board.boardId, board);
        }
        return next;
      });
    } catch (err) {
      console.warn('SSE update_board parse error:', err);
    }
  });

  es.addEventListener('syncStatus', (e) => {
    syncStatus.set(e.data);
  });

  es.addEventListener('animateStatus', (e) => {
    const val = e.data;
    animateStatus.set(val);
    // Treat non-empty animateStatus as "animating"
    animating.set(val !== '' && val !== 'off' && val !== 'done');
  });

  es.addEventListener('numDevices', (e) => {
    const n = parseInt(e.data, 10);
    if (!isNaN(n)) numDevices.set(n);
  });

  es.addEventListener('calibrationStatus', (e) => {
    try {
      const data = JSON.parse(e.data);
      calibrationStatus.set(data);
    } catch (err) {
      // plain string
      calibrationStatus.set({ status: e.data });
    }
  });

  es.addEventListener('distanceStatus', (e) => {
    try {
      const data = JSON.parse(e.data);
      distanceStatus.set(data);
    } catch (err) {
      distanceStatus.set({ status: e.data });
    }
  });

  es.addEventListener('clientClap', (e) => {
    try {
      const data = JSON.parse(e.data);
      clientClap.set(data);
    } catch (err) {
      clientClap.set({ raw: e.data });
    }
  });

  es.onerror = (err) => {
    console.warn('SSE connection error, will auto-reconnect:', err);
  };

  return () => {
    es.close();
  };
}
