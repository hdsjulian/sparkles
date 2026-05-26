<script>
  import { onMount } from 'svelte';
  import { getDarkroomParams, setDarkroomParams } from '$lib/api.js';
  import NoUiSlider from '$lib/components/NoUiSlider.svelte';

  let error = '';
  let successMsg = '';
  let loaded = false;

  // Strobe timeframe (1–120)
  let strobeMin = 1;
  let strobeMax = 60;

  // Brightness (100–255)
  let brightnessMin = 100;
  let brightnessMax = 200;

  // Candle (51–255)
  let candlelightMin = 51;
  let candlelightMax = 180;
  let candlelightEnabled = true;

  // Redlight (10–255)
  let redlightMin = 10;
  let redlightMax = 150;
  let redLightEnabled = true;

  onMount(async () => {
    try {
      const p = await getDarkroomParams();
      strobeMin = p.strobeMin ?? 1;
      strobeMax = p.strobeMax ?? 60;
      brightnessMin = p.redlightMin ?? 100; // using redlightMin/Max as brightness (server field names)
      brightnessMax = p.redlightMax ?? 200;
      candlelightMin = p.candlelightMin ?? 51;
      candlelightMax = p.candlelightMax ?? 180;
      redLightEnabled = !!p.redLightEnabled;
      candlelightEnabled = !!p.candlelightEnabled;
      loaded = true;
    } catch (e) {
      error = `Failed to load darkroom params: ${e.message}`;
      loaded = true;
    }
  });

  async function handleSubmit() {
    error = '';
    successMsg = '';
    try {
      await setDarkroomParams({
        strobeMin,
        strobeMax,
        redlightMin: brightnessMin,
        redlightMax: brightnessMax,
        candlelightMin,
        candlelightMax,
        redLightEnabled: redLightEnabled ? 1 : 0,
        candleLightEnabled: candlelightEnabled ? 1 : 0
      });
      successMsg = 'Darkroom params saved';
      setTimeout(() => { successMsg = ''; }, 2500);
    } catch (e) {
      error = e.message;
    }
  }
</script>

<div class="page-content">
  <h1 class="page-title">Darkroom</h1>

  {#if error}
    <div class="status-msg error">{error}</div>
  {/if}
  {#if successMsg}
    <div class="status-msg success">{successMsg}</div>
  {/if}

  {#if !loaded}
    <div class="card" style="text-align:center; color:var(--color-text-muted); padding:2rem;">Loading...</div>
  {:else}

  <!-- Strobe Timeframe -->
  <div class="card" style="margin-bottom:1.25rem;">
    <div class="card-title">Strobe Timeframe (1–120 s)</div>
    <div class="slider-row">
      <span class="slider-val">{strobeMin}s</span>
      <div class="slider-container">
        <NoUiSlider
          min={1}
          max={120}
          start={[strobeMin, strobeMax]}
          step={1}
          connect={true}
          tooltips={true}
          on:change={(e) => { strobeMin = e.detail[0]; strobeMax = e.detail[1]; }}
        />
      </div>
      <span class="slider-val">{strobeMax}s</span>
    </div>
  </div>

  <!-- Brightness -->
  <div class="card" style="margin-bottom:1.25rem;">
    <div class="card-title">Brightness (100–255)</div>
    <div class="slider-row">
      <span class="slider-val">{brightnessMin}</span>
      <div class="slider-container">
        <NoUiSlider
          min={100}
          max={255}
          start={[brightnessMin, brightnessMax]}
          step={1}
          connect={true}
          tooltips={true}
          on:change={(e) => { brightnessMin = e.detail[0]; brightnessMax = e.detail[1]; }}
        />
      </div>
      <span class="slider-val">{brightnessMax}</span>
    </div>
  </div>

  <!-- Candle Light -->
  <div class="card" style="margin-bottom:1.25rem;">
    <div class="checkbox-row" style="margin-bottom:0.75rem;">
      <input type="checkbox" id="candle-enable" bind:checked={candlelightEnabled} />
      <label for="candle-enable">Enable Candle Light</label>
    </div>
    <div class="card-title">Candle Light (51–255)</div>
    <div class="slider-row">
      <span class="slider-val" class:muted={!candlelightEnabled}>{candlelightMin}</span>
      <div class="slider-container">
        <NoUiSlider
          min={51}
          max={255}
          start={[candlelightMin, candlelightMax]}
          step={1}
          connect={true}
          tooltips={true}
          disabled={!candlelightEnabled}
          on:change={(e) => { candlelightMin = e.detail[0]; candlelightMax = e.detail[1]; }}
        />
      </div>
      <span class="slider-val" class:muted={!candlelightEnabled}>{candlelightMax}</span>
    </div>
  </div>

  <!-- Red Light -->
  <div class="card" style="margin-bottom:1.25rem;">
    <div class="checkbox-row" style="margin-bottom:0.75rem;">
      <input type="checkbox" id="red-enable" bind:checked={redLightEnabled} />
      <label for="red-enable">Enable Red Light</label>
    </div>
    <div class="card-title">Red Light (10–255)</div>
    <div class="slider-row">
      <span class="slider-val" class:muted={!redLightEnabled}>{redlightMin}</span>
      <div class="slider-container">
        <NoUiSlider
          min={10}
          max={255}
          start={[redlightMin, redlightMax]}
          step={1}
          connect={true}
          tooltips={true}
          disabled={!redLightEnabled}
          on:change={(e) => { redlightMin = e.detail[0]; redlightMax = e.detail[1]; }}
        />
      </div>
      <span class="slider-val" class:muted={!redLightEnabled}>{redlightMax}</span>
    </div>
  </div>

  <button class="btn btn-primary" style="min-width:160px;" on:click={handleSubmit}>
    Save Darkroom Settings
  </button>

  {/if}
</div>

<style>
  .slider-row {
    display: flex;
    align-items: center;
    gap: 0.75rem;
  }

  .slider-container {
    flex: 1;
  }

  .slider-val {
    min-width: 40px;
    font-size: 0.85rem;
    font-weight: 600;
    color: var(--color-accent);
    text-align: right;
    transition: color 0.2s;
  }

  .slider-val.muted {
    color: var(--color-text-muted);
  }
</style>
