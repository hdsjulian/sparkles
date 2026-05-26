<script>
  import { onMount } from 'svelte';
  import { getMidiParams, setMidiParams } from '$lib/api.js';
  import NoUiSlider from '$lib/components/NoUiSlider.svelte';

  let error = '';
  let successMsg = '';
  let loaded = false;

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

  // Slider refs for programmatic update
  let pitchSlider;
  let valSlider;
  let satSliderComp;
  let hueSlider;
  let satSingleSlider;
  let dbSlider;
  let distSlider;

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

  onMount(async () => {
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
    <div class="card-title">Hue (0–360)</div>
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
    <div class="card-title">Saturation Single (0–255)</div>
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

  <!-- dB Range -->
  <div class="card" style="margin-bottom:1.25rem;">
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
