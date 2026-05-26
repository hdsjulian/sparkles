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

  const upsertBoard = (board) => {
    const key = board.id ?? board.boardId;
    devices.update(map => {
      const next = new Map(map);
      next.set(key, { ...map.get(key), ...board });
      return next;
    });
  };

  es.addEventListener('new_board', (e) => {
    try { upsertBoard(JSON.parse(e.data)); }
    catch (err) { console.warn('SSE new_board parse error:', err); }
  });

  es.addEventListener('update_board', (e) => {
    try { upsertBoard(JSON.parse(e.data)); }
    catch (err) { console.warn('SSE update_board parse error:', err); }
  });

  const onAnimateStatus = (e) => {
    try {
      const data = JSON.parse(e.data);
      const running = data.status === true || data.status === 'true';
      animateStatus.set(running ? 'true' : 'false');
      animating.set(running);
    } catch {
      animateStatus.set(e.data);
      animating.set(e.data !== '' && e.data !== 'off' && e.data !== 'done');
    }
  };
  es.addEventListener('animate_status', onAnimateStatus);
  es.addEventListener('animateStatus',  onAnimateStatus);

  const onNumDevices = (e) => {
    try {
      const data = JSON.parse(e.data);
      const n = data.numDevices ?? parseInt(e.data, 10);
      if (!isNaN(n)) numDevices.set(n);
    } catch {
      const n = parseInt(e.data, 10);
      if (!isNaN(n)) numDevices.set(n);
    }
  };
  es.addEventListener('num_devices', onNumDevices);
  es.addEventListener('numDevices',  onNumDevices);

  const onCalibration = (e) => {
    try { calibrationStatus.set(JSON.parse(e.data)); }
    catch { calibrationStatus.set({ status: e.data }); }
  };
  es.addEventListener('calibration_status', onCalibration);
  es.addEventListener('calibrationStatus',  onCalibration);

  const onDistance = (e) => {
    try { distanceStatus.set(JSON.parse(e.data)); }
    catch { distanceStatus.set({ status: e.data }); }
  };
  es.addEventListener('distance_status', onDistance);
  es.addEventListener('distanceStatus',  onDistance);

  const onClientClap = (e) => {
    try { clientClap.set(JSON.parse(e.data)); }
    catch { clientClap.set({ raw: e.data }); }
  };
  es.addEventListener('client_clap', onClientClap);
  es.addEventListener('clientClap',  onClientClap);

  es.addEventListener('sync_status', (e) => { syncStatus.set(e.data); });
  es.addEventListener('syncStatus',  (e) => { syncStatus.set(e.data); });

  es.onerror = (err) => {
    console.warn('SSE connection error, will auto-reconnect:', err);
  };

  return () => {
    es.close();
  };
}
