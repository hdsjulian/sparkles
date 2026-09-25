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

  // knobs worth reaching for while tuning — remembered in this browser so a
  // setting that works survives the next session
  const SETTINGS_KEY = 'brain.settings';
  let anchor = 15;
  let rise = 30;
  let fall = 20;
  let bias = 0.5;
  let minValue = 60;
  let maxValue = 200;
  let ppg = true;
  try {
    const saved = JSON.parse(localStorage.getItem(SETTINGS_KEY) ?? 'null');
    if (saved) ({ anchor, rise, fall, bias, minValue, maxValue, ppg } = { anchor, rise, fall, bias, minValue, maxValue, ppg, ...saved });
  } catch { /* no storage here — defaults are fine */ }

  // last few minutes of evidence and settle, for the tuning chart
  const HISTORY_S = 180;
  const CHART_W = 600, CHART_H = 160, CHART_MAX = 1.2;
  let history = [];
  $: record($brainStatus);

  function record(st) {
    if (!st || !running) return;
    const now = Date.now();
    // SSE and the 3s poll both deliver frames; one point a second is plenty
    if (history.length && now - history[history.length - 1].t < 800) return;
    history = [...history, { t: now, ev: st.evidence ?? null, settle: st.settle ?? 0 }]
      .filter(p => now - p.t < HISTORY_S * 1000);
  }

  const y = v => CHART_H - Math.min(Math.max(v, 0), CHART_MAX) / CHART_MAX * CHART_H;

  function path(points, key) {
    if (!points.length) return '';
    const end = points[points.length - 1].t;
    const x = t => CHART_W - (end - t) / (HISTORY_S * 1000) * CHART_W;
    let d = '', pen = false;
    for (const p of points) {
      const v = p[key];
      if (v === null || v === undefined) { pen = false; continue; }   // gap during the baseline window
      d += `${pen ? 'L' : 'M'}${x(p.t).toFixed(1)},${y(v).toFixed(1)} `;
      pen = true;
    }
    return d;
  }

  $: evidence = s.evidence ?? null;
  $: activeBias = s.bias ?? bias;
  $: scores = Object.entries(s.scores ?? {});
  $: trend = evidence === null ? null
    : evidence > activeBias ? 'rising'
    : settle > 0 ? 'falling' : 'holding at dark';

  const scoreName = {
    still: 'stillness', emg: 'jaw / temple', bpm: 'heart rate', alpha: 'alpha', beta: 'beta',
  };

  $: s = $brainStatus ?? {};
  $: settle = s.settle ?? 0;
  $: phase = running ? (s.phase ?? 'waiting') : 'stopped';
  $: contact = s.contact ?? 0;
  $: connected = s.connected ?? false;
  $: channels = s.channels ?? {};
  $: statusText = !running ? 'not running'
    : !connected ? 'connecting…'
    : contact < 2 ? `connected, but only ${contact}/4 electrodes — adjust the fit`
    : (phaseText[phase] ?? phase);

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
      try {
        localStorage.setItem(SETTINGS_KEY,
          JSON.stringify({ anchor, rise, fall, bias, minValue, maxValue, ppg }));
      } catch { /* not persisted, still starts */ }
      history = [];
      const r = await brainStart({ ppg, anchor, rise, fall, bias, minValue, maxValue });
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
    <span class="phase" class:live={phase === 'live' && contact >= 2}
          class:warn={connected && contact < 2}>{statusText}</span>
  </div>

  <div class="headband">
    {#if scanning}
      <span class="dim">scanning…</span>
    {:else if running && !connected}
      <span class="dim">connecting… (scan takes ~8s, then it subscribes)</span>
    {:else if running}
      <span class="found">● connected</span>
      {#each Object.entries(channels) as [ch, uv]}
        <span class:bad={uv === null || uv < 5 || uv > 900} class="dim">
          {ch} {uv === null ? '?' : Math.round(uv)}µV
        </span>
      {/each}
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

{#if running}
<div class="panel">
  <h2>Tuning</h2>
  <div class="readout">
    {#if evidence === null}
      <span class="dim">evidence starts once the baseline window is over</span>
    {:else}
      <span class="big">{evidence.toFixed(2)}</span>
      <span>evidence — needs above {activeBias.toFixed(2)} to rise</span>
      <span class:up={trend === 'rising'} class:down={trend !== 'rising'}>{trend}</span>
    {/if}
  </div>

  <svg class="chart" viewBox="0 0 {CHART_W} {CHART_H}" preserveAspectRatio="none">
    <line class="grid" x1="0" x2={CHART_W} y1={y(1)} y2={y(1)} />
    <line class="grid" x1="0" x2={CHART_W} y1={y(0.5)} y2={y(0.5)} />
    <line class="bias" x1="0" x2={CHART_W} y1={y(activeBias)} y2={y(activeBias)} />
    <path class="ev" d={path(history, 'ev')} />
    <path class="st" d={path(history, 'settle')} />
  </svg>
  <div class="legend">
    <span><i class="k ev"></i>evidence</span>
    <span><i class="k st"></i>settle (lamp)</span>
    <span><i class="k bias"></i>bias</span>
    <span class="dim">last {HISTORY_S / 60} min</span>
  </div>

  {#if scores.length}
    <div class="scores">
      {#each scores as [name, v]}
        <span class="sname">{scoreName[name] ?? name}</span>
        <span class="strack">
          <span class="smid"></span>
          {#if v !== null}
            <span class="sfill" class:good={v >= 0.5}
                  style="left: {Math.min(v, 0.5) * 100}%; width: {Math.abs(v - 0.5) * 100}%"></span>
          {/if}
        </span>
        <span class="sval">{v === null ? '—' : v.toFixed(2)}</span>
      {/each}
    </div>
    <p class="hint">
      0.50 is exactly how you were when you sat down. Green is calmer than that,
      red is tenser. Stillness, jaw and heart rate carry the score; alpha and
      beta can only nudge it. Set the bias just under where evidence sits once
      you have genuinely settled. Watching this closely is itself enough to
      keep you tense, so glance rather than stare.
    </p>
  {/if}
</div>
{/if}

<fieldset class="panel" disabled={running}>
  <legend>Settings</legend>
  <label>Baseline window <input type="number" bind:value={anchor} min="5" max="300" /> s</label>
  <label>Time to full <input type="number" bind:value={rise} min="5" max="1800" /> s</label>
  <label>Time to dark <input type="number" bind:value={fall} min="5" max="1800" /> s</label>
  <label>Bias <input type="number" bind:value={bias} min="0.3" max="0.9" step="0.05" />
    <span class="dim">evidence needed to rise — lower is easier</span></label>
  <label>Min brightness <input type="number" bind:value={minValue} min="0" max="254" /></label>
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
  .phase.warn { color: #d9a441; }
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
  h2 { font-size: 1rem; color: #bbb; margin: 0 0 0.6rem; font-weight: normal; }
  .dim { color: #777; }
  .up { color: #7ec699; }
  .down { color: #d98080; }
  .chart {
    width: 100%; height: 160px; margin-top: 0.8rem;
    background: #111; border: 1px solid #333; border-radius: 4px;
  }
  .chart path { fill: none; stroke-width: 2; vector-effect: non-scaling-stroke; }
  .chart .ev { stroke: #6b9bd1; }
  .chart .st { stroke: #d9a441; }
  .chart .grid { stroke: #2a2a2a; vector-effect: non-scaling-stroke; }
  .chart .bias { stroke: #999; stroke-dasharray: 6 4; vector-effect: non-scaling-stroke; }
  .legend { display: flex; gap: 1.2rem; flex-wrap: wrap; font-size: 0.8rem; color: #999; margin-top: 0.3rem; }
  .k { display: inline-block; width: 14px; height: 3px; margin-right: 0.35rem; vertical-align: middle; }
  .k.ev { background: #6b9bd1; }
  .k.st { background: #d9a441; }
  .k.bias { background: repeating-linear-gradient(90deg, #999 0 4px, transparent 4px 7px); }
  .scores {
    display: grid; grid-template-columns: auto 1fr 3rem; gap: 0.4rem 0.8rem;
    align-items: center; margin-top: 1rem; font-size: 0.9rem;
  }
  .sname { color: #bbb; }
  .strack { position: relative; height: 10px; background: #111; border-radius: 3px; }
  .smid { position: absolute; left: 50%; top: -2px; bottom: -2px; width: 1px; background: #555; }
  .sfill { position: absolute; top: 0; bottom: 0; background: #d98080; border-radius: 3px; }
  .sfill.good { background: #7ec699; }
  .sval { color: #999; text-align: right; font-variant-numeric: tabular-nums; }
</style>
