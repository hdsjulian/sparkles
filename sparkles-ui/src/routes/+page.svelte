<script>
  import { onMount, onDestroy } from 'svelte';
  import { devices, animating, numDevices, syncStatus, animateStatus, currentAnimationName, deviceListError, deviceHealth, healthCheck } from '$lib/stores.js';
  import {
    getAddressList,
    getSystemInfo,
    getAnimateStatus,
    commandSyncAll,
    commandSyncFast,
    commandBlinkAll,
    commandAnimate,
    commandAnimationOff,
    removeAllDevices,
    sleepNow,
    wakeNow,
    systemHealthCheck
  } from '$lib/api.js';
  import DeviceCard from '$lib/components/DeviceCard.svelte';

  let error = '';
  let actionMsg = '';
  let masterMac = '—';
  let healthEs = null;

  onMount(async () => {
    try {
      const info = await getSystemInfo();
      masterMac = info.macAddress ?? '—';
    } catch (_) {}

    try {
      const status = await getAnimateStatus();
      animating.set(status.status === true || status.status === 'true');
    } catch (_) {}

    try {
      const data = await getAddressList();
      const list = data.addresses ?? [];
      devices.update(map => {
        const next = new Map(map);
        list.forEach(d => next.set(d.id, d));
        return next;
      });
    } catch (e) {
      error = `Failed to load devices: ${e.message}`;
    }
  });


  // The pi drives the sweep and owns the count: one board asked at a time,
  // three passes, retrying only the boards that stayed quiet. The page just
  // renders what the stream says, so reopening it mid-run loses nothing but
  // the progress line.
  const tally = (d) => healthCheck.update(h => ({
    ...h, pass: d.pass ?? h.pass, pinged: d.pinged ?? h.pinged,
    responded: d.responded, noResponse: d.noResponse
  }));

  const healthHandlers = {
    health_check_start: (d) => healthCheck.set({
      running: true, total: d.total, pinged: 0, responded: 0, noResponse: 0,
      pass: 1, passes: d.passes, current: null, missing: [], done: false, error: ''
    }),
    health_pass_start: (d) => healthCheck.update(h => ({ ...h, pass: d.pass })),
    health_pinging: (d) => healthCheck.update(h => ({
      ...h, current: d.boardId, pass: d.pass, pinged: d.pinged
    })),
    // health_board and health_no_reply carry the same tally, the difference is
    // only which side of it moved
    health_board: (d) => tally(d),
    health_no_reply: (d) => tally(d),
    health_pass_done: (d) => tally(d),
    health_check_done: (d) => {
      healthCheck.update(h => ({
        ...h, running: false, done: true, current: null,
        total: d.total, pinged: d.pinged, responded: d.responded,
        noResponse: d.noResponse, missing: d.missingIds ?? []
      }));
      closeHealthStream();
    },
    health_check_cancelled: (d) => {
      healthCheck.update(h => ({ ...h, running: false, current: null,
                                 responded: d.responded ?? h.responded,
                                 noResponse: d.noResponse ?? h.noResponse }));
      closeHealthStream();
    },
    health_check_error: (d) => {
      healthCheck.update(h => ({ ...h, running: false, current: null, error: d.detail || 'Health check failed' }));
      closeHealthStream();
    }
  };

  function closeHealthStream() {
    if (healthEs) { healthEs.close(); healthEs = null; }
  }

  function handleHealthCheck() {
    error = '';
    if ($healthCheck.running) return;
    closeHealthStream();
    deviceHealth.set(new Map());
    healthCheck.set({ running: true, total: 0, pinged: 0, responded: 0, noResponse: 0,
                      pass: 0, passes: 0, current: null, missing: [], done: false, error: '' });
    healthEs = systemHealthCheck();
    for (const [name, fn] of Object.entries(healthHandlers)) {
      healthEs.addEventListener(name, (e) => {
        try { fn(JSON.parse(e.data)); } catch (err) { console.warn(`health ${name} parse error:`, err); }
      });
    }
    healthEs.onerror = () => {
      // the stream ends by closing itself after health_check_done, which lands
      // here too — only an unfinished run is an actual failure
      if (!$healthCheck.done) {
        healthCheck.update(h => ({ ...h, running: false, current: null, error: 'Lost the health check stream' }));
      }
      closeHealthStream();
    };
  }

  onDestroy(closeHealthStream);

  async function handleSyncAll() {
    error = '';
    try {
      await commandSyncAll();
      actionMsg = 'Sync All sent — sequential, ~2s per board, can take a while for a full fleet';
      setTimeout(() => { actionMsg = ''; }, 4000);
    } catch (e) {
      error = e.message;
    }
  }

  async function handleSyncFast() {
    error = '';
    try {
      await commandSyncFast();
      actionMsg = 'Sync Fast sent — parallel batches of 5, much quicker';
      setTimeout(() => { actionMsg = ''; }, 3000);
    } catch (e) {
      error = e.message;
    }
  }

  async function handleSleepNow() {
    error = '';
    if (!confirm('Put ALL devices to sleep now (no sync) and hold until Wake Now?')) return;
    try {
      await sleepNow();
      actionMsg = 'Sleeping all devices now';
      setTimeout(() => { actionMsg = ''; }, 3000);
    } catch (e) {
      error = e.message;
    }
  }

  async function handleWakeNow() {
    error = '';
    try {
      await wakeNow();
      actionMsg = 'Waking all devices — keeps going until every board returns';
      setTimeout(() => { actionMsg = ''; }, 3000);
    } catch (e) {
      error = e.message;
    }
  }

  async function handleBlinkAll() {
    error = '';
    try {
      await commandBlinkAll();
      actionMsg = 'Blink All sent';
      setTimeout(() => { actionMsg = ''; }, 2000);
    } catch (e) {
      error = e.message;
    }
  }

  async function handleAnimate() {
    error = '';
    try {
      const result = await commandAnimate();
      animating.set(true);
      actionMsg = `Animate: ${result.status ?? 'started'}`;
      setTimeout(() => { actionMsg = ''; }, 3000);
    } catch (e) {
      error = e.message;
    }
  }

  async function handleAnimationOff() {
    error = '';
    try {
      await commandAnimationOff();
      animating.set(false);
      actionMsg = 'Animation stopped';
      setTimeout(() => { actionMsg = ''; }, 2000);
    } catch (e) {
      error = e.message;
    }
  }

  async function handleRemoveAll() {
    error = '';
    if (!confirm(`Remove all ${deviceList.length} devices? Each one re-adds itself the next time it announces (e.g. after a reboot or re-sync).`)) return;
    try {
      await removeAllDevices();
      actionMsg = 'Clearing device list…';
      setTimeout(() => { actionMsg = ''; }, 2000);
    } catch (e) {
      error = e.message;
    }
  }

  $: deviceList = Array.from($devices.values());
</script>

<div class="page-content">
  <h1 class="page-title">Dashboard</h1>

  <!-- Status row -->
  <div class="status-row">
    <div class="card stat-card">
      <div class="card-title">Devices</div>
      <div class="devicenum">{$numDevices || deviceList.length}</div>
    </div>
    <div class="card stat-card">
      <div class="card-title">Sync Status</div>
      <div class="reading" style="font-size:1.1rem;">{$syncStatus || '—'}</div>
    </div>
    <div class="card stat-card">
      <div class="card-title">Animation</div>
      <div class="reading" style="font-size:1.1rem; color: {$animating ? 'var(--color-ok)' : 'var(--color-text-muted)'}">
        {$animating ? ($currentAnimationName || 'Running') : 'Off'}
      </div>
    </div>
    <div class="card stat-card">
      <div class="card-title">Master MAC</div>
      <div class="reading" style="font-size:0.85rem; font-family: monospace;">{masterMac}</div>
    </div>
  </div>

  <!-- Global actions -->
  <div class="card" style="margin-bottom: 1.5rem;">
    <div class="card-title">Global Commands</div>
    {#if error}
      <div class="status-msg error">{error}</div>
    {/if}
    {#if actionMsg}
      <div class="status-msg success">{actionMsg}</div>
    {/if}
    <div class="btn-row">
      <button class="btn btn-primary" on:click={handleSyncFast}>⚡⟳ Sync Fast</button>
      <button class="btn btn-secondary" on:click={handleSyncAll}>⟳ Sync All (slow)</button>
      <button class="btn btn-ghost" on:click={handleBlinkAll}>⚡ Blink All</button>
      <button class="btn btn-primary" on:click={handleAnimate} disabled={$animating}>▶ Animate</button>
      <button class="btn btn-ghost" on:click={handleAnimationOff} disabled={!$animating}>■ Stop Animation</button>
    </div>
    <div class="btn-row" style="margin-top:0.75rem;">
      <button class="btn btn-warning" on:click={handleSleepNow}>💤 Sleep Now</button>
      <button class="btn btn-primary" on:click={handleWakeNow}>☀️ Wake Now</button>
      <button class="btn btn-secondary" on:click={handleHealthCheck} disabled={$healthCheck.running}>
        {$healthCheck.running ? '🩺 Checking…' : '🩺 Check System Health'}
      </button>
    </div>
  </div>

  <!-- Health check -->
  {#if $healthCheck.running || $healthCheck.done || $healthCheck.error}
    <div class="card" style="margin-bottom: 1.5rem;">
      <div class="card-title">System Health</div>

      {#if $healthCheck.error}
        <div class="status-msg error">{$healthCheck.error}</div>
      {/if}

      <div class="health-row">
        <div class="health-stat">
          <div class="health-value">{$healthCheck.pinged}<span class="health-of">/{$healthCheck.total}</span></div>
          <div class="stat-label">Pinged</div>
        </div>
        <div class="health-stat">
          <div class="health-value ok">{$healthCheck.responded}</div>
          <div class="stat-label">Responded</div>
        </div>
        <div class="health-stat">
          <div class="health-value" class:low={$healthCheck.noResponse > 0}>{$healthCheck.noResponse}</div>
          <div class="stat-label">No response</div>
        </div>
        <div class="health-stat">
          <div class="health-value">{$healthCheck.pass || '—'}<span class="health-of">/{$healthCheck.passes || 3}</span></div>
          <div class="stat-label">Pass</div>
        </div>
      </div>

      {#if $healthCheck.total > 0}
        <div class="health-bar">
          <div class="health-bar-fill ok" style="width: {100 * $healthCheck.responded / $healthCheck.total}%"></div>
          <div class="health-bar-fill low" style="width: {100 * $healthCheck.noResponse / $healthCheck.total}%"></div>
        </div>
      {/if}

      {#if $healthCheck.running}
        <div class="health-note">
          {#if $healthCheck.current != null}
            Pinging board #{$healthCheck.current} — pass {$healthCheck.pass} of {$healthCheck.passes}
          {:else}
            Asking the master for its device list…
          {/if}
        </div>
      {:else if $healthCheck.done}
        <div class="health-note" class:warn={$healthCheck.noResponse > 0}>
          {#if $healthCheck.noResponse === 0}
            ✓ All {$healthCheck.responded} boards answered.
          {:else}
            ✗ {$healthCheck.responded}/{$healthCheck.total} answered after {$healthCheck.passes} passes —
            silent: {$healthCheck.missing.map(id => `#${id}`).join(', ')}
          {/if}
        </div>
      {/if}
    </div>
  {/if}

  <!-- Device grid -->
  <div class="devices-header">
    <h2 style="font-size:1rem; color:var(--color-text-muted); text-transform:uppercase; letter-spacing:0.05em;">
      Devices ({deviceList.length})
    </h2>
    {#if deviceList.length > 0}
      <button class="btn btn-ghost btn-sm btn-danger" on:click={handleRemoveAll}>Remove All Devices</button>
    {/if}
  </div>

  {#if $deviceListError}
    <div class="status-msg error" style="margin-bottom:1rem;">{$deviceListError}</div>
  {/if}

  {#if deviceList.length === 0}
    <div class="card" style="text-align:center; color:var(--color-text-muted); padding:2rem;">
      No devices discovered yet. Waiting for SSE events...
    </div>
  {:else}
    <div class="card-grid">
      {#each deviceList as device (device.boardId)}
        <DeviceCard {device} health={$deviceHealth.get(device.boardId)} />
      {/each}
    </div>
  {/if}
</div>

<style>
  .status-row {
    display: flex;
    gap: 1rem;
    margin-bottom: 1.5rem;
    flex-wrap: wrap;
  }

  .stat-card {
    flex: 1;
    min-width: 140px;
  }

  .devices-header {
    display: flex;
    align-items: center;
    justify-content: space-between;
    margin-bottom: 0.75rem;
  }

  .btn-danger {
    color: var(--color-low, #f44336);
    border-color: var(--color-low, #f44336);
  }

  .health-row {
    display: flex;
    gap: 2rem;
    flex-wrap: wrap;
    margin-bottom: 0.75rem;
  }

  .health-value {
    font-size: 1.6rem;
    font-weight: 800;
  }

  .health-value.ok  { color: var(--color-ok); }
  .health-value.low { color: var(--color-low, #f44336); }

  .health-of {
    font-size: 1rem;
    font-weight: 600;
    color: var(--color-text-muted);
  }

  .stat-label {
    font-size: 0.68rem;
    color: var(--color-text-muted);
    text-transform: uppercase;
    letter-spacing: 0.05em;
  }

  .health-bar {
    display: flex;
    height: 6px;
    border-radius: 3px;
    overflow: hidden;
    background: var(--color-border, #333);
    margin-bottom: 0.6rem;
  }

  .health-bar-fill {
    transition: width 0.2s linear;
  }

  .health-bar-fill.ok  { background: var(--color-ok); }
  .health-bar-fill.low { background: var(--color-low, #f44336); }

  .health-note {
    font-size: 0.85rem;
    color: var(--color-text-muted);
  }

  .health-note.warn {
    color: var(--color-mid);
  }
</style>
