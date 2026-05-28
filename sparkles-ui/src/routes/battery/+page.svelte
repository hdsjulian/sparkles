<script>
  import { onMount } from 'svelte';
  import { devices } from '$lib/stores.js';
  import { getAddressList, commandBlink, commandSync, setMaintenanceMode, commandShimmer } from '$lib/api.js';

  let error = '';
  let blinkMsg = '';
  let maintenanceMode = false;

  onMount(async () => {
    try {
      const resp = await getAddressList();
      const list = Array.isArray(resp) ? resp : (resp.addresses ?? []);
      devices.update(map => {
        const next = new Map(map);
        list.forEach(d => {
          const key = d.boardId ?? d.id;
          next.set(key, { ...d, boardId: key });
        });
        return next;
      });
    } catch (e) {
      error = `Failed to load devices: ${e.message}`;
    }
  });

  function batteryRowClass(pct) {
    if (pct >= 60) return 'row-ok';
    if (pct >= 25) return 'row-mid';
    return 'row-low';
  }

  function batteryBarClass(pct) {
    if (pct >= 60) return 'ok';
    if (pct >= 25) return 'mid';
    return 'low';
  }

  function formatMac(addr) {
    if (!Array.isArray(addr)) return '—';
    return addr.map(b => b.toString(16).padStart(2, '0').toUpperCase()).join(':');
  }

  async function handleBlink(boardId) {
    blinkMsg = '';
    error = '';
    try {
      await commandBlink(boardId);
      blinkMsg = `Blink sent to #${boardId}`;
      setTimeout(() => { blinkMsg = ''; }, 2000);
    } catch (e) {
      error = `Blink failed: ${e.message}`;
    }
  }

  async function handleSync(boardId) {
    blinkMsg = '';
    error = '';
    try {
      await commandSync(boardId);
      blinkMsg = `Sync sent to #${boardId}`;
      setTimeout(() => { blinkMsg = ''; }, 2000);
    } catch (e) {
      error = `Sync failed: ${e.message}`;
    }
  }

  async function toggleMaintenanceMode() {
    const next = !maintenanceMode;
    try {
      await setMaintenanceMode(next);
      maintenanceMode = next;
      blinkMsg = next ? 'Maintenance mode ON' : 'Maintenance mode OFF';
      setTimeout(() => { blinkMsg = ''; }, 2000);
    } catch (e) {
      error = `Failed: ${e.message}`;
    }
  }

  async function handleShimmer(boardId) {
    try {
      await commandShimmer(boardId);
      blinkMsg = boardId === -1 ? 'Shimmer sent to all' : `Shimmer sent to #${boardId}`;
      setTimeout(() => { blinkMsg = ''; }, 2000);
    } catch (e) {
      error = `Shimmer failed: ${e.message}`;
    }
  }

  $: sortedDevices = Array.from($devices.values()).sort(
    (a, b) => (a.batteryPercentage ?? 0) - (b.batteryPercentage ?? 0)
  );
</script>

<div class="page-content">
  <div class="page-header">
    <h1 class="page-title">Battery Status</h1>
    <button
      class="btn btn-sm"
      class:btn-active={maintenanceMode}
      class:btn-ghost={!maintenanceMode}
      on:click={toggleMaintenanceMode}
    >
      {maintenanceMode ? '🔧 Maintenance ON' : 'Maintenance Mode'}
    </button>
  </div>

  {#if error}
    <div class="status-msg error">{error}</div>
  {/if}
  {#if blinkMsg}
    <div class="status-msg success">{blinkMsg}</div>
  {/if}

  <div class="card">
    {#if sortedDevices.length === 0}
      <div style="text-align:center; color:var(--color-text-muted); padding:2rem;">
        No devices found. Waiting for SSE events...
      </div>
    {:else}
      <table class="battery-table">
        <thead>
          <tr>
            <th>ID</th>
            <th>MAC Address</th>
            <th>Battery</th>
            <th>Bar</th>
            <th>Status</th>
            <th>Delay</th>
            <th>Action</th>
          </tr>
        </thead>
        <tbody>
          {#each sortedDevices as device (device.boardId)}
            {@const pct = device.batteryPercentage ?? 0}
            <tr class={batteryRowClass(pct)}>
              <td>#{device.boardId}</td>
              <td style="font-family:monospace; font-size:0.8rem;">{formatMac(device.address)}</td>
              <td><strong>{pct.toFixed(1)}%</strong></td>
              <td>
                <div class="battery-bar-wrap">
                  <div
                    class="battery-bar {batteryBarClass(pct)}"
                    style="width: {Math.min(100, pct)}%"
                  ></div>
                </div>
              </td>
              <td>
                <span class="badge" class:badge-active={device.active} class:badge-inactive={!device.active}>
                  {device.active ? 'Active' : 'Inactive'}
                </span>
              </td>
              <td>{device.delay ?? '—'} ms</td>
              <td style="display:flex; gap:0.4rem; flex-wrap:wrap;">
                <button class="btn btn-ghost btn-sm" on:click={() => handleBlink(device.boardId)}>Blink</button>
                <button class="btn btn-ghost btn-sm" on:click={() => handleSync(device.boardId)}>Sync</button>
                {#if maintenanceMode}
                  <button class="btn btn-ghost btn-sm btn-shimmer" on:click={() => handleShimmer(device.boardId)}>Shimmer</button>
                {/if}
              </td>
            </tr>
          {/each}
        </tbody>
      </table>
    {/if}
  </div>

  <div class="legend">
    <span class="legend-item ok">≥ 60% Good</span>
    <span class="legend-item mid">25–59% Medium</span>
    <span class="legend-item low">&lt; 25% Low</span>
  </div>
</div>

<style>
  .page-header {
    display: flex;
    align-items: center;
    justify-content: space-between;
    margin-bottom: 1rem;
  }
  .page-header .page-title {
    margin-bottom: 0;
  }
  .btn-active {
    background: rgba(255,152,0,0.2);
    border: 1px solid var(--color-mid);
    color: var(--color-mid);
  }
  .btn-shimmer {
    color: #a78bfa;
    border-color: #a78bfa;
  }
  .legend {
    display: flex;
    gap: 1rem;
    margin-top: 1rem;
    flex-wrap: wrap;
  }

  .legend-item {
    font-size: 0.8rem;
    padding: 0.25rem 0.75rem;
    border-radius: 99px;
    font-weight: 600;
  }

  .legend-item.ok {
    background: rgba(76,175,80,0.15);
    color: var(--color-ok);
    border: 1px solid var(--color-ok);
  }

  .legend-item.mid {
    background: rgba(255,152,0,0.15);
    color: var(--color-mid);
    border: 1px solid var(--color-mid);
  }

  .legend-item.low {
    background: rgba(244,67,54,0.15);
    color: var(--color-low);
    border: 1px solid var(--color-low);
  }
</style>
