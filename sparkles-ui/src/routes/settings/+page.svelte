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
  let clockInterval;

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

  function syncClock() {
    const now = new Date();
    clockYear = now.getFullYear();
    clockMonth = now.getMonth() + 1;
    clockDay = now.getDate();
    clockHour = now.getHours();
    clockMinute = now.getMinutes();
    clockSecond = now.getSeconds();
  }

  onMount(() => {
    loadSystemInfo();
    pollInterval = setInterval(loadSystemInfo, 5000);
    syncClock();
    clockInterval = setInterval(syncClock, 1000);
  });

  onDestroy(() => {
    clearInterval(pollInterval);
    clearInterval(clockInterval);
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

  // Compile
  let compileTarget = 'client';
  let incrementVersion = false;
  let compileLog = [];
  let compiling = false;
  let compileSuccess = null;
  let currentVersion = '';
  let compileLogEl;

  onMount(async () => {
    try {
      const r = await fetch('/current-version');
      if (r.ok) currentVersion = (await r.json()).version;
    } catch (_) {}
  });

  async function handleCompile() {
    compileLog = [];
    compileSuccess = null;
    compiling = true;
    const url = `/compile-stream?target=${compileTarget}&incrementVersion=${incrementVersion}`;
    const es = new EventSource(url);
    es.addEventListener('compile_log', (e) => {
      compileLog = [...compileLog, JSON.parse(e.data)];
      if (compileLogEl) setTimeout(() => { compileLogEl.scrollTop = compileLogEl.scrollHeight; }, 0);
    });
    es.addEventListener('compile_done', (e) => {
      const result = JSON.parse(e.data);
      compileSuccess = result.success;
      compiling = false;
      es.close();
      if (result.success && incrementVersion) {
        fetch('/current-version').then(r => r.json()).then(d => { currentVersion = d.version; });
      }
    });
    es.onerror = () => {
      if (compiling) {
        compileLog = [...compileLog, '[SSE connection lost]'];
        compiling = false;
        compileSuccess = false;
        es.close();
      }
    };
  }

  let firmwareFile = null;
  let uploadProgress = '';

  async function handleUploadFirmware() {
    if (!firmwareFile) { error = 'Select a .bin file first'; return; }
    uploadProgress = 'Uploading…';
    error = '';
    try {
      const form = new FormData();
      form.append('file', firmwareFile);
      const res = await fetch('/upload-firmware', { method: 'POST', body: form });
      const json = await res.json();
      if (!res.ok) throw new Error(json.detail ?? 'Upload failed');
      uploadProgress = `Uploaded ${(json.size / 1024).toFixed(1)} KB`;
      firmwareFile = null;
    } catch (e) {
      error = e.message;
      uploadProgress = '';
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

  <!-- Compile & Deploy -->
  <div class="card" style="margin-bottom:1.25rem;">
    <div class="card-title">
      Compile &amp; Deploy
      {#if currentVersion}<span style="font-size:0.78rem; color:var(--color-text-muted); font-weight:400; margin-left:0.5rem;">current: v{currentVersion}</span>{/if}
    </div>

    <div style="display:flex; gap:1.5rem; flex-wrap:wrap; margin-bottom:1rem;">
      <div style="display:flex; flex-direction:column; gap:0.4rem;">
        <span style="font-size:0.78rem; color:var(--color-text-muted); text-transform:uppercase; letter-spacing:0.05em;">Target</span>
        <div style="display:flex; gap:0.5rem;">
          {#each ['client','master','both'] as t}
            <button
              class="btn btn-sm {compileTarget === t ? 'btn-primary' : 'btn-ghost'}"
              on:click={() => { compileTarget = t; if (t === 'client') incrementVersion = false; }}
              disabled={compiling}
            >{t}</button>
          {/each}
        </div>
      </div>

      {#if compileTarget !== 'client'}
        <label style="display:flex; align-items:center; gap:0.5rem; font-size:0.85rem; color:var(--color-text-muted); cursor:pointer; align-self:flex-end;">
          <input type="checkbox" bind:checked={incrementVersion} disabled={compiling} />
          Increment version
        </label>
      {/if}
    </div>

    <button class="btn btn-primary btn-sm" on:click={handleCompile} disabled={compiling}>
      {compiling ? 'Compiling…' : 'Compile'}
    </button>

    {#if compileLog.length > 0}
      <div
        bind:this={compileLogEl}
        style="margin-top:0.75rem; background:#0a0a0a; border:1px solid var(--color-border,#333); border-radius:6px; padding:0.6rem 0.8rem; height:220px; overflow-y:auto; font-family:monospace; font-size:0.72rem; line-height:1.5;"
      >
        {#each compileLog as line}
          <div style="white-space:pre-wrap; word-break:break-all; color:{line.startsWith('[ERROR]') ? '#f44336' : line.startsWith('[') ? '#7c6af7' : '#c8c8c8'};">{line}</div>
        {/each}
        {#if compiling}<div style="color:#7c6af7;">▍</div>{/if}
      </div>
    {/if}

    {#if compileSuccess === true}
      <div class="status-msg success" style="margin-top:0.5rem;">Build successful</div>
    {:else if compileSuccess === false}
      <div class="status-msg error" style="margin-top:0.5rem;">Build failed — see output above</div>
    {/if}
  </div>

  <!-- Firmware Upload -->
  <div class="card" style="margin-bottom:1.25rem;">
    <div class="card-title">Firmware Upload</div>
    <p style="font-size:0.85rem; color:var(--color-text-muted); margin-bottom:0.75rem;">
      Upload a compiled client <code>.bin</code> to the Pi, then click <em>OTA Update</em> to push it to all devices.
    </p>
    <div style="display:flex; gap:0.75rem; align-items:center; flex-wrap:wrap;">
      <input
        type="file"
        accept=".bin"
        on:change={(e) => { firmwareFile = e.target.files[0]; uploadProgress = ''; }}
        style="flex:1; font-size:0.85rem;"
      />
      <button class="btn btn-primary btn-sm" on:click={handleUploadFirmware} disabled={!firmwareFile}>
        Upload
      </button>
    </div>
    {#if uploadProgress}
      <div class="status-msg success" style="margin-top:0.5rem;">{uploadProgress}</div>
    {/if}
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
