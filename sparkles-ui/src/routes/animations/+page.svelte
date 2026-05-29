<script>
  import { setSyncAsyncParams, commandStrobeAll, commandBatteryBlinkAll, commandAnimationOff, commandBreath, commandBioluminescence, commandCandleAll } from '$lib/api.js';

  let error = '';
  let successMsg = '';

  // ----- Strobe -----
  let strobe = { frequency: 10, duration: 3000, hue: 0, saturation: 0, brightness: 255 };

  async function submitStrobe() {
    error = '';
    successMsg = '';
    try {
      await commandStrobeAll(strobe);
      successMsg = 'Strobe sent';
      setTimeout(() => { successMsg = ''; }, 2500);
    } catch (e) {
      error = e.message;
    }
  }

  async function sendBatteryBlink() {
    error = '';
    successMsg = '';
    try {
      await commandBatteryBlinkAll();
      successMsg = 'Battery blink sent';
      setTimeout(() => { successMsg = ''; }, 2500);
    } catch (e) {
      error = e.message;
    }
  }

  // ----- Colors + Candle variant -----
  let colorsCandle = {
    minRed: 0,
    maxRed: 255,
    minGreen: 0,
    maxGreen: 255,
    minBlue: 0,
    maxBlue: 255,
    minAniReps: 1,
    maxAniReps: 5,
    minSpeed: 10,
    maxSpeed: 100,
    minPause: 0,
    maxPause: 500,
    minReps: 1,
    maxReps: 10,
    minSpread: 0,
    maxSpread: 100,
    // Candle-specific: use hours/minutes/seconds for candle duration
    hours: 0,
    minutes: 30,
    seconds: 0,
    brightness: 200
  };

  // ----- Colors + Spatial variant -----
  let colorsSpatial = {
    minRed: 0,
    maxRed: 255,
    minGreen: 0,
    maxGreen: 255,
    minBlue: 0,
    maxBlue: 255,
    minAniReps: 1,
    maxAniReps: 5,
    minSpeed: 10,
    maxSpeed: 100,
    minPause: 0,
    maxPause: 500,
    minReps: 1,
    maxReps: 10,
    minSpread: 0,
    maxSpread: 100,
    hours: 0,
    minutes: 30,
    seconds: 0,
    brightness: 200
  };

  // ----- Good morning / good night -----
  let goodMorningHour = '07';
  let goodMorningMinute = '00';
  let goodNightHour = '22';
  let goodNightMinute = '00';

  async function submitColorsCandle() {
    error = '';
    successMsg = '';
    try {
      const params = {
        minRed: colorsCandle.minRed,
        maxRed: colorsCandle.maxRed,
        minGreen: colorsCandle.minGreen,
        maxGreen: colorsCandle.maxGreen,
        minBlue: colorsCandle.minBlue,
        maxBlue: colorsCandle.maxBlue,
        minAniReps: colorsCandle.minAniReps,
        maxAniReps: colorsCandle.maxAniReps,
        minSpeed: colorsCandle.minSpeed,
        maxSpeed: colorsCandle.maxSpeed,
        minPause: colorsCandle.minPause,
        maxPause: colorsCandle.maxPause,
        minReps: colorsCandle.minReps,
        maxReps: colorsCandle.maxReps,
        minSpread: colorsCandle.minSpread,
        maxSpread: colorsCandle.maxSpread,
        hours: colorsCandle.hours,
        minutes: colorsCandle.minutes,
        seconds: colorsCandle.seconds,
        brightness: colorsCandle.brightness
      };
      await setSyncAsyncParams(params);
      successMsg = 'Colors + Candle params saved';
      setTimeout(() => { successMsg = ''; }, 2500);
    } catch (e) {
      error = e.message;
    }
  }

  async function submitColorsSpatial() {
    error = '';
    successMsg = '';
    try {
      const params = {
        minRed: colorsSpatial.minRed,
        maxRed: colorsSpatial.maxRed,
        minGreen: colorsSpatial.minGreen,
        maxGreen: colorsSpatial.maxGreen,
        minBlue: colorsSpatial.minBlue,
        maxBlue: colorsSpatial.maxBlue,
        minAniReps: colorsSpatial.minAniReps,
        maxAniReps: colorsSpatial.maxAniReps,
        minSpeed: colorsSpatial.minSpeed,
        maxSpeed: colorsSpatial.maxSpeed,
        minPause: colorsSpatial.minPause,
        maxPause: colorsSpatial.maxPause,
        minReps: colorsSpatial.minReps,
        maxReps: colorsSpatial.maxReps,
        minSpread: colorsSpatial.minSpread,
        maxSpread: colorsSpatial.maxSpread,
        hours: colorsSpatial.hours,
        minutes: colorsSpatial.minutes,
        seconds: colorsSpatial.seconds,
        brightness: colorsSpatial.brightness
      };
      await setSyncAsyncParams(params);
      successMsg = 'Colors + Spatial params saved';
      setTimeout(() => { successMsg = ''; }, 2500);
    } catch (e) {
      error = e.message;
    }
  }

  // ----- Candle -----
  let candle = { hue: 20, saturation: 210, brightness: 180 };
  let candleActive = false;

  async function toggleCandle() {
    error = ''; successMsg = '';
    try {
      if (candleActive) {
        await commandAnimationOff();
        candleActive = false;
        successMsg = 'Candle off';
      } else {
        await commandCandleAll(candle);
        candleActive = true;
        successMsg = 'Candle started';
      }
      setTimeout(() => { successMsg = ''; }, 2500);
    } catch (e) {
      error = e.message;
    }
  }

  // ----- Bioluminescence -----
  let bioLum = { minInterval: 2000, maxInterval: 8000, fadeDuration: 1500, hue: 140, hueVariance: 20, saturation: 220, brightness: 80, repetitions: 0 };
  let bioLumActive = false;

  async function toggleBioluminescence() {
    error = ''; successMsg = '';
    try {
      if (bioLumActive) {
        await commandAnimationOff();
        bioLumActive = false;
        successMsg = 'Bioluminescence stopped';
      } else {
        await commandBioluminescence(bioLum);
        bioLumActive = true;
        successMsg = 'Bioluminescence started';
      }
      setTimeout(() => { successMsg = ''; }, 2500);
    } catch (e) {
      error = e.message;
    }
  }

  // ----- Breath -----
  let breath = { cycleDuration: 4000, spreadDelay: 2000, hue: 96, saturation: 180, brightness: 200, repetitions: 0 };
  let breathActive = false;

  async function toggleBreath() {
    error = ''; successMsg = '';
    try {
      if (breathActive) {
        await commandAnimationOff();
        breathActive = false;
        successMsg = 'Breath stopped';
      } else {
        await commandBreath(breath);
        breathActive = true;
        successMsg = 'Breath started';
      }
      setTimeout(() => { successMsg = ''; }, 2500);
    } catch (e) {
      error = e.message;
    }
  }
</script>

<div class="page-content">
  <div style="display:flex; align-items:center; justify-content:space-between; margin-bottom:1rem;">
    <h1 class="page-title" style="margin-bottom:0;">Animations</h1>
    <button class="btn btn-ghost" on:click={() => commandAnimationOff()}>All Off</button>
  </div>

  {#if error}
    <div class="status-msg error">{error}</div>
  {/if}
  {#if successMsg}
    <div class="status-msg success">{successMsg}</div>
  {/if}

  <!-- Battery Blink -->
  <div class="card" style="margin-bottom:1.25rem;">
    <div class="card-title">Battery Blink All</div>
    <p style="font-size:0.85rem;color:var(--color-text-muted);margin-bottom:0.75rem;">
      Each device blinks in its own battery color — white when full, red when low.
    </p>
    <button class="btn btn-primary" on:click={sendBatteryBlink}>Send Battery Blink</button>
  </div>

  <!-- Strobe -->
  <div class="card" style="margin-bottom:1.25rem;">
    <div class="card-title">Strobe All</div>
    <div class="param-section">
      <div class="form-row">
        <div class="form-group">
          <label>Frequency (flashes/s)</label>
          <input type="number" min="1" max="255" bind:value={strobe.frequency} />
        </div>
        <div class="form-group">
          <label>Duration (ms)</label>
          <input type="number" min="100" bind:value={strobe.duration} />
        </div>
        <div class="form-group">
          <label>Hue (0–255)</label>
          <input type="number" min="0" max="255" bind:value={strobe.hue} />
        </div>
        <div class="form-group">
          <label>Saturation (0–255)</label>
          <input type="number" min="0" max="255" bind:value={strobe.saturation} />
        </div>
        <div class="form-group">
          <label>Brightness (0–255)</label>
          <input type="number" min="0" max="255" bind:value={strobe.brightness} />
        </div>
      </div>
    </div>
    <button class="btn btn-primary" on:click={submitStrobe}>Send Strobe</button>
  </div>

  <!-- Colors + Candle -->
  <div class="card" style="margin-bottom:1.25rem;">
    <div class="card-title">Colors + Candle</div>

    <div class="param-section">
      <div class="param-heading">Color Ranges (0–255)</div>
      <div class="form-row">
        <div class="form-group"><label>Min Red</label><input type="number" min="0" max="255" bind:value={colorsCandle.minRed} /></div>
        <div class="form-group"><label>Max Red</label><input type="number" min="0" max="255" bind:value={colorsCandle.maxRed} /></div>
        <div class="form-group"><label>Min Green</label><input type="number" min="0" max="255" bind:value={colorsCandle.minGreen} /></div>
        <div class="form-group"><label>Max Green</label><input type="number" min="0" max="255" bind:value={colorsCandle.maxGreen} /></div>
        <div class="form-group"><label>Min Blue</label><input type="number" min="0" max="255" bind:value={colorsCandle.minBlue} /></div>
        <div class="form-group"><label>Max Blue</label><input type="number" min="0" max="255" bind:value={colorsCandle.maxBlue} /></div>
      </div>
    </div>

    <hr class="section-sep" />

    <div class="param-section">
      <div class="param-heading">Animation Parameters</div>
      <div class="form-row">
        <div class="form-group"><label>Min Ani Reps</label><input type="number" min="0" bind:value={colorsCandle.minAniReps} /></div>
        <div class="form-group"><label>Max Ani Reps</label><input type="number" min="0" bind:value={colorsCandle.maxAniReps} /></div>
        <div class="form-group"><label>Min Speed</label><input type="number" min="0" bind:value={colorsCandle.minSpeed} /></div>
        <div class="form-group"><label>Max Speed</label><input type="number" min="0" bind:value={colorsCandle.maxSpeed} /></div>
        <div class="form-group"><label>Min Pause</label><input type="number" min="0" bind:value={colorsCandle.minPause} /></div>
        <div class="form-group"><label>Max Pause</label><input type="number" min="0" bind:value={colorsCandle.maxPause} /></div>
        <div class="form-group"><label>Min Reps</label><input type="number" min="0" bind:value={colorsCandle.minReps} /></div>
        <div class="form-group"><label>Max Reps</label><input type="number" min="0" bind:value={colorsCandle.maxReps} /></div>
        <div class="form-group"><label>Min Spread</label><input type="number" min="0" bind:value={colorsCandle.minSpread} /></div>
        <div class="form-group"><label>Max Spread</label><input type="number" min="0" bind:value={colorsCandle.maxSpread} /></div>
      </div>
    </div>

    <hr class="section-sep" />

    <div class="param-section">
      <div class="param-heading">Duration & Brightness</div>
      <div class="form-row">
        <div class="form-group"><label>Hours</label><input type="number" min="0" max="23" bind:value={colorsCandle.hours} /></div>
        <div class="form-group"><label>Minutes</label><input type="number" min="0" max="59" bind:value={colorsCandle.minutes} /></div>
        <div class="form-group"><label>Seconds</label><input type="number" min="0" max="59" bind:value={colorsCandle.seconds} /></div>
        <div class="form-group"><label>Brightness</label><input type="number" min="0" max="255" bind:value={colorsCandle.brightness} /></div>
      </div>
    </div>

    <button class="btn btn-primary" on:click={submitColorsCandle}>Save Colors + Candle</button>
  </div>

  <!-- Colors + Spatial -->
  <div class="card" style="margin-bottom:1.25rem;">
    <div class="card-title">Colors + Spatial</div>

    <div class="param-section">
      <div class="param-heading">Color Ranges (0–255)</div>
      <div class="form-row">
        <div class="form-group"><label>Min Red</label><input type="number" min="0" max="255" bind:value={colorsSpatial.minRed} /></div>
        <div class="form-group"><label>Max Red</label><input type="number" min="0" max="255" bind:value={colorsSpatial.maxRed} /></div>
        <div class="form-group"><label>Min Green</label><input type="number" min="0" max="255" bind:value={colorsSpatial.minGreen} /></div>
        <div class="form-group"><label>Max Green</label><input type="number" min="0" max="255" bind:value={colorsSpatial.maxGreen} /></div>
        <div class="form-group"><label>Min Blue</label><input type="number" min="0" max="255" bind:value={colorsSpatial.minBlue} /></div>
        <div class="form-group"><label>Max Blue</label><input type="number" min="0" max="255" bind:value={colorsSpatial.maxBlue} /></div>
      </div>
    </div>

    <hr class="section-sep" />

    <div class="param-section">
      <div class="param-heading">Animation Parameters</div>
      <div class="form-row">
        <div class="form-group"><label>Min Ani Reps</label><input type="number" min="0" bind:value={colorsSpatial.minAniReps} /></div>
        <div class="form-group"><label>Max Ani Reps</label><input type="number" min="0" bind:value={colorsSpatial.maxAniReps} /></div>
        <div class="form-group"><label>Min Speed</label><input type="number" min="0" bind:value={colorsSpatial.minSpeed} /></div>
        <div class="form-group"><label>Max Speed</label><input type="number" min="0" bind:value={colorsSpatial.maxSpeed} /></div>
        <div class="form-group"><label>Min Pause</label><input type="number" min="0" bind:value={colorsSpatial.minPause} /></div>
        <div class="form-group"><label>Max Pause</label><input type="number" min="0" bind:value={colorsSpatial.maxPause} /></div>
        <div class="form-group"><label>Min Reps</label><input type="number" min="0" bind:value={colorsSpatial.minReps} /></div>
        <div class="form-group"><label>Max Reps</label><input type="number" min="0" bind:value={colorsSpatial.maxReps} /></div>
        <div class="form-group"><label>Min Spread</label><input type="number" min="0" bind:value={colorsSpatial.minSpread} /></div>
        <div class="form-group"><label>Max Spread</label><input type="number" min="0" bind:value={colorsSpatial.maxSpread} /></div>
      </div>
    </div>

    <hr class="section-sep" />

    <div class="param-section">
      <div class="param-heading">Duration & Brightness</div>
      <div class="form-row">
        <div class="form-group"><label>Hours</label><input type="number" min="0" max="23" bind:value={colorsSpatial.hours} /></div>
        <div class="form-group"><label>Minutes</label><input type="number" min="0" max="59" bind:value={colorsSpatial.minutes} /></div>
        <div class="form-group"><label>Seconds</label><input type="number" min="0" max="59" bind:value={colorsSpatial.seconds} /></div>
        <div class="form-group"><label>Brightness</label><input type="number" min="0" max="255" bind:value={colorsSpatial.brightness} /></div>
      </div>
    </div>

    <button class="btn btn-primary" on:click={submitColorsSpatial}>Save Colors + Spatial</button>
  </div>

  <!-- Candle -->
  <div class="card" style="margin-bottom:1.25rem;">
    <div class="card-title">Candle</div>
    <p style="font-size:0.85rem;color:var(--color-text-muted);margin-bottom:0.75rem;">
      Each lamp flickers independently like a candle flame until switched off.
    </p>
    <div class="form-row" style="margin-bottom:0.75rem;">
      <div class="form-group"><label>Hue (0–255)</label><input type="number" min="0" max="255" bind:value={candle.hue} /></div>
      <div class="form-group"><label>Saturation (0–255)</label><input type="number" min="0" max="255" bind:value={candle.saturation} /></div>
      <div class="form-group"><label>Brightness (0–255)</label><input type="number" min="0" max="255" bind:value={candle.brightness} /></div>
    </div>
    <button class="btn {candleActive ? 'btn-warning' : 'btn-primary'}" on:click={toggleCandle}>
      {candleActive ? 'Turn Off Candle' : 'Start Candle'}
    </button>
  </div>

  <!-- Bioluminescence -->
  <div class="card" style="margin-bottom:1.25rem;">
    <div class="card-title">Bioluminescence</div>
    <p style="font-size:0.85rem;color:var(--color-text-muted);margin-bottom:0.75rem;">
      Each lamp pulses independently at random intervals with a slow blue-green glow.
    </p>
    <div class="form-row">
      <div class="form-group"><label>Min interval (ms)</label><input type="number" min="100" max="30000" step="100" bind:value={bioLum.minInterval} /></div>
      <div class="form-group"><label>Max interval (ms)</label><input type="number" min="100" max="60000" step="100" bind:value={bioLum.maxInterval} /></div>
      <div class="form-group"><label>Fade duration (ms)</label><input type="number" min="100" max="10000" step="100" bind:value={bioLum.fadeDuration} /></div>
      <div class="form-group"><label>Hue</label><input type="number" min="0" max="255" bind:value={bioLum.hue} /></div>
      <div class="form-group"><label>Hue variance</label><input type="number" min="0" max="127" bind:value={bioLum.hueVariance} /></div>
      <div class="form-group"><label>Saturation</label><input type="number" min="0" max="255" bind:value={bioLum.saturation} /></div>
      <div class="form-group"><label>Brightness</label><input type="number" min="0" max="255" bind:value={bioLum.brightness} /></div>
      <div class="form-group"><label>Reps (0=∞)</label><input type="number" min="0" max="999" bind:value={bioLum.repetitions} /></div>
    </div>
    <button class="btn {bioLumActive ? 'btn-warning' : 'btn-primary'}" on:click={toggleBioluminescence}>
      {bioLumActive ? 'Stop Bioluminescence' : 'Start Bioluminescence'}
    </button>
  </div>

  <!-- Breath -->
  <div class="card" style="margin-bottom:1.25rem;">
    <div class="card-title">Breath</div>
    <p style="font-size:0.85rem;color:var(--color-text-muted);margin-bottom:0.75rem;">
      All lamps fade in and out like breathing. Lamps farther from center start slightly later, creating a ripple across the forest.
    </p>
    <div class="form-row">
      <div class="form-group"><label>Cycle (ms)</label><input type="number" min="500" max="30000" step="500" bind:value={breath.cycleDuration} /></div>
      <div class="form-group"><label>Spread (ms)</label><input type="number" min="0" max="10000" step="250" bind:value={breath.spreadDelay} /></div>
      <div class="form-group"><label>Hue</label><input type="number" min="0" max="255" bind:value={breath.hue} /></div>
      <div class="form-group"><label>Saturation</label><input type="number" min="0" max="255" bind:value={breath.saturation} /></div>
      <div class="form-group"><label>Brightness</label><input type="number" min="0" max="255" bind:value={breath.brightness} /></div>
      <div class="form-group"><label>Reps (0=∞)</label><input type="number" min="0" max="999" bind:value={breath.repetitions} /></div>
    </div>
    <button class="btn {breathActive ? 'btn-warning' : 'btn-primary'}" on:click={toggleBreath}>
      {breathActive ? 'Stop Breath' : 'Start Breath'}
    </button>
  </div>

  <!-- Good Morning / Good Night -->
  <div class="card">
    <div class="card-title">Good Morning / Good Night Times</div>
    <div class="form-row" style="align-items:flex-end;">
      <div class="time-block">
        <div class="param-heading">Good Morning</div>
        <div class="form-row">
          <div class="form-group">
            <label>Hour</label>
            <input type="number" min="0" max="23" bind:value={goodMorningHour} />
          </div>
          <div class="form-group">
            <label>Minute</label>
            <input type="number" min="0" max="59" bind:value={goodMorningMinute} />
          </div>
        </div>
      </div>
      <div class="time-block">
        <div class="param-heading">Good Night</div>
        <div class="form-row">
          <div class="form-group">
            <label>Hour</label>
            <input type="number" min="0" max="23" bind:value={goodNightHour} />
          </div>
          <div class="form-group">
            <label>Minute</label>
            <input type="number" min="0" max="59" bind:value={goodNightMinute} />
          </div>
        </div>
      </div>
    </div>
    <p style="font-size:0.8rem; color:var(--color-text-muted); margin-top:0.5rem;">
      These times are sent as part of the animation params (hours/minutes fields).
    </p>
  </div>
</div>

<style>
  .param-section {
    margin-bottom: 0.5rem;
  }

  .param-heading {
    font-size: 0.78rem;
    font-weight: 600;
    color: var(--color-text-muted);
    text-transform: uppercase;
    letter-spacing: 0.05em;
    margin-bottom: 0.5rem;
  }

  .time-block {
    flex: 1;
    min-width: 200px;
  }
</style>
