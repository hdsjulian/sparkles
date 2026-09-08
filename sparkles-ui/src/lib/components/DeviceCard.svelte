<script>
  import { commandBlink, commandSync, submitPositions, removeDevice, commandResetClient, commandHealthPing } from '$lib/api.js';

  export let device;
  // last client_health reply for this board, undefined until it has answered one
  export let health = undefined;

  // the master sends lowercase xpos/ypos, both in the address dump and in the
  // live update_board events — reading xPos left every card showing 0,0
  let xPos = device.xpos ?? 0;
  let yPos = device.ypos ?? 0;
  let zPos = device.zpos ?? 0;
  let error = '';
  let successMsg = '';

  // Reactive update when device prop changes
  $: {
    xPos = device.xpos ?? 0;
    yPos = device.ypos ?? 0;
    zPos = device.zpos ?? 0;
  }

  function formatMac(addr) {
    // the master sends this pre-formatted ("aa:bb:cc:dd:ee:ff"); the array
    // branch is only for any future/other source that sends raw bytes
    if (typeof addr === 'string') return addr.toUpperCase();
    if (Array.isArray(addr)) return addr.map(b => b.toString(16).padStart(2, '0').toUpperCase()).join(':');
    return '—';
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

  async function handleReboot() {
    error = '';
    successMsg = '';
    if (!confirm(`Reboot board #${device.boardId}? It drops off the mesh for a few seconds and re-announces itself.`)) return;
    try {
      await commandResetClient(device.boardId);
      successMsg = 'Rebooting';
      setTimeout(() => { successMsg = ''; }, 2500);
    } catch (e) {
      error = `Reboot failed: ${e.message}`;
    }
  }

  async function handlePing() {
    error = '';
    successMsg = '';
    try {
      await commandHealthPing(device.boardId);
      successMsg = 'Pinged';
      setTimeout(() => { successMsg = ''; }, 2000);
    } catch (e) {
      error = `Ping failed: ${e.message}`;
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

  // esp_reset_reason() values, ESP_RST_* in esp_system.h — only the ones worth
  // naming on a card; a panic or a brownout is the whole story
  const resetReasons = {
    0: 'unknown', 1: 'power-on', 2: 'external', 3: 'software', 4: 'panic',
    5: 'watchdog (int)', 6: 'watchdog (task)', 7: 'watchdog', 8: 'deep sleep',
    9: 'brownout', 10: 'sdio'
  };

  function formatUptime(s) {
    if (s == null) return '—';
    if (s < 60) return `${s}s`;
    if (s < 3600) return `${Math.floor(s / 60)}m`;
    if (s < 86400) return `${Math.floor(s / 3600)}h ${Math.floor((s % 3600) / 60)}m`;
    return `${Math.floor(s / 86400)}d ${Math.floor((s % 86400) / 3600)}h`;
  }

  function formatHeap(bytes) {
    if (bytes == null) return '—';
    return `${(bytes / 1024).toFixed(0)}k`;
  }

  $: bclass = batteryClass(device.batteryPercentage ?? 0);
  // a board that had to be woken by its own watchdog or a panic is worth
  // flagging even when everything else about it reads fine
  $: badReset = health != null && [4, 5, 6, 7, 9].includes(health.resetReason);
  $: heapClass = health == null ? '' : (health.freeHeap < 20000 ? 'low' : health.freeHeap < 40000 ? 'mid' : 'ok');
  // status is the string the master sends, `active` was never in the payload
  $: isActive = device.status === 'active' || device.active === true;
</script>

<div class="card device-card">
  <div class="device-header">
    <div class="device-id">#{device.boardId}</div>
    <span class="badge" class:badge-active={isActive} class:badge-inactive={!isActive}>
      {isActive ? 'Active' : 'Inactive'}
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

  {#if health}
    <div class="health-block">
      <div class="health-line">
        <span class="health-item {heapClass}">{formatHeap(health.freeHeap)} free</span>
        <span class="health-item">min {formatHeap(health.minFreeHeap)}</span>
        <span class="health-item">up {formatUptime(health.uptimeS)}</span>
        {#if health.rssi}<span class="health-item">{health.rssi} dBm</span>{/if}
      </div>
      <div class="health-line health-meta">
        <span>v{health.version || '?'}</span>
        <span class:warn={badReset}>reset: {resetReasons[health.resetReason] ?? health.resetReason}</span>
      </div>
    </div>
  {/if}

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
    <button class="btn btn-ghost btn-sm" on:click={handlePing}>Ping</button>
    <button class="btn btn-primary btn-sm" on:click={handleSubmitPosition}>Save Pos</button>
    <button class="btn btn-ghost btn-sm" on:click={handleReboot}>Reboot</button>
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

  .health-block {
    display: flex;
    flex-direction: column;
    gap: 0.15rem;
    padding: 0.4rem 0.5rem;
    border-radius: 4px;
    background: rgba(255, 255, 255, 0.04);
  }

  .health-line {
    display: flex;
    gap: 0.75rem;
    flex-wrap: wrap;
    font-size: 0.78rem;
  }

  .health-item.ok  { color: var(--color-ok); }
  .health-item.mid { color: var(--color-mid); }
  .health-item.low { color: var(--color-low, #f44336); }

  .health-meta {
    font-size: 0.7rem;
    color: var(--color-text-muted);
  }

  .health-meta .warn {
    color: var(--color-mid);
  }
</style>
