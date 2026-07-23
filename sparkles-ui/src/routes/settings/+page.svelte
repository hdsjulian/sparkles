<script>
  import { onMount, onDestroy } from 'svelte';
  import { animating } from '$lib/stores.js';
  import {
    getSystemInfo,
    setTime,
    setSleepTime,
    setWakeupTime,
    sleepUntil,
    sleepUntilCancel,
    sleepNow,
    setManualSleepMode,
    toggleLogging,
    toggleTestMode,
    commandOTAUpdate,
    reannounce,
    resetSystem,
    resetClients,
    factoryReset,
    commandAnimate,
    commandAnimationOff,
    getAppSettings,
    setAppSettings,
    commandTestSleepCycle
  } from '$lib/api.js';

  let systemInfo = null;
  let testMode = false;
  let testModeSpacing = 1.0;
  let error = '';
  let successMsg = '';
  let pollInterval;
  let resyncMode = 'fast'; // "fast" | "slow" | "off"

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

  // One-shot "sleep now until HH:MM" — for the pack-up/power-cycle workflow
  let sleepUntilHours = '22';
  let sleepUntilMinutes = '00';

  async function loadSystemInfo() {
    try {
      systemInfo = await getSystemInfo();
      if (systemInfo?.testMode !== undefined) testMode = systemInfo.testMode;
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

  async function loadAppSettings() {
    try {
      const s = await getAppSettings();
      if (s?.resync_mode) resyncMode = s.resync_mode;
    } catch (_) {}
  }

  async function handleResyncModeChange(mode) {
    resyncMode = mode;
    try {
      await setAppSettings({ resync_mode: mode });
      successMsg = `Resync mode: ${mode}`;
      setTimeout(() => { successMsg = ''; }, 2000);
    } catch (e) {
      error = e.message;
    }
  }

  onMount(() => {
    loadSystemInfo();
    loadAppSettings();
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

  async function handleSleepUntil() {
    error = '';
    successMsg = '';
    if (!confirm(`Sleep now until ${sleepUntilHours}:${sleepUntilMinutes}? This starts immediately and doesn't change the recurring Sleep/Wakeup Time above.`)) return;
    try {
      await sleepUntil(sleepUntilHours, sleepUntilMinutes);
      successMsg = 'Sleeping now — will wake automatically';
      setTimeout(() => { successMsg = ''; }, 2500);
      loadSystemInfo();
    } catch (e) {
      error = e.message;
    }
  }

  async function handleWakeUpAt() {
    error = '';
    successMsg = '';
    if (!confirm(`Fleet is already asleep — hold them down and wake at ${sleepUntilHours}:${sleepUntilMinutes}? Use this after a master/Pi reboot; it won't resync or disturb sleeping boards.`)) return;
    try {
      await sleepUntil(sleepUntilHours, sleepUntilMinutes, 0, true);  // skipResync
      successMsg = 'Holding fleet asleep — will wake automatically at the set time';
      setTimeout(() => { successMsg = ''; }, 2500);
      loadSystemInfo();
    } catch (e) {
      error = e.message;
    }
  }

  async function handleToggleManualMode() {
    error = '';
    successMsg = '';
    const turningOn = !systemInfo?.manualMode;
    if (turningOn && !confirm('Switch to fully manual sleep? The daily schedule is ignored — the fleet only sleeps/wakes when you press Sleep Now / Wake Now. This also wakes everyone now.')) return;
    try {
      await setManualSleepMode(turningOn);
      successMsg = turningOn ? 'Manual mode ON — schedule off, waking now' : 'Manual mode OFF — daily schedule resumes';
      setTimeout(() => { successMsg = ''; }, 3000);
      loadSystemInfo();
    } catch (e) {
      error = e.message;
    }
  }

  async function handleSleepNow() {
    error = '';
    successMsg = '';
    if (!confirm('Put ALL devices to sleep right now and hold them until you Wake Up Now? No resync, no wake time — the definitive "everyone down".')) return;
    try {
      await sleepNow();
      successMsg = 'Sleeping all devices now — hold until Wake Up Now';
      setTimeout(() => { successMsg = ''; }, 2500);
      loadSystemInfo();
    } catch (e) {
      error = e.message;
    }
  }

  async function handleSleepUntilCancel() {
    error = '';
    try {
      await sleepUntilCancel();
      successMsg = 'Cancelling — clients wake up within one nap cycle';
      setTimeout(() => { successMsg = ''; }, 2500);
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
      const r = await toggleTestMode(testModeSpacing);
      if (r?.testMode !== undefined) testMode = r.testMode;
      successMsg = `Test mode ${testMode ? 'ON' : 'OFF'}`;
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
    if (!confirm('Reset system? This restarts the master AND all clients, and wipes the saved board list (positions get reassigned as boards re-announce).')) return;
    error = '';
    try {
      await resetSystem();
      successMsg = 'System reset sent';
    } catch (e) {
      error = e.message;
    }
  }

  async function handleResetClients() {
    if (!confirm('Reboot all clients? The master and its board list stay put — boards reboot, re-announce, and re-sync one at a time. Use this when boards get stuck.')) return;
    error = '';
    try {
      await resetClients();
      successMsg = 'Reboot-all-clients sent — boards return over the next ~30s';
      setTimeout(() => { successMsg = ''; }, 4000);
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

  // Sleep cycle test
  let sleepTestRunning = false;
  let sleepTestLog = [];
  let sleepTestSleepS = 15;
  let sleepTestPhaseS = 60;

  const _SLEEP_TEST_LABELS = {
    sleep_test_start:              (d) => `Starting — ${d.clients} clients, sleep ${d.sleep_duration_s}s, phase ${d.phase_duration_s}s`,
    sleep_test_resync_start:       ()  => 'Resync started',
    sleep_test_resync_done:        (d) => `Resync done in ${(d.elapsed_ms/1000).toFixed(1)}s`,
    sleep_test_broadcast_start:    (d) => `Broadcasting sleep — ${d.cycles_expected} cycles expected over ${d.phase_duration_s}s`,
    sleep_test_cycle_heartbeat:    (d) => `Cycle ${d.cycle} — ${d.broadcasts} broadcasts, ${d.elapsed_s}s elapsed — all quiet ✓`,
    sleep_test_unexpected_wakeup:  (d) => `⚠ Client ${d.id} woke mid-phase at ${(d.elapsed_ms/1000).toFixed(1)}s and did not go back to sleep`,
    sleep_test_broadcast_end:      (d) => `Sleep phase ended — ${d.broadcasts} broadcasts over ${(d.elapsed_ms/1000).toFixed(1)}s`,
    sleep_test_waiting_for_wakeup: ()  => 'Phase over — waiting for clients to wake up and re-announce…',
    sleep_test_client_back:        (d) => `Client back: ${d.returned}/${d.expected} (${(d.elapsed_ms/1000).toFixed(1)}s)`,
    sleep_test_done:               (d) => `Done — ${d.returned}/${d.expected} returned${d.missing_ids?.length ? `, missing: [${d.missing_ids.join(', ')}]` : ''} — ${d.success ? '✓ OK' : '✗ INCOMPLETE'}`,
    sleep_test_timeout:            ()  => 'Test timed out',
  };

  function handleRunSleepTest() {
    if (sleepTestRunning) return;
    sleepTestRunning = true;
    sleepTestLog = [];

    const es = commandTestSleepCycle(sleepTestSleepS, sleepTestPhaseS);

    const allEvents = Object.keys(_SLEEP_TEST_LABELS);
    allEvents.forEach(evt => {
      es.addEventListener(evt, (e) => {
        const d = JSON.parse(e.data);
        const label = _SLEEP_TEST_LABELS[evt]?.(d) ?? evt;
        const now = new Date().toLocaleTimeString();
        sleepTestLog = [...sleepTestLog, { time: now, event: evt, label, data: d }];
        if (evt === 'sleep_test_done' || evt === 'sleep_test_timeout') {
          sleepTestRunning = false;
          es.close();
        }
      });
    });

    es.onerror = () => {
      sleepTestLog = [...sleepTestLog, { time: new Date().toLocaleTimeString(), event: 'error', label: 'Connection lost', data: {} }];
      sleepTestRunning = false;
      es.close();
    };
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

  <!-- Sleep Mode: schedule vs fully manual -->
  <div class="card" style="margin-bottom:1.25rem;">
    <div class="card-title">Sleep Mode</div>
    <p style="font-size:0.82rem;color:var(--text-secondary);margin-bottom:0.75rem;">
      {#if systemInfo?.manualMode}
        <strong style="color:var(--color-warning,#f9a825);">Manual</strong> — the daily Sleep/Wakeup Time below is ignored.
        The fleet stays awake until you press Sleep Now, and only wakes on Wake Now.
      {:else}
        <strong>Scheduled</strong> — the fleet sleeps/wakes automatically per the Sleep/Wakeup Time below.
      {/if}
    </p>
    <button class="btn {systemInfo?.manualMode ? 'btn-primary' : 'btn-warning'}" on:click={handleToggleManualMode}>
      {systemInfo?.manualMode ? 'Resume Daily Schedule' : 'Switch to Manual (wake & stay awake)'}
    </button>
  </div>

  <!-- Sleep Time -->
  <div class="card" style="margin-bottom:1.25rem;{systemInfo?.manualMode ? 'opacity:0.5;' : ''}">
    <div class="card-title">Sleep Time {#if systemInfo?.manualMode}(ignored — manual mode){/if}</div>
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

  <!-- Sleep Until (one-shot) -->
  <div class="card" style="margin-bottom:1.25rem;">
    <div class="card-title">Sleep Until</div>
    <p style="font-size:0.82rem;color:var(--text-secondary);margin-bottom:0.75rem;">
      Sleep the whole fleet now, until this time today (or tomorrow if already passed).
      Use it when packing away, or after a master/Pi reboot to hold everything down until
      showtime. Survives another power loss (resumes on boot). At the set time the master
      keeps broadcasting wake until <em>every</em> board has actually re-checked-in — not
      just for a fixed window — and reports any that don't come back. Doesn't touch the
      recurring Sleep/Wakeup Time above.
    </p>
    {#if systemInfo?.sleepUntilActive}
      <div class="status-msg" style="margin-bottom:0.75rem;">
        {#if systemInfo.sleepUntilIndefinite}
          😴 All devices sleeping now — held until you wake them
        {:else}
          😴 Sleeping now until {String(systemInfo.sleepUntilHours).padStart(2, '0')}:{String(systemInfo.sleepUntilMinutes).padStart(2, '0')}
        {/if}
      </div>
      <button class="btn btn-ghost" on:click={handleSleepUntilCancel}>Cancel — Wake Up Now</button>
    {:else if systemInfo?.sleepUntilWaking}
      <div class="status-msg" style="margin-bottom:0.75rem;">
        ☀️ Waking up — {systemInfo.wakeReturned ?? 0}/{systemInfo.wakeExpected ?? '?'} boards back.
        The master keeps waking until every board returns (up to 20 min); leave this running.
      </div>
    {:else}
      <div class="form-row" style="margin-bottom:0.75rem;">
        <div class="form-group">
          <label>Hours</label>
          <input type="number" bind:value={sleepUntilHours} min="0" max="23" />
        </div>
        <div class="form-group">
          <label>Minutes</label>
          <input type="number" bind:value={sleepUntilMinutes} min="0" max="59" />
        </div>
      </div>
      <div class="btn-row">
        <button class="btn btn-primary" on:click={handleSleepUntil}>Sleep Now Until This Time</button>
        <button class="btn btn-ghost" on:click={handleWakeUpAt}>Wake Up At This Time</button>
      </div>
      <p style="font-size:0.78rem;color:var(--text-secondary);margin-top:0.5rem;">
        <strong>Sleep Now</strong>: put an awake fleet to sleep until the time.
        <strong>Wake Up At</strong>: fleet is already asleep (after a reboot) — just hold
        them down and wake at the time, no resync.
      </p>
      <div class="btn-row" style="margin-top:0.75rem;">
        <button class="btn btn-warning" on:click={handleSleepNow}>💤 Sleep All Now (hold until woken)</button>
      </div>
      <p style="font-size:0.78rem;color:var(--text-secondary);margin-top:0.5rem;">
        The definitive one: no resync, no time, can't be stalled by a slow sync — every
        device sleeps immediately and stays down until you press Wake Up Now.
      </p>
    {/if}
  </div>

  <!-- Resync Mode -->
  <div class="card" style="margin-bottom:1.25rem;">
    <div class="card-title">Resync Mode</div>
    <p style="font-size:0.82rem;color:var(--text-secondary);margin-bottom:0.75rem;">
      Controls timer sync before blink/strobe animations. Fast: ~20s parallel sync. Slow: full sequential sync. Off: no auto-sync.
    </p>
    <div class="btn-row">
      <button class="btn" class:btn-primary={resyncMode === 'fast'} class:btn-ghost={resyncMode !== 'fast'}
        on:click={() => handleResyncModeChange('fast')}>Fast</button>
      <button class="btn" class:btn-primary={resyncMode === 'slow'} class:btn-ghost={resyncMode !== 'slow'}
        on:click={() => handleResyncModeChange('slow')}>Slow</button>
      <button class="btn" class:btn-warning={resyncMode === 'off'} class:btn-ghost={resyncMode !== 'off'}
        on:click={() => handleResyncModeChange('off')}>Off</button>
    </div>
  </div>

  <!-- Sleep Cycle Test -->
  <div class="card" style="margin-bottom:1.25rem;">
    <div class="card-title">Sleep Cycle Test</div>
    <p style="font-size:0.82rem;color:var(--text-secondary);margin-bottom:0.75rem;">
      Runs a compressed sleep cycle: resync → broadcast sleep → wait for wakeup. Phase must cover at least 2× client sleep duration to verify clients wake mid-phase, receive the rebroadcast, and go back to sleep. Any unexpected mid-phase wakeup is flagged in red.
    </p>
    {#if sleepTestPhaseS < sleepTestSleepS * 2 + 5}
      <p style="font-size:0.8rem;color:#f9a825;margin-bottom:0.5rem;">
        ⚠ Phase will be auto-extended to {sleepTestSleepS * 2 + 5}s to cover at least 2 client sleep cycles.
      </p>
    {/if}
    <div class="form-row" style="margin-bottom:0.75rem;">
      <div class="form-group">
        <label>Client sleep (s)</label>
        <input type="number" min="5" max="300" bind:value={sleepTestSleepS} disabled={sleepTestRunning} />
      </div>
      <div class="form-group">
        <label>Phase duration (s)</label>
        <input type="number" min="10" max="600" bind:value={sleepTestPhaseS} disabled={sleepTestRunning} />
      </div>
      <div class="form-group" style="align-self:flex-end;">
        <button class="btn btn-primary" on:click={handleRunSleepTest} disabled={sleepTestRunning}>
          {sleepTestRunning ? 'Running…' : 'Run Test'}
        </button>
      </div>
    </div>
    {#if sleepTestLog.length > 0}
      <div style="font-family:monospace;font-size:0.8rem;background:var(--bg-secondary,#111);border-radius:6px;padding:0.75rem;max-height:280px;overflow-y:auto;">
        {#each sleepTestLog as entry}
          <div style="margin-bottom:0.25rem;color:{
            (entry.event==='sleep_test_done' && entry.data.success) || entry.event==='sleep_test_cycle_heartbeat' ? '#4caf50' :
            entry.event==='sleep_test_done' || entry.event==='error' || entry.event==='sleep_test_timeout' || entry.event==='sleep_test_unexpected_wakeup' ? '#f44336' :
            'inherit'}">
            <span style="opacity:0.5">{entry.time}</span>
            &nbsp;{entry.label}
          </div>
        {/each}
        {#if sleepTestRunning}
          <div style="opacity:0.4">…</div>
        {/if}
      </div>
    {/if}
  </div>

  <!-- Toggles -->
  <div class="card" style="margin-bottom:1.25rem;">
    <div class="card-title">Toggles</div>
    <div class="btn-row">
      <button class="btn btn-ghost" on:click={handleToggleLogging}>Toggle Logging</button>
      <label style="display:flex;align-items:center;gap:0.4rem;font-size:0.85rem;">
        <span>m/client</span>
        <input type="number" min="0.1" max="100" step="0.1" bind:value={testModeSpacing}
          style="width:5rem;" disabled={testMode} />
      </label>
      <button class="btn" class:btn-warning={testMode} class:btn-ghost={!testMode} on:click={handleToggleTestMode}>
        Test Mode: {testMode ? 'ON' : 'OFF'}
      </button>
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
      <button class="btn btn-ghost" on:click={handleResetClients}>Reboot All Clients</button>
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
