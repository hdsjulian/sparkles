<script>
  import { onMount, onDestroy } from 'svelte';
  import { calibrationStatus, distanceStatus, positionStatus, clientClap } from '$lib/stores.js';
  import {
    commandStartCalibration,
    commandCancelCalibration,
    commandContinueCalibration,
    commandResetCalibration,
    commandEndCalibration,
    commandStartDistanceCalibration,
    commandContinueDistanceCalibration,
    commandAbortDistanceCalibration,
    commandTestCalibration
  } from '$lib/api.js';

  // ---- Calibration state machine ----
  // States: idle | open | clapHappened | waiting | calibrated
  let calState = 'idle';
  let calBoardId = null;
  let calX = 0;
  let calY = 0;
  let calInputX = 0;
  let calInputY = 0;
  let calError = '';
  let calMsg = '';

  // ---- Distance calibration state machine ----
  // States: idle | running | done
  let distState = 'idle';
  let distError = '';
  let distMsg = '';

  // ---- Test ----
  let testMsg = '';
  let testError = '';

  // Subscribe to SSE calibration events
  const unsubCal = calibrationStatus.subscribe(ev => {
    if (!ev) return;
    const s = ev.status;
    if (s === 'open' || s === 'started') {
      calState = 'open';
      calMsg = 'Waiting for clap...';
    } else if (s === 'clapHappened' || s === 'clap') {
      calState = 'clapHappened';
      calBoardId = ev.boardId ?? calBoardId;
      calX = ev.x ?? 0;
      calY = ev.y ?? 0;
      calMsg = `Clap detected from board #${calBoardId} at (${calX.toFixed(2)}, ${calY.toFixed(2)})`;
    } else if (s === 'waiting') {
      calState = 'waiting';
      calMsg = 'Processing...';
    } else if (s === 'calibrated' || s === 'done') {
      calState = 'calibrated';
      calMsg = 'Calibration complete!';
    } else if (s === 'cancelled' || s === 'reset') {
      calState = 'idle';
      calMsg = '';
    }
  });

  const unsubDist = distanceStatus.subscribe(ev => {
    if (!ev) return;
    const s = ev.status;
    if (s === 'started' || s === 'running') {
      distState = 'running';
      distMsg = ev.chirp
        ? `Chirp ${ev.chirp} of ${ev.total}...`
        : 'Distance calibration running...';
    } else if (s === 'done' || s === 'complete') {
      distState = 'done';
      distMsg = `Distance calibration complete, averaged over ${ev.total ?? 10} chirps.`;
    } else if (s === 'failed') {
      distState = 'idle';
      distMsg = '';
      distError = `Distance calibration failed: ${ev.reason ?? 'no measurements'}`;
    } else if (s === 'aborted' || s === 'cancelled') {
      distState = 'idle';
      distMsg = '';
    }
  });

  const unsubPos = positionStatus.subscribe(ev => {
    if (!ev) return;
    const s = ev.status;
    if (s === 'running') {
      posBusy = true;
      posError = '';
      posMsg = ev.chirp
        ? `Location ${ev.slot + 1}: chirp ${ev.chirp} of ${ev.total}...`
        : `Location ${ev.slot + 1} at (${ev.x?.toFixed(2)}, ${ev.y?.toFixed(2)}): chirping...`;
    } else if (s === 'done') {
      posBusy = false;
      posMsg = `Location ${ev.slot + 1} recorded: ${ev.boards} boards heard ${ev.chirp} chirps.`;
      posLocations = [
        ...posLocations.filter(l => l.slot !== ev.slot),
        { slot: ev.slot, x: ev.x, y: ev.y, boards: ev.boards }
      ].sort((a, b) => a.slot - b.slot);
    } else if (s === 'solved') {
      posBusy = false;
      posMsg = `Trilateration done: ${ev.boards} boards positioned from ${ev.locations} locations, `
             + `worst residual ${(ev.worstResidual ?? 0).toFixed(2)} m.`;
    } else if (s === 'failed') {
      posBusy = false;
      posMsg = '';
      posError = ev.reason ?? 'Chirp burst failed';
    }
  });

  const unsubClap = clientClap.subscribe(ev => {
    if (!ev) return;
    if (calState === 'open') {
      calState = 'clapHappened';
      calMsg = `Clap received from board #${ev.boardId ?? '?'}`;
    }
  });

  onDestroy(() => {
    unsubCal();
    unsubDist();
    unsubPos();
    unsubClap();
  });

  // ---- Calibration actions ----
  async function startCalibration() {
    calError = '';
    calMsg = '';
    try {
      await commandStartCalibration();
      calState = 'open';
      calMsg = 'Waiting for clap...';
    } catch (e) {
      calError = e.message;
    }
  }

  async function cancelCalibration() {
    calError = '';
    try {
      await commandCancelCalibration();
      calState = 'idle';
      calMsg = '';
    } catch (e) {
      calError = e.message;
    }
  }

  async function continueCalibration() {
    calError = '';
    try {
      await commandContinueCalibration(calInputX, calInputY);
      calState = 'waiting';
      calMsg = 'Submitted position, processing...';
    } catch (e) {
      calError = e.message;
    }
  }

  async function resetCalibration() {
    calError = '';
    try {
      await commandResetCalibration();
      calState = 'idle';
      calMsg = '';
    } catch (e) {
      calError = e.message;
    }
  }

  async function endCalibration() {
    calError = '';
    try {
      await commandEndCalibration();
      calState = 'idle';
      calMsg = 'Calibration ended and saved.';
    } catch (e) {
      calError = e.message;
    }
  }

  // ---- Distance calibration actions ----
  async function startDistanceCalibration() {
    distError = '';
    distMsg = '';
    try {
      await commandStartDistanceCalibration();
      distState = 'running';
      distMsg = 'Distance calibration running...';
    } catch (e) {
      distError = e.message;
    }
  }

  // another full burst, replaces the previous measurements
  async function rerunDistanceCalibration() {
    distError = '';
    try {
      await commandContinueDistanceCalibration();
      distState = 'running';
      distMsg = 'Distance calibration running...';
    } catch (e) {
      distError = e.message;
    }
  }

  async function abortDistanceCalibration() {
    distError = '';
    try {
      await commandAbortDistanceCalibration();
      distState = 'idle';
      distMsg = '';
    } catch (e) {
      distError = e.message;
    }
  }

  // ---- Test ----
  async function handleTest() {
    testError = '';
    testMsg = '';
    try {
      await commandTestCalibration();
      testMsg = 'Test calibration command sent';
      setTimeout(() => { testMsg = ''; }, 2500);
    } catch (e) {
      testError = e.message;
    }
  }

  // Reactive label helpers
  $: calStateLabel = {
    idle: 'Not Started',
    open: 'Waiting for Clap',
    clapHappened: 'Clap Detected',
    waiting: 'Processing',
    calibrated: 'Calibrated'
  }[calState] ?? calState;

  $: distStateLabel = {
    idle: 'Not Started',
    running: 'Running',
    done: 'Complete'
  }[distState] ?? distState;
</script>

<div class="page-content">
  <h1 class="page-title">Calibration</h1>

  <!-- ======== Position Calibration ======== -->
  <div class="card" style="margin-bottom:1.25rem;">
    <div class="card-title">Position Calibration</div>

    <div class="state-box">
      <div class="state-label">Current State</div>
      <div class="state-value">{calStateLabel}</div>

      {#if calError}
        <div class="status-msg error">{calError}</div>
      {/if}
      {#if calMsg}
        <div class="status-msg info">{calMsg}</div>
      {/if}
    </div>

    <!-- State: idle -->
    {#if calState === 'idle'}
      <div class="btn-row">
        <button class="btn btn-primary" on:click={startCalibration}>Start Calibration</button>
      </div>

    <!-- State: open — waiting for clap -->
    {:else if calState === 'open'}
      <p class="state-hint">Make a clap near the device you want to calibrate.</p>
      <div class="btn-row">
        <button class="btn btn-ghost" on:click={cancelCalibration}>Cancel</button>
      </div>

    <!-- State: clapHappened — enter position -->
    {:else if calState === 'clapHappened'}
      <div class="clap-info">
        <strong>Board #{calBoardId}</strong> responded at reported position
        ({calX.toFixed(3)}, {calY.toFixed(3)})
      </div>
      <p class="state-hint">Enter the true physical position of this device:</p>
      <div class="form-row" style="margin-bottom:0.75rem;">
        <div class="form-group">
          <label>X Position</label>
          <input type="number" step="0.001" bind:value={calInputX} />
        </div>
        <div class="form-group">
          <label>Y Position</label>
          <input type="number" step="0.001" bind:value={calInputY} />
        </div>
      </div>
      <div class="btn-row">
        <button class="btn btn-primary" on:click={continueCalibration}>Submit Position</button>
        <button class="btn btn-ghost" on:click={cancelCalibration}>Cancel</button>
      </div>

    <!-- State: waiting — processing -->
    {:else if calState === 'waiting'}
      <p class="state-hint">Processing position data, please wait...</p>
      <div class="btn-row">
        <button class="btn btn-ghost" on:click={cancelCalibration}>Cancel</button>
      </div>

    <!-- State: calibrated -->
    {:else if calState === 'calibrated'}
      <div class="btn-row">
        <button class="btn btn-primary" on:click={startCalibration}>Calibrate Another</button>
        <button class="btn btn-secondary" on:click={endCalibration}>End & Save</button>
        <button class="btn btn-ghost" on:click={resetCalibration}>Reset All</button>
      </div>
    {/if}
  </div>

  <!-- ======== Distance Calibration ======== -->
  <div class="card" style="margin-bottom:1.25rem;">
    <div class="card-title">Distance Calibration</div>

    <div class="state-box">
      <div class="state-label">Current State</div>
      <div class="state-value">{distStateLabel}</div>

      {#if distError}
        <div class="status-msg error">{distError}</div>
      {/if}
      {#if distMsg}
        <div class="status-msg info">{distMsg}</div>
      {/if}
    </div>

    {#if distState === 'idle'}
      <div class="btn-row">
        <button class="btn btn-primary" on:click={startDistanceCalibration}>Start Distance Calibration</button>
      </div>

    {:else if distState === 'running'}
      <p class="state-hint">Move to the reference distance and confirm each step.</p>
      <div class="btn-row">
        <button class="btn btn-secondary" on:click={continueDistanceCalibration}>Continue</button>
        <button class="btn btn-ghost" on:click={abortDistanceCalibration}>Abort</button>
      </div>

    {:else if distState === 'done'}
      <div class="btn-row">
        <button class="btn btn-ghost" on:click={() => { distState = 'idle'; distMsg = ''; }}>Reset</button>
      </div>
    {/if}
  </div>

  <!-- ======== Test Calibration ======== -->
  <div class="card">
    <div class="card-title">Test Calibration</div>
    {#if testError}
      <div class="status-msg error">{testError}</div>
    {/if}
    {#if testMsg}
      <div class="status-msg success">{testMsg}</div>
    {/if}
    <p style="font-size:0.875rem; color:var(--color-text-muted); margin-bottom:0.75rem;">
      Send a test calibration command to verify the current calibration data.
    </p>
    <button class="btn btn-ghost" on:click={handleTest}>Run Test Calibration</button>
  </div>
</div>

<style>
  .clap-info {
    background: rgba(233, 69, 96, 0.1);
    border: 1px solid rgba(233, 69, 96, 0.3);
    border-radius: var(--radius);
    padding: 0.65rem 1rem;
    font-size: 0.9rem;
    margin-bottom: 0.75rem;
    color: var(--color-text);
  }

  .state-hint {
    font-size: 0.875rem;
    color: var(--color-text-muted);
    margin-bottom: 0.75rem;
  }

  .loc-list {
    list-style: none;
    margin: 0.75rem 0 0;
    padding: 0;
    font-size: 0.875rem;
    color: var(--color-text-muted);
  }

  .loc-list li {
    padding: 0.15rem 0;
  }

  .flip-toggle {
    display: flex;
    align-items: center;
    gap: 0.35rem;
    font-size: 0.875rem;
    color: var(--color-text-muted);
  }

  .map-stats {
    display: flex;
    flex-wrap: wrap;
    gap: 0.9rem;
    margin-top: 0.9rem;
    font-size: 0.875rem;
    color: var(--color-text-muted);
  }

  .map-stats .warn { color: #e9a04f; }
  .map-stats .ok   { color: #4fbf7a; }

  .map-view {
    display: block;
    width: 100%;
    max-width: 300px;
    margin-top: 0.75rem;
    background: rgba(255, 255, 255, 0.03);
    border: 1px solid rgba(255, 255, 255, 0.08);
    border-radius: var(--radius);
  }

  .map-view .board { fill: var(--color-primary, #e94560); }
  .map-view .board.untrusted { fill: none; stroke: var(--color-text-muted); stroke-width: 1.2; }
  .map-view .spot { fill: none; stroke: #4f9be9; stroke-width: 1.6; }
</style>
