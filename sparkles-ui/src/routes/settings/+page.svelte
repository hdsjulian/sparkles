<script>
  import { onMount, onDestroy } from 'svelte';
  import { animating } from '$lib/stores.js';
  import {
    getSystemInfo,
    setTime,
    setSleepTime,
    setWakeupTime,
    toggleLogging,
    toggleTestMode,
    commandOTAUpdate,
    reannounce,
    resetSystem,
    factoryReset,
    commandAnimate,
    commandAnimationOff
  } from '$lib/api.js';

  let systemInfo = null;
  let error = '';
  let successMsg = '';
  let pollInterval;

  // Clock form
  let clockYear = new Date().getFullYear();
  let clockMonth = new Date().getMonth() + 1;
  let clockDay = new Date().getDate();
  let clockHour = new Date().getHours();
  let clockMinute = new Date().getMinutes();
  let clockSecond = new Date().getSeconds();

  // Sleep / wakeup time forms (HH:MM:SS strings)
  let sleepHours = '22';
  let sleepMinutes = '00';
  let sleepSeconds = '00';

  let wakeHours = '07';
  let wakeMinutes = '00';
  let wakeSeconds = '00';

  async function loadSystemInfo() {
    try {
      systemInfo = await getSystemInfo();
    } catch (e) {
      error = `Failed to load system info: ${e.message}`;
    }
  }

  onMount(() => {
    loadSystemInfo();
    pollInterval = setInterval(loadSystemInfo, 5000);
  });

  onDestroy(() => {
    clearInterval(pollInterval);
  });

  async function handleSetTime() {
    error = '';
    successMsg = '';
    try {
      await setTime(clockYear, clockMonth, clockDay, clockHour, clockMinute, clockSecond);
      successMsg = 'Clock updated';
      setTimeout(() => { successMsg = ''; }, 2000);
    } catch (e) {
      error = e.message;
    }
  }

  async function handleUseNow() {
    const now = new Date();
    clockYear = now.getFullYear();
    clockMonth = now.getMonth() + 1;
    clockDay = now.getDate();
    clockHour = now.getHours();
    clockMinute = now.getMinutes();
    clockSecond = now.getSeconds();
    await handleSetTime();
  }

  async function handleSetSleep() {
    error = '';
    successMsg = '';
    try {
      await setSleepTime(sleepHours, sleepMinutes, sleepSeconds);
      successMsg = 'Sleep time set';
      setTimeout(() => { successMsg = ''; }, 2000);
    } catch (e) {
      error = e.message;
    }
  }

  async function handleSetWakeup() {
    error = '';
    successMsg = '';
    try {
      await setWakeupTime(wakeHours, wakeMinutes, wakeSeconds);
      successMsg = 'Wakeup time set';
      setTimeout(() => { successMsg = ''; }, 2000);
    } catch (e) {
      error = e.message;
    }
  }

  async function handleToggleLogging() {
    error = '';
    try {
      const r = await toggleLogging();
      successMsg = `Logging: ${JSON.stringify(r)}`;
      setTimeout(() => { successMsg = ''; }, 2000);
    } catch (e) {
      error = e.message;
    }
  }

  async function handleToggleTestMode() {
    error = '';
    try {
      const r = await toggleTestMode();
      successMsg = `Test mode: ${JSON.stringify(r)}`;
      setTimeout(() => { successMsg = ''; }, 2000);
    } catch (e) {
      error = e.message;
    }
  }

  async function handleOTA() {
    if (!confirm('Start OTA update? Devices will restart.')) return;
    error = '';
    try {
      await commandOTAUpdate();
      successMsg = 'OTA update started';
    } catch (e) {
      error = e.message;
    }
  }

  async function handleReannounce() {
    error = '';
    try {
      await reannounce();
      successMsg = 'Reannounce sent';
      setTimeout(() => { successMsg = ''; }, 2000);
    } catch (e) {
      error = e.message;
    }
  }

  async function handleReset() {
    if (!confirm('Reset system? This will restart all devices.')) return;
    error = '';
    try {
      await resetSystem();
      successMsg = 'System reset sent';
    } catch (e) {
      error = e.message;
    }
  }

  async function handleFactoryReset() {
    if (!confirm('Factory reset? All configuration will be lost!')) return;
    error = '';
    try {
      await factoryReset();
      successMsg = 'Factory reset sent';
    } catch (e) {
      error = e.message;
    }
  }

  async function handleAnimate() {
    error = '';
    try {
      const r = await commandAnimate();
      animating.set(true);
      successMsg = `Animate: ${r.status ?? 'started'}`;
      setTimeout(() => { successMsg = ''; }, 2000);
    } catch (e) {
      error = e.message;
    }
  }

  async function handleAnimationOff() {
    error = '';
    try {
      await commandAnimationOff();
      animating.set(false);
      successMsg = 'Animation stopped';
      setTimeout(() => { successMsg = ''; }, 2000);
    } catch (e) {
      error = e.message;
    }
  }
</script>

<div class="page-content">
  <h1 class="page-title">Settings</h1>

  {#if error}
    <div class="status-msg error">{error}</div>
  {/if}
  {#if successMsg}
    <div class="status-msg success">{successMsg}</div>
  {/if}

  <!-- System Info -->
  <div class="card" style="margin-bottom:1.25rem;">
    <div class="card-title">System Info</div>
    {#if systemInfo}
      <div class="info-grid">
        <div class="info-item">
          <div class="info-label">System Time</div>
          <div class="info-value">{systemInfo.systemTime ?? '—'}</div>
        </div>
        <div class="info-item">
          <div class="info-label">Num Devices</div>
          <div class="info-value">{systemInfo.numDevices ?? '—'}</div>
        </div>
        <div class="info-item">
          <div class="info-label">Sleep At</div>
          <div class="info-value">{systemInfo.sleepAt ?? '—'}</div>
        </div>
        <div class="info-item">
          <div class="info-label">Sleep In</div>
          <div class="info-value">{systemInfo.sleepIn ?? '—'}</div>
        </div>
        <div class="info-item">
          <div class="info-label">Sleep Duration</div>
          <div class="info-value">{systemInfo.sleepDuration ?? '—'}</div>
        </div>
      </div>
    {:else}
      <div style="color:var(--color-text-muted);">Loading...</div>
    {/if}
  </div>

  <!-- Set Clock -->
  <div class="card" style="margin-bottom:1.25rem;">
    <div class="card-title">Set Clock</div>
    <div class="form-row" style="margin-bottom:0.75rem;">
      <div class="form-group">
        <label>Year</label>
        <input type="number" bind:value={clockYear} min="2020" max="2099" />
      </div>
      <div class="form-group">
        <label>Month</label>
        <input type="number" bind:value={clockMonth} min="1" max="12" />
      </div>
      <div class="form-group">
        <label>Day</label>
        <input type="number" bind:value={clockDay} min="1" max="31" />
      </div>
      <div class="form-group">
        <label>Hour</label>
        <input type="number" bind:value={clockHour} min="0" max="23" />
      </div>
      <div class="form-group">
        <label>Minute</label>
        <input type="number" bind:value={clockMinute} min="0" max="59" />
      </div>
      <div class="form-group">
        <label>Second</label>
        <input type="number" bind:value={clockSecond} min="0" max="59" />
      </div>
    </div>
    <div class="btn-row">
      <button class="btn btn-primary" on:click={handleSetTime}>Set Clock</button>
      <button class="btn btn-ghost" on:click={handleUseNow}>Use Now</button>
    </div>
  </div>

  <!-- Sleep Time -->
  <div class="card" style="margin-bottom:1.25rem;">
    <div class="card-title">Sleep Time</div>
    <div class="form-row" style="margin-bottom:0.75rem;">
      <div class="form-group">
        <label>Hours</label>
        <input type="number" bind:value={sleepHours} min="0" max="23" />
      </div>
      <div class="form-group">
        <label>Minutes</label>
        <input type="number" bind:value={sleepMinutes} min="0" max="59" />
      </div>
      <div class="form-group">
        <label>Seconds</label>
        <input type="number" bind:value={sleepSeconds} min="0" max="59" />
      </div>
    </div>
    <button class="btn btn-primary" on:click={handleSetSleep}>Set Sleep Time</button>
  </div>

  <!-- Wakeup Time -->
  <div class="card" style="margin-bottom:1.25rem;">
    <div class="card-title">Wakeup Time</div>
    <div class="form-row" style="margin-bottom:0.75rem;">
      <div class="form-group">
        <label>Hours</label>
        <input type="number" bind:value={wakeHours} min="0" max="23" />
      </div>
      <div class="form-group">
        <label>Minutes</label>
        <input type="number" bind:value={wakeMinutes} min="0" max="59" />
      </div>
      <div class="form-group">
        <label>Seconds</label>
        <input type="number" bind:value={wakeSeconds} min="0" max="59" />
      </div>
    </div>
    <button class="btn btn-primary" on:click={handleSetWakeup}>Set Wakeup Time</button>
  </div>

  <!-- Toggles -->
  <div class="card" style="margin-bottom:1.25rem;">
    <div class="card-title">Toggles</div>
    <div class="btn-row">
      <button class="btn btn-ghost" on:click={handleToggleLogging}>Toggle Logging</button>
      <button class="btn btn-ghost" on:click={handleToggleTestMode}>Toggle Test Mode</button>
      <button class="btn btn-ghost" on:click={handleReannounce}>Reannounce</button>
    </div>
  </div>

  <!-- Animation -->
  <div class="card" style="margin-bottom:1.25rem;">
    <div class="card-title">Animation</div>
    <div class="btn-row">
      <button class="btn btn-primary" on:click={handleAnimate} disabled={$animating}>▶ Animate</button>
      <button class="btn btn-ghost" on:click={handleAnimationOff} disabled={!$animating}>■ Stop Animation</button>
    </div>
  </div>

  <!-- Danger Zone -->
  <div class="card danger-zone">
    <div class="card-title" style="color:var(--color-low);">Danger Zone</div>
    <div class="btn-row">
      <button class="btn btn-danger" on:click={handleOTA}>OTA Update</button>
      <button class="btn btn-danger" on:click={handleReset}>Reset System</button>
      <button class="btn btn-danger" on:click={handleFactoryReset}>Factory Reset</button>
    </div>
  </div>
</div>

<style>
  .info-grid {
    display: flex;
    flex-wrap: wrap;
    gap: 1rem;
  }

  .info-item {
    min-width: 120px;
  }

  .info-label {
    font-size: 0.72rem;
    color: var(--color-text-muted);
    text-transform: uppercase;
    letter-spacing: 0.05em;
    margin-bottom: 0.2rem;
  }

  .info-value {
    font-size: 1rem;
    font-weight: 600;
    color: var(--color-text);
  }

  .danger-zone {
    border-color: rgba(244, 67, 54, 0.3);
  }
</style>
