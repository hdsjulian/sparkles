<script>
  import { onMount, onDestroy } from 'svelte';
  import { brainStart, brainStop, getBrainStatus, getBrainDevices } from '$lib/api.js';
  import { brainStatus } from '$lib/stores.js';

  let error = '';
  let running = false;
  let busy = false;
  let poll;
  let headband = null;      // last scan result
  let scanning = false;

  // knobs worth reaching for while bringing this up on a lamp or two
  let anchor = 40;
  let rise = 120;
  let fall = 40;
  let maxValue = 200;
  let ppg = true;

  $: s = $brainStatus ?? {};
  $: settle = s.settle ?? 0;
  $: phase = running ? (s.phase ?? 'waiting') : 'stopped';
  $: contact = s.contact ?? 0;

  const phaseText = {
    waiting: 'waiting for the headband',
    anchor:  'learning your baseline — lamps stay dark',
    live:    'listening',
    stopped: 'not running',
  };

  async function refresh() {
    try {
      const st = await getBrainStatus();
      running = st.running;
      if (st.running) brainStatus.set(st);
    } catch { /* the page is useful even if a poll misses */ }
  }

  async function scan() {
    scanning = true;
    try {
      headband = await getBrainDevices();
    } catch (e) {
      headband = { error: e.message };
    } finally {
      scanning = false;
    }
  }

  async function start() {
    error = ''; busy = true;
    try {
      const r = await brainStart({ ppg, anchor, rise, fall, maxValue });
      if (r.status === false) throw new Error(r.detail ?? 'failed to start');
      running = true;
    } catch (e) { error = e.message; }
    finally { busy = false; }
  }

  async function stop() {
    error = ''; busy = true;
    try {
      await brainStop();
      running = false;
      brainStatus.set(null);
      scan();                 // confirm the headband let go of the link
    } catch (e) { error = e.message; }
    finally { busy = false; }
  }

  onMount(() => {
    refresh();
    scan();
    poll = setInterval(refresh, 3000);
  });
  onDestroy(() => clearInterval(poll));
</script>

<h1>Brain (test)</h1>

<p class="blurb">
  Settling score straight to brightness on every lamp that is powered on.
  No positions, no distance, no animation curve — if a lamp is dark the score is
  low. Put the headband on, wait out the baseline window, then actually relax.
</p>

{#if error}<div class="error">{error}</div>{/if}

<div class="panel">
  <div class="row">
    {#if running}
      <button class="stop" on:click={stop} disabled={busy}>Stop</button>
    {:else}
      <button class="start" on:click={start} disabled={busy}>Start</button>
    {/if}
    <span class="phase" class:live={phase === 'live'}>{phaseText[phase] ?? phase}</span>
  </div>

  <div class="headband">
    {#if scanning}
      <span class="dim">scanning…</span>
    {:else if running && phase === 'waiting'}
      <span class="dim">connecting… (scan takes ~8s, then it subscribes)</span>
    {:else if running}
      <span class="found">● connected</span>
      <span class="dim">contact {contact}/4</span>
    {:else if headband?.error}
      <span class="bad">scan failed: {headband.error}</span>
    {:else if headband?.devices?.length}
      {#each headband.devices as d}
        <span class="found">● {d.name}</span>
        <span class="dim">{d.rssi} dBm{d.rssi > -70 ? '' : d.rssi > -80 ? ' (weak)' : ' (very weak)'}</span>
      {/each}
    {:else if headband}
      <span class="bad">● no headband found</span>
      <span class="dim">switched off, or already connected to a phone</span>
    {/if}
    <button class="link" on:click={scan} disabled={scanning || running}>rescan</button>
  </div>

  <div class="meter">
    <div class="fill" style="width: {Math.round(settle * 100)}%"></div>
  </div>
  <div class="readout">
    <span class="big">{settle.toFixed(2)}</span>
    <span>lamp {s.value ?? 0}/255</span>
    <span class:bad={running && contact < 2}>contact {contact}/4</span>
    {#if s.session}<span>{Math.round(s.session)}s</span>{/if}
    {#if s.bpm}<span>{Math.round(s.bpm)} bpm</span>{/if}
    {#if s.battery}<span>headband {Math.round(s.battery)}%</span>{/if}
  </div>
</div>

<fieldset class="panel" disabled={running}>
  <legend>Settings</legend>
  <label>Baseline window <input type="number" bind:value={anchor} min="5" max="300" /> s</label>
  <label>Time to full <input type="number" bind:value={rise} min="5" max="1800" /> s</label>
  <label>Time to dark <input type="number" bind:value={fall} min="5" max="1800" /> s</label>
  <label>Max brightness <input type="number" bind:value={maxValue} min="1" max="255" /></label>
  <label class="check"><input type="checkbox" bind:checked={ppg} /> Heart rate (PPG)</label>
  <p class="hint">Locked while running — stop first to change them.</p>
</fieldset>

<style>
  h1 { margin-bottom: 0.2rem; }
  .blurb { color: #999; max-width: 46rem; margin-top: 0; }
  .error { color: #f44336; font-size: 0.9rem; }
  .panel {
    background: #1b1b1b; border: 1px solid #333; border-radius: 8px;
    padding: 1rem; margin-bottom: 1rem; max-width: 46rem;
  }
  .row { display: flex; align-items: center; gap: 1rem; margin-bottom: 1rem; }
  button {
    font-size: 1rem; padding: 0.6rem 1.6rem; border-radius: 6px;
    border: none; cursor: pointer; color: #fff;
  }
  button:disabled { opacity: 0.5; cursor: default; }
  .start { background: #2d7d46; }
  .stop  { background: #9b3030; }
  .phase { color: #888; }
  .headband {
    display: flex; align-items: center; gap: 0.6rem;
    margin-bottom: 0.8rem; font-size: 0.9rem;
  }
  .headband .found { color: #7ec699; }
  .headband .bad { color: #d98080; }
  .headband .dim { color: #777; }
  button.link {
    background: none; border: none; color: #6b9bd1;
    text-decoration: underline; cursor: pointer; padding: 0; font-size: 0.85rem;
  }
  .phase.live { color: #7ec699; }
  .meter {
    height: 28px; background: #111; border: 1px solid #333;
    border-radius: 4px; overflow: hidden;
  }
  .fill {
    height: 100%; background: linear-gradient(90deg, #3a5a8a, #d9a441);
    transition: width 0.4s linear;
  }
  .readout {
    display: flex; gap: 1.2rem; flex-wrap: wrap;
    margin-top: 0.5rem; color: #999; font-variant-numeric: tabular-nums;
  }
  .readout .big { color: #eee; font-size: 1.3rem; min-width: 3.5rem; }
  .readout .bad { color: #d98080; }
  fieldset { border: 1px solid #333; }
  fieldset:disabled { opacity: 0.55; }
  legend { color: #999; padding: 0 0.4rem; }
  label { display: block; margin-bottom: 0.5rem; color: #ccc; }
  label.check { margin-top: 0.8rem; }
  input[type="number"] { width: 5.5rem; margin: 0 0.3rem; }
  .hint { color: #777; font-size: 0.85rem; margin-bottom: 0; }
</style>
