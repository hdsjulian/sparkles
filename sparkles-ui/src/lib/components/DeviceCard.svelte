<script>
  import { commandBlink, commandSync, submitPositions, removeDevice } from '$lib/api.js';

  export let device;

  let xPos = device.xPos ?? 0;
  let yPos = device.yPos ?? 0;
  let zPos = device.zPos ?? 0;
  let error = '';
  let successMsg = '';

  // Reactive update when device prop changes
  $: {
    xPos = device.xPos ?? 0;
    yPos = device.yPos ?? 0;
    zPos = device.zPos ?? 0;
  }

  function formatMac(addr) {
    if (!Array.isArray(addr)) return '—';
    return addr.map(b => b.toString(16).padStart(2, '0').toUpperCase()).join(':');
  }

  function batteryClass(pct) {
    if (pct >= 60) return 'ok';
    if (pct >= 25) return 'mid';
    return 'low';
  }

  async function handleBlink() {
    error = '';
    try {
      await commandBlink(device.boardId);
    } catch (e) {
      error = `Blink failed: ${e.message}`;
    }
  }

  async function handleSync() {
    error = '';
    try {
      await commandSync(device.boardId);
    } catch (e) {
      error = `Sync failed: ${e.message}`;
    }
  }

  async function handleSubmitPosition() {
    error = '';
    successMsg = '';
    try {
      await submitPositions(device.boardId, xPos, yPos, zPos);
      successMsg = 'Position saved';
      setTimeout(() => { successMsg = ''; }, 2000);
    } catch (e) {
      error = `Submit failed: ${e.message}`;
    }
  }

  async function handleRemove() {
    error = '';
    if (!confirm(`Remove board #${device.boardId}? Boards after it will shift down one index. It re-adds itself the next time it announces.`)) return;
    try {
      await removeDevice(device.boardId);
      // the board vanishing from the grid is the confirmation; failures
      // (e.g. sync in progress) surface via the dashboard's error banner
    } catch (e) {
      error = `Remove failed: ${e.message}`;
    }
  }

  $: bclass = batteryClass(device.batteryPercentage ?? 0);
</script>

<div class="card device-card">
  <div class="device-header">
    <div class="device-id">#{device.boardId}</div>
    <span class="badge" class:badge-active={device.active} class:badge-inactive={!device.active}>
      {device.active ? 'Active' : 'Inactive'}
    </span>
  </div>

  <div class="device-mac">{formatMac(device.address)}</div>

  <div class="device-stats">
    <div class="stat">
      <div class="stat-value {bclass}">{device.batteryPercentage != null ? device.batteryPercentage.toFixed(1) + '%' : '—'}</div>
      <div class="stat-label">Battery</div>
    </div>
    <div class="stat">
      <div class="stat-value">{device.delay ?? '—'}</div>
      <div class="stat-label">Delay (ms)</div>
    </div>
    <div class="stat">
      <div class="stat-value">{device.timerOffset ?? '—'}</div>
      <div class="stat-label">Offset</div>
    </div>
  </div>

  <div class="position-row">
    <div class="form-group pos-input">
      <label for="x-{device.boardId}">X</label>
      <input id="x-{device.boardId}" type="number" step="0.01" bind:value={xPos} />
    </div>
    <div class="form-group pos-input">
      <label for="y-{device.boardId}">Y</label>
      <input id="y-{device.boardId}" type="number" step="0.01" bind:value={yPos} />
    </div>
    <div class="form-group pos-input">
      <label for="z-{device.boardId}">Z</label>
      <input id="z-{device.boardId}" type="number" step="0.01" bind:value={zPos} />
    </div>
  </div>

  {#if error}
    <div class="status-msg error">{error}</div>
  {/if}
  {#if successMsg}
    <div class="status-msg success">{successMsg}</div>
  {/if}

  <div class="btn-row">
    <button class="btn btn-ghost btn-sm" on:click={handleBlink}>Blink</button>
    <button class="btn btn-ghost btn-sm" on:click={handleSync}>Sync</button>
    <button class="btn btn-primary btn-sm" on:click={handleSubmitPosition}>Save Pos</button>
    <button class="btn btn-ghost btn-sm btn-danger" on:click={handleRemove}>Remove</button>
  </div>
</div>

<style>
  .device-card {
    display: flex;
    flex-direction: column;
    gap: 0.6rem;
  }

  .device-header {
    display: flex;
    align-items: center;
    justify-content: space-between;
  }

  .device-id {
    font-size: 1.3rem;
    font-weight: 800;
    color: var(--color-accent);
  }

  .device-mac {
    font-size: 0.78rem;
    color: var(--color-text-muted);
    font-family: monospace;
    letter-spacing: 0.05em;
  }

  .device-stats {
    display: flex;
    gap: 1rem;
    margin: 0.25rem 0;
  }

  .stat {
    display: flex;
    flex-direction: column;
  }

  .stat-value {
    font-size: 1rem;
    font-weight: 700;
  }

  .stat-value.ok { color: var(--color-ok); }
  .stat-value.mid { color: var(--color-mid); }
  .stat-value.low { color: var(--color-low); }

  .stat-label {
    font-size: 0.68rem;
    color: var(--color-text-muted);
    text-transform: uppercase;
    letter-spacing: 0.05em;
  }

  .position-row {
    display: flex;
    gap: 0.5rem;
  }

  .pos-input {
    flex: 1;
    margin-bottom: 0;
  }

  .pos-input input {
    width: 100%;
  }

  .btn-danger {
    color: var(--color-low, #f44336);
    border-color: var(--color-low, #f44336);
  }
</style>
