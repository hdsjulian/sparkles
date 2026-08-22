<script>
  import { onMount } from 'svelte';
  import { getMidiParams, setMidiParams, getColors, setColors, getAudioLevel, setRmsThresholds } from '$lib/api.js';
  import NoUiSlider from '$lib/components/NoUiSlider.svelte';

  let error = '';
  let successMsg = '';
  let loaded = false;

  // ---- Five-step level test ----------------------------------------------
  // aubio owns the audio device, so nothing else can open it. It reports what it
  // hears to /audioLevel and this samples that — the browser never touches audio.
  const TEST_STEPS = 5;
  const STEP_SECONDS = 3;
  let levelTest = { step: 0, recording: false, results: [], error: '', live: null, stale: true };
  let liveTimer = null;

  async function pollLive() {
    try {
      const l = await getAudioLevel();
      levelTest.live = l.db;
      levelTest.stale = l.stale;
    } catch {
      levelTest.stale = true;
    }
    levelTest = levelTest;
  }

  function startLiveMeter() {
    if (liveTimer) return;
    pollLive();
    liveTimer = setInterval(pollLive, 400);
  }

  function stopLiveMeter() {
    if (liveTimer) clearInterval(liveTimer);
    liveTimer = null;
  }

  async function recordStep() {
    levelTest.error = '';
    levelTest.recording = true;
    levelTest = levelTest;
    const samples = [];
    const until = Date.now() + STEP_SECONDS * 1000;
    while (Date.now() < until) {
      try {
        const l = await getAudioLevel();
        // ignore the digital floor: silence would drag the average down and make
        // a step look quieter than it was actually sung
        if (!l.stale && typeof l.db === 'number' && l.db > -90) samples.push(l.db);
      } catch { /* keep sampling, one dropped read does not spoil a step */ }
      await new Promise(r => setTimeout(r, 200));
    }
    levelTest.recording = false;
    if (!samples.length) {
      levelTest.error = 'Heard nothing — is aubio running and the input right?';
      levelTest = levelTest;
      return;
    }
    const avg = samples.reduce((a, b) => a + b, 0) / samples.length;
    levelTest.results = [...levelTest.results,
      { step: levelTest.step + 1, db: avg, peak: Math.max(...samples), n: samples.length }];
    levelTest.step += 1;
    levelTest = levelTest;
  }

  function resetTest() {
    levelTest = { step: 0, recording: false, results: [], error: '', live: levelTest.live, stale: levelTest.stale };
  }

  // quietest step sets the gate, loudest sets the top of the range
  $: testMin = levelTest.results.length ? Math.min(...levelTest.results.map(r => r.db)) : null;
  $: testMax = levelTest.results.length ? Math.max(...levelTest.results.map(r => r.db)) : null;

  async function applyTest() {
    levelTest.error = '';
    try {
      const res = await setRmsThresholds(testMin.toFixed(1), testMax.toFixed(1));
      rmsMin = res.rmsMin;
      rmsMax = res.rmsMax;
      successMsg = `RMS range set to ${res.rmsMin} … ${res.rmsMax} dB and saved`;
      setTimeout(() => { successMsg = ''; }, 3000);
    } catch (e) {
      levelTest.error = e.message;
    }
    levelTest = levelTest;
  }

  // Param state
  let valMin = 0;
  let valMax = 127;
  let satMin = 0;
  let satMax = 255;
  let hue = 180;
  let saturation = 200;
  let rangeMin = 200;
  let rangeMax = 800;
  let rmsMin = -60;
  let rmsMax = 0;
  let mode = 'midi'; // 'midi' | 'frequency'
  let distance = 100;
  let distanceSwitch = false;
  let distanceMode = 'brightness'; // 'brightness' | 'delay'
  let shimmerHue = 31;
  let shimmerSat = 255;

  // Slider refs for programmatic update
  let pitchSlider;
  let valSlider;
  let satSliderComp;
  let hueSlider;
  let satSingleSlider;
  let dbSlider;
  let distSlider;
  let shimmerHueSlider;
  let shimmerSatSlider;

  const instruments = [
    { label: 'Clarinet',   min: 180, max: 620 },
    { label: 'Alto Sax',   min: 138, max: 880 },
    { label: 'Tenor Sax',  min: 104, max: 698 },
    { label: 'Flute',      min: 262, max: 2093 },
    { label: 'Trumpet',    min: 165, max: 988 },
    { label: 'Low Voice',  min: 85,  max: 255 },
    { label: 'Mid Voice',  min: 165, max: 622 },
    { label: 'High Voice', min: 255, max: 1100 },
  ];

  import { onDestroy } from 'svelte';
  onDestroy(stopLiveMeter);

  onMount(async () => {
    startLiveMeter();
    try {
      const p = await getMidiParams();
      valMin = p.valMin ?? 0;
      valMax = p.valMax ?? 127;
      satMin = p.satMin ?? 0;
      satMax = p.satMax ?? 255;
      hue = p.hue ?? 180;
      saturation = p.saturation ?? 200;
      rangeMin = p.rangeMin ?? 200;
      rangeMax = p.rangeMax ?? 800;
      rmsMin = p.rmsMin ?? -60;
      rmsMax = p.rmsMax ?? 0;
      mode = p.mode ?? 'midi';
      distance = p.distance ?? 100;
      distanceSwitch = !!p.distanceSwitch;
      distanceMode = p.distanceMode ?? 'brightness';
      // raspi color store is the source of truth for hues
      const c = await getColors();
      hue = c.midi?.hue ?? hue;
      saturation = c.midi?.saturation ?? saturation;
      shimmerHue = c.shimmer?.hue ?? shimmerHue;
      shimmerSat = c.shimmer?.saturation ?? shimmerSat;
      loaded = true;
    } catch (e) {
      error = `Failed to load MIDI params: ${e.message}`;
      loaded = true;
    }
  });

  function applyPreset(inst) {
    rangeMin = inst.min;
    rangeMax = inst.max;
    if (pitchSlider) pitchSlider.setValues([inst.min, inst.max]);
  }

  async function handleSubmit() {
    error = '';
    successMsg = '';
    try {
      await setMidiParams({
        minVal: valMin,
        maxVal: valMax,
        minSat: satMin,
        maxSat: satMax,
        midiSaturation: saturation,
        midiHue: hue,
        rangeMin,
        rangeMax,
        minDb: rmsMin,
        maxDb: rmsMax,
        distance,
        distanceSwitch: distanceSwitch ? 1 : 0,
        distanceMode,
        mode
      });
      await setColors({
        midiHue: hue,
        midiSaturation: saturation,
        shimmerHue,
        shimmerSaturation: shimmerSat
      });
      successMsg = 'MIDI params saved';
      setTimeout(() => { successMsg = ''; }, 2500);
    } catch (e) {
      error = e.message;
    }
  }
</script>

<div class="page-content">
  <h1 class="page-title">MIDI Settings</h1>

  {#if error}
    <div class="status-msg error">{error}</div>
  {/if}
  {#if successMsg}
    <div class="status-msg success">{successMsg}</div>
  {/if}

  {#if !loaded}
    <div class="card" style="text-align:center; color:var(--color-text-muted); padding:2rem;">Loading...</div>
  {:else}

  <!-- Mode toggle -->
  <div class="card" style="margin-bottom:1.25rem;">
    <div class="card-title">Mode</div>
    <div class="mode-toggle">
      <button
        class="btn {mode === 'midi' ? 'btn-primary' : 'btn-ghost'}"
        on:click={() => { mode = 'midi'; }}
      >MIDI</button>
      <button
        class="btn {mode === 'frequency' ? 'btn-primary' : 'btn-ghost'}"
        on:click={() => { mode = 'frequency'; }}
      >Frequency</button>
    </div>
  </div>

  <!-- Instrument presets (frequency mode) -->
  {#if mode === 'frequency'}
  <div class="card" style="margin-bottom:1.25rem;">
    <div class="card-title">Instrument Presets</div>
    <div class="preset-grid">
      {#each instruments as inst}
        <button class="btn btn-ghost btn-sm" on:click={() => applyPreset(inst)}>
          {inst.label}<br><span class="preset-range">{inst.min}–{inst.max} Hz</span>
        </button>
      {/each}
    </div>
  </div>
  {/if}

  <!-- Pitch Range -->
  <div class="card" style="margin-bottom:1.25rem;">
    <div class="card-title">Pitch Range {mode === 'frequency' ? '(Hz)' : '(MIDI note)'}</div>
    <div class="slider-row">
      <span class="slider-val">{rangeMin}</span>
      <div class="slider-container">
        <NoUiSlider
          bind:this={pitchSlider}
          min={mode === 'frequency' ? 20 : 0}
          max={mode === 'frequency' ? 4000 : 127}
          start={[rangeMin, rangeMax]}
          step={1}
          connect={true}
          tooltips={true}
          on:change={(e) => { rangeMin = e.detail[0]; rangeMax = e.detail[1]; }}
        />
      </div>
      <span class="slider-val">{rangeMax}</span>
    </div>
  </div>

  <!-- Val Range -->
  <div class="card" style="margin-bottom:1.25rem;">
    <div class="card-title">Velocity / Val Range (0–127)</div>
    <div class="slider-row">
      <span class="slider-val">{valMin}</span>
      <div class="slider-container">
        <NoUiSlider
          bind:this={valSlider}
          min={0}
          max={127}
          start={[valMin, valMax]}
          step={1}
          connect={true}
          tooltips={true}
          on:change={(e) => { valMin = e.detail[0]; valMax = e.detail[1]; }}
        />
      </div>
      <span class="slider-val">{valMax}</span>
    </div>
  </div>

  <!-- Saturation Range -->
  <div class="card" style="margin-bottom:1.25rem;">
    <div class="card-title">Saturation Range (0–255)</div>
    <div class="slider-row">
      <span class="slider-val">{satMin}</span>
      <div class="slider-container">
        <NoUiSlider
          bind:this={satSliderComp}
          min={0}
          max={255}
          start={[satMin, satMax]}
          step={1}
          connect={true}
          tooltips={true}
          on:change={(e) => { satMin = e.detail[0]; satMax = e.detail[1]; }}
        />
      </div>
      <span class="slider-val">{satMax}</span>
    </div>
  </div>

  <!-- Hue (single) -->
  <div class="card" style="margin-bottom:1.25rem;">
    <div class="card-title">MIDI Hue (0–360)</div>
    <div class="slider-row">
      <span class="slider-val">{hue}</span>
      <div class="slider-container">
        <NoUiSlider
          bind:this={hueSlider}
          min={0}
          max={360}
          start={hue}
          step={1}
          connect={true}
          tooltips={true}
          on:change={(e) => { hue = e.detail; }}
        />
      </div>
    </div>
  </div>

  <!-- Saturation single -->
  <div class="card" style="margin-bottom:1.25rem;">
    <div class="card-title">MIDI Saturation (0–255)</div>
    <div class="slider-row">
      <span class="slider-val">{saturation}</span>
      <div class="slider-container">
        <NoUiSlider
          bind:this={satSingleSlider}
          min={0}
          max={255}
          start={saturation}
          step={1}
          connect={true}
          tooltips={true}
          on:change={(e) => { saturation = e.detail; }}
        />
      </div>
    </div>
  </div>

  <!-- Shimmer (mic) color -->
  <div class="card" style="margin-bottom:1.25rem;">
    <div class="card-title">Shimmer / Mic Hue (0–360)</div>
    <div class="slider-row" style="margin-bottom:0.75rem;">
      <span class="slider-val">{shimmerHue}</span>
      <div class="slider-container">
        <NoUiSlider
          bind:this={shimmerHueSlider}
          min={0}
          max={360}
          start={shimmerHue}
          step={1}
          connect={true}
          tooltips={true}
          on:change={(e) => { shimmerHue = e.detail; }}
        />
      </div>
    </div>
    <div class="card-title">Shimmer / Mic Saturation (0–255)</div>
    <div class="slider-row">
      <span class="slider-val">{shimmerSat}</span>
      <div class="slider-container">
        <NoUiSlider
          bind:this={shimmerSatSlider}
          min={0}
          max={255}
          start={shimmerSat}
          step={1}
          connect={true}
          tooltips={true}
          on:change={(e) => { shimmerSat = e.detail; }}
        />
      </div>
    </div>
  </div>

  <!-- dB Range -->
  <div class="card" style="margin-bottom:1.25rem;">
    <div class="card-title">Sound Level Test</div>
    <p class="hint">
      Sing a sustained note at five steadily louder levels, three seconds each — from
      barely audible to as loud as you will ever perform. The quietest step becomes the
      gate, the loudest the top of the range. Saved on the pi, so it survives a reboot,
      and it stops the detector from deriving its own thresholds from the room.
    </p>

    <div class="live-row">
      <span class="muted">input</span>
      {#if levelTest.stale}
        <span class="live-bad">no signal — is aubio running?</span>
      {:else}
        <span class="live-db">{levelTest.live?.toFixed(1)} dB</span>
        <span class="live-bar"><span class="live-fill"
          style="width:{Math.max(0, Math.min(100, ((levelTest.live ?? -90) + 90) * 1.4))}%"></span></span>
      {/if}
    </div>

    {#if levelTest.error}
      <div class="status-msg error">{levelTest.error}</div>
    {/if}

    {#if levelTest.results.length}
      <table class="level-table">
        <thead><tr><th>step</th><th>average</th><th>peak</th></tr></thead>
        <tbody>
          {#each levelTest.results as r}
            <tr><td>{r.step}</td><td>{r.db.toFixed(1)} dB</td><td>{r.peak.toFixed(1)} dB</td></tr>
          {/each}
        </tbody>
      </table>
    {/if}

    <div class="btn-row" style="margin-top:0.75rem;">
      {#if levelTest.step < TEST_STEPS}
        <button class="btn btn-primary" on:click={recordStep} disabled={levelTest.recording}>
          {levelTest.recording ? `Recording ${STEP_SECONDS}s...` : `Record step ${levelTest.step + 1} of ${TEST_STEPS}`}
        </button>
      {:else}
        <button class="btn btn-primary" on:click={applyTest}>
          Set {testMin?.toFixed(1)} … {testMax?.toFixed(1)} dB
        </button>
      {/if}
      {#if levelTest.results.length}
        <button class="btn btn-ghost" on:click={resetTest} disabled={levelTest.recording}>Start over</button>
      {/if}
    </div>
  </div>

  <div class="card">
    <div class="card-title">RMS dB Range</div>
    <div class="slider-row">
      <span class="slider-val">{rmsMin} dB</span>
      <div class="slider-container">
        <NoUiSlider
          bind:this={dbSlider}
          min={-120}
          max={0}
          start={[rmsMin, rmsMax]}
          step={1}
          connect={true}
          tooltips={true}
          on:change={(e) => { rmsMin = e.detail[0]; rmsMax = e.detail[1]; }}
        />
      </div>
      <span class="slider-val">{rmsMax} dB</span>
    </div>
  </div>

  <!-- Distance -->
  <div class="card" style="margin-bottom:1.25rem;">
    <div class="card-title">Distance</div>
    <div class="slider-row" style="margin-bottom:0.75rem;">
      <span class="slider-val">{distance}</span>
      <div class="slider-container">
        <NoUiSlider
          bind:this={distSlider}
          min={0}
          max={1000}
          start={distance}
          step={1}
          connect={true}
          tooltips={true}
          on:change={(e) => { distance = e.detail; }}
        />
      </div>
    </div>

    <div class="switch-row">
      <label class="switch">
        <input type="checkbox" bind:checked={distanceSwitch} />
        <span class="slider-track"></span>
      </label>
      <span class="switch-label">Enable Distance Control</span>
    </div>

    {#if distanceSwitch}
      <div class="radio-group" style="margin-top:0.75rem;">
        <div class="card-title">Distance Mode</div>
        <label class="radio-label">
          <input type="radio" bind:group={distanceMode} value="brightness" />
          Brightness
        </label>
        <label class="radio-label">
          <input type="radio" bind:group={distanceMode} value="delay" />
          Delay
        </label>
      </div>
    {/if}
  </div>

  <button class="btn btn-primary" style="min-width:160px;" on:click={handleSubmit}>
    Save MIDI Settings
  </button>

  {/if}
</div>

<style>
  .hint { font-size: 0.85rem; color: var(--color-text-muted); margin-bottom: 0.75rem; }
  .muted { color: var(--color-text-muted); font-size: 0.8rem; }
  .live-row { display: flex; align-items: center; gap: 0.6rem; margin-bottom: 0.75rem; }
  .live-db { font-variant-numeric: tabular-nums; font-weight: 700; min-width: 5rem; }
  .live-bad { color: var(--color-low, #f44336); font-size: 0.85rem; }
  .live-bar { flex: 1; height: 8px; border-radius: 4px; background: rgba(255,255,255,0.08); overflow: hidden; }
  .live-fill { display: block; height: 100%; background: var(--color-ok, #4fbf7a); transition: width 0.2s linear; }
  .level-table { width: 100%; font-size: 0.85rem; border-collapse: collapse; }
  .level-table th { text-align: left; color: var(--color-text-muted); font-weight: 500; padding: 0.2rem 0; }
  .level-table td { padding: 0.15rem 0; font-variant-numeric: tabular-nums; }

  .mode-toggle {
    display: flex;
    gap: 0.5rem;
  }

  .preset-grid {
    display: flex;
    flex-wrap: wrap;
    gap: 0.5rem;
  }

  .preset-grid .btn {
    flex-direction: column;
    line-height: 1.3;
    text-align: center;
  }

  .preset-range {
    font-size: 0.7rem;
    color: var(--color-text-muted);
    font-weight: 400;
  }

  .slider-row {
    display: flex;
    align-items: center;
    gap: 0.75rem;
  }

  .slider-container {
    flex: 1;
  }

  .slider-val {
    min-width: 50px;
    font-size: 0.85rem;
    font-weight: 600;
    color: var(--color-accent);
    text-align: right;
  }

  .radio-group {
    display: flex;
    flex-direction: column;
    gap: 0.4rem;
  }

  .radio-label {
    display: flex;
    align-items: center;
    gap: 0.5rem;
    font-size: 0.9rem;
    color: var(--color-text);
    cursor: pointer;
  }

  .radio-label input[type="radio"] {
    accent-color: var(--color-accent);
    width: 16px;
    height: 16px;
  }
</style>
