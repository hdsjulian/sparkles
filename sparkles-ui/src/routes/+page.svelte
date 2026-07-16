<script>
  import { onMount } from 'svelte';
  import { devices, animating, numDevices, syncStatus, animateStatus, currentAnimationName, deviceListError } from '$lib/stores.js';
  import {
    getAddressList,
    getSystemInfo,
    getAnimateStatus,
    commandSyncAll,
    commandSyncFast,
    commandBlinkAll,
    commandAnimate,
    commandAnimationOff,
    removeAllDevices
  } from '$lib/api.js';
  import DeviceCard from '$lib/components/DeviceCard.svelte';

  let error = '';
  let actionMsg = '';
  let masterMac = '—';

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
  </div>

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
        <DeviceCard {device} />
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
</style>
