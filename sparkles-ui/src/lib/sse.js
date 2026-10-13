import {
  devices,
  animating,
  numDevices,
  syncStatus,
  animateStatus,
  currentAnimationName,
  calibrationStatus,
  distanceStatus,
  positionStatus,
  clientClap,
  deviceListError,
  deviceHealth,
  brainStatus
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

  // Listeners are collected here and attached to each connection, so a new one
  // can be opened when the browser gives up on the old (see connect below).
  const handlers = [];
  const on = (name, fn) => handlers.push([name, fn]);

  const upsertBoard = (board) => {
    const key = board.id ?? board.boardId;
    devices.update(map => {
      const next = new Map(map);
      next.set(key, { ...map.get(key), ...board, boardId: key });
      return next;
    });
  };

  on('new_board', (e) => {
    try { upsertBoard(JSON.parse(e.data)); }
    catch (err) { console.warn('SSE new_board parse error:', err); }
  });

  on('update_board', (e) => {
    try { upsertBoard(JSON.parse(e.data)); }
    catch (err) { console.warn('SSE update_board parse error:', err); }
  });

  // Removal shifts every index after the removed one, so a partial map update
  // would leave stale entries — clear and let the update_board dump that
  // immediately follows (see emitAddressList on the master) repopulate it.
  on('device_removed', () => {
    deviceListError.set('');
    devices.set(new Map());
  });
  on('all_devices_removed', () => {
    deviceListError.set('');
    devices.set(new Map());
    numDevices.set(0);
  });
  on('remove_device_error', (e) => {
    try { deviceListError.set(JSON.parse(e.data).detail || 'Remove failed'); }
    catch { deviceListError.set('Remove failed'); }
  });

  on('address_list', (e) => {
    try {
      const n = JSON.parse(e.data).numDevices;
      if (typeof n === 'number') numDevices.set(n);
    } catch {}
  });

  const onAnimateStatus = (e) => {
    try {
      const data = JSON.parse(e.data);
      const running = data.status === true || data.status === 'true';
      animateStatus.set(running ? 'true' : 'false');
      animating.set(running);
      if (data.animation) currentAnimationName.set(data.animation);
    } catch {
      animateStatus.set(e.data);
      animating.set(e.data !== '' && e.data !== 'off' && e.data !== 'done');
    }
  };
  on('animate_status', onAnimateStatus);
  on('animateStatus',  onAnimateStatus);

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
  on('num_devices', onNumDevices);
  on('numDevices',  onNumDevices);

  // the master's one line every 5 s, in place of separate animate_status and
  // num_devices dumps
  on('status', (e) => {
    try {
      const s = JSON.parse(e.data);
      animateStatus.set(s.animating ? 'true' : 'false');
      animating.set(!!s.animating);
      if (s.animation) currentAnimationName.set(s.animation);
      if (typeof s.boards === 'number') numDevices.set(s.boards);
    } catch (err) { console.warn('SSE status parse error:', err); }
  });

  const onCalibration = (e) => {
    try { calibrationStatus.set(JSON.parse(e.data)); }
    catch { calibrationStatus.set({ status: e.data }); }
  };
  on('calibration_status', onCalibration);
  on('calibrationStatus',  onCalibration);

  const onDistance = (e) => {
    try { distanceStatus.set(JSON.parse(e.data)); }
    catch { distanceStatus.set({ status: e.data }); }
  };
  on('distance_status', onDistance);
  on('distanceStatus',  onDistance);

  const onPosition = (e) => {
    try { positionStatus.set(JSON.parse(e.data)); }
    catch { positionStatus.set({ status: e.data }); }
  };
  on('position_status', onPosition);
  on('positionStatus',  onPosition);

  const onClientClap = (e) => {
    try { clientClap.set(JSON.parse(e.data)); }
    catch { clientClap.set({ raw: e.data }); }
  };
  on('client_clap', onClientClap);
  on('clientClap',  onClientClap);

  // a health reply also arrives here as an update_board, so the card's battery
  // refreshes on its own — this only keeps the extras the board sent with it
  on('client_health', (e) => {
    try {
      const h = JSON.parse(e.data);
      if (h.boardId == null) return;
      deviceHealth.update(map => {
        const next = new Map(map);
        next.set(h.boardId, { ...h, receivedAt: Date.now() });
        return next;
      });
    } catch (err) { console.warn('SSE client_health parse error:', err); }
  });

  on('brain_status', (e) => {
    try { brainStatus.set(JSON.parse(e.data)); }
    catch (err) { console.warn('SSE brain_status parse error:', err); }
  });

  on('sync_status', (e) => { syncStatus.set(e.data); });
  on('syncStatus',  (e) => { syncStatus.set(e.data); });

  // A browser retries a dropped connection by itself, but gives up for good when
  // a retry gets an error status: a 502 while the Pi restarts its service. The
  // page then missed every event, a calibration's "done" included, until it was
  // reloaded.
  let es = null;
  let retry = null;
  const connect = () => {
    es = new EventSource('/events');
    for (const [name, fn] of handlers) es.addEventListener(name, fn);
    es.onerror = () => {
      if (es.readyState !== EventSource.CLOSED) return;   // the browser is retrying
      console.warn('SSE connection closed, reconnecting in 3 s');
      clearTimeout(retry);
      retry = setTimeout(connect, 3000);
    };
  };
  connect();

  return () => {
    clearTimeout(retry);
    es?.close();
  };
}
