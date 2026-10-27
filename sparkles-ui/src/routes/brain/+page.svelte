<script>
  import { onMount, onDestroy } from 'svelte';
  import { brainStart, brainStop, brainReset, brainTune, getBrainStatus, getBrainDevices,
           brainRecord, brainRecordStop, getBrainRecordings, brainRecordingUrl,
           brainWeights, getBrainPresets, brainPresetSave, brainPresetDelete } from '$lib/api.js';
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
  let bias = 0.5;
  // brightness on a head before they settle — not the old minValue, which also
  // lit the lamps with nobody wearing it, so a saved one is left behind
  let wornValue = 13;
  let maxValue = 200;
  let ppg = true;
  // the lamp curve, live: where a session starts, and how fast it climbs and sinks
  let startLevel = 0.3;
  let up = 0.1;
  let down = 0.15;
  try {
    const saved = JSON.parse(localStorage.getItem(SETTINGS_KEY) ?? 'null');
    if (saved) ({ anchor, bias, wornValue, maxValue, ppg, startLevel, up, down } =
      { anchor, bias, wornValue, maxValue, ppg, startLevel, up, down, ...saved });
  } catch { /* no storage here — defaults are fine */ }

  function save() {
    try {
      localStorage.setItem(SETTINGS_KEY,
        JSON.stringify({ anchor, bias, wornValue, maxValue, ppg, startLevel, up, down }));
    } catch { /* not persisted, still works */ }
  }

  // a slider let go: kept here and on the pi, and handed to a running
  // session straight away
  async function tune(params) {
    save();
    try {
      const r = await brainTune(params);
      if (r.status !== true) throw new Error(r.detail ?? 'tune failed');
    } catch (e) { error = e.message; }
  }

  $: startLamp = Math.round(wornValue + (maxValue - wornValue) * startLevel);

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
    alpha: 'alpha', beta: 'beta', theta: 'theta', bpm: 'heart rate', still: 'stillness', emg: 'jaw / temple',
  };

  // how much each signal counts toward the lamps: sliders, and named sets of
  // them kept on the pi
  const signals = [
    { key: 'alpha', name: 'Alpha', what: 'relaxed wakefulness, calmer when up' },
    { key: 'beta',  name: 'Beta',  what: 'active thinking, calmer when down' },
    { key: 'theta', name: 'Theta', what: 'deep meditation, calmer when up; eye movements look like it too' },
    { key: 'bpm',   name: 'Heart rate', what: 'calmer when slower' },
    { key: 'still', name: 'Stillness', what: 'calmer when moving less' },
    { key: 'emg',   name: 'Jaw / temple', what: 'calmer when looser' },
  ];
  let weights = { alpha: 2, beta: 2, theta: 0, bpm: 0.5, still: 0.25, emg: 0.25 };
  let presets = { builtin: {}, saved: {} };

  const sameWeights = (a, b) => signals.every(({ key }) => Number(a?.[key] ?? 0) === Number(b?.[key] ?? 0));
  $: activePreset = Object.entries({ ...presets.builtin, ...presets.saved })
    .find(([, w]) => sameWeights(w, weights))?.[0] ?? null;

  async function loadPresets() {
    try { presets = await getBrainPresets(); } catch { /* listed next time */ }
  }

  async function weigh(changed) {
    try {
      const r = await brainWeights(changed);
      if (r.status !== true) throw new Error(r.detail ?? 'could not set the weights');
    } catch (e) { error = e.message; }
  }

  function applyPreset(w) {
    weights = { ...weights, ...w };
    weigh(weights);
  }

  async function savePreset() {
    const name = prompt('Name for this preset');
    if (!name?.trim()) return;
    try {
      const r = await brainPresetSave(name.trim(), weights);
      if (r.status !== true) throw new Error(r.detail ?? 'could not save the preset');
      await loadPresets();
    } catch (e) { error = e.message; }
  }

  async function deletePreset(name) {
    if (!confirm(`Delete the preset "${name}"?`)) return;
    try {
      const r = await brainPresetDelete(name);
      if (r.status !== true) throw new Error(r.detail ?? 'could not delete the preset');
      await loadPresets();
    } catch (e) { error = e.message; }
  }

  $: s = $brainStatus ?? {};
  $: settle = s.settle ?? 0;
  $: phase = running ? (s.phase ?? 'waiting') : 'stopped';
  $: contact = s.contact ?? 0;
  $: connected = s.connected ?? false;
  $: channels = s.channels ?? {};
  $: statusText = !running ? 'not listening'
    : !connected ? (s.worn ? 'link lost — reconnecting, the lamps hold'
                           : 'listening — no headband connected (switched off, or reconnecting)')
    : !s.worn ? 'headband on, nobody wearing it — the microphone has the lamps'
    : contact < 2 ? `only ${contact}/4 electrodes — adjust the fit`
    : (phaseText[phase] ?? phase);

  const phaseText = {
    waiting: 'waiting for the headband',
    settling: 'settling in — waiting for a steady brain signal',
    anchor:  'learning your baseline — lamps coming up',
    live:    'listening',
    stopped: 'not listening',
  };

  // the pi keeps the settings it listens with; take them once, so a page
  // opened in another browser shows what the lamps are really doing
  let synced = false;

  async function refresh() {
    try {
      const st = await getBrainStatus();
      if (!synced && st.settings) {
        const p = st.settings;
        ppg = p.ppg; anchor = p.anchor; bias = p.bias; wornValue = p.minValue;
        maxValue = p.maxValue; startLevel = p.start; up = p.up; down = p.down;
        if (p.weights) weights = { ...weights, ...p.weights };
        synced = true;
      }
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
      save();
      history = [];
      const r = await brainStart({ ppg, anchor, bias, minValue: wornValue, maxValue,
                                   start: startLevel, up, down });
      if (r.status === false) throw new Error(r.detail ?? 'failed to start');
      running = true;
    } catch (e) { error = e.message; }
    finally { busy = false; }
  }

  // the score starts over too, and the chart's evidence was measured against
  // the old baseline, so it goes as well
  async function reset() {
    error = ''; busy = true;
    try {
      const r = await brainReset();
      if (r.status !== true) throw new Error(r.detail ?? 'reset failed');
      history = [];
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

  // guided tests: each learns the baseline again (eyes open, sitting calmly),
  // then records a condition, so the scoring can be judged and re-run offline
  const tests = [
    { label: 'meditation', name: 'Meditation',
      task: 'Close your eyes. Breathe slowly and let thoughts pass without following them.' },
    { label: 'desk', name: 'Desk work',
      task: 'Work at the computer: read, type, browse, or play something like GeoGuessr.' },
    { label: 'talk', name: 'Talk & move',
      task: 'Talk with someone, look around, gesture, laugh. Be lively, but keep the headband on.' },
  ];
  const testName = Object.fromEntries(tests.map(t => [t.label, t]));
  let testMinutes = 3;
  let recordings = [];
  let lastFile = null;

  $: rec = s.recording ?? null;
  $: result = s.recorded ?? null;
  $: if (result?.file && result.file !== lastFile) { lastFile = result.file; loadRecordings(); }

  async function loadRecordings() {
    try { recordings = await getBrainRecordings(); } catch { /* listed next time */ }
  }

  async function runTest(label) {
    error = ''; busy = true;
    try {
      const r = await brainRecord(label, testMinutes * 60);
      if (r.status !== true) throw new Error(r.detail ?? 'could not start the test');
      history = [];
    } catch (e) { error = e.message; }
    finally { busy = false; }
  }

  async function stopTest() {
    try { await brainRecordStop(); } catch (e) { error = e.message; }
  }

  const clock = secs => `${Math.floor(secs / 60)}:${String(secs % 60).padStart(2, '0')}`;
  const pct = x => x == null ? '—' : `${Math.round(x * 100)}%`;
  const num = x => x == null ? '—' : x.toFixed(2);

  onMount(() => {
    loadRecordings();
    loadPresets();
    refresh();
    scan();
    poll = setInterval(refresh, 3000);
  });
  onDestroy(() => clearInterval(poll));
</script>

<h1>Brain (test)</h1>

<p class="blurb">
  Settling score straight to brightness on every lamp that is powered on,
  whenever someone wears the headband. Once started the pi listens all the
  time, through restarts and reboots, until Stop. Switch the headband on and
  put it on: the microphone and keyboard step back, the lamps come up while
  your baseline is learned, then follow you. Take it off and the lamps go back
  to the microphone.
</p>

{#if error}<div class="error">{error}</div>{/if}

<div class="panel">
  <div class="row">
    {#if running}
      <button class="stop" on:click={stop} disabled={busy}>Stop listening</button>
      <button class="reset" on:click={reset} disabled={busy}>Reset baseline</button>
    {:else}
      <button class="start" on:click={start} disabled={busy}>Start listening</button>
    {/if}
    <span class="phase" class:live={phase === 'live' && contact >= 2}
          class:warn={connected && contact < 2}>{statusText}</span>
  </div>

  <div class="headband">
    {#if scanning}
      <span class="dim">scanning…</span>
    {:else if running && !connected}
      <span class="dim">listening — the headband connects within about 15 s of being switched on</span>
    {:else if running}
      <span class="found">● connected</span>
      {#each Object.entries(channels) as [ch, uv]}
        <span class:bad={s.good ? !s.good.includes(ch) : (uv === null || uv < 5 || uv > 900)} class="dim">
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
  {#if running && s.worn}
    <p class="hint">Pitch detection and the keyboard are kept off the lamps while the headband is on a head.</p>
  {/if}
</div>

<div class="panel">
  <h2>Tests</h2>
  {#if rec}
    <div class="test-run">
      <strong>{testName[rec.label]?.name ?? rec.label}</strong>
      {#if rec.stage === 'baseline'}
        <p>Sit comfortably with your eyes open and look calmly at one spot.
          <span class="dim">{phase === 'settling' ? 'Waiting for a steady brain signal…'
            : phase === 'anchor' ? 'Learning your baseline…' : 'Put the headband on.'}</span></p>
      {:else}
        <p class="task">{testName[rec.label]?.task}</p>
        <p><span class="big">{clock(rec.left ?? 0)}</span> <span class="dim">left</span></p>
      {/if}
      <button class="reset" on:click={stopTest}>Stop test</button>
    </div>
  {:else}
    <div class="tests">
      {#each tests as t}
        <button class="reset" on:click={() => runTest(t.label)} disabled={!running || busy}>{t.name}</button>
        <span class="dim">{t.task}</span>
      {/each}
    </div>
    <label class="dim">Length <select bind:value={testMinutes}>
      <option value={2}>2</option><option value={3}>3</option><option value={5}>5</option>
    </select> minutes, after the baseline is learned again with your eyes open</label>
  {/if}

  {#if result}
    <div class="result">
      <strong>{testName[result.label]?.name ?? result.label}</strong>
      {#if result.outcome === 'no steady signal'}
        — no steady brain signal within 3 minutes; check the fit and try again.
      {:else}
        {result.outcome === 'done' ? '' : `(${result.outcome} after ${clock(result.seconds)})`}
        <div>Evidence {num(result.evidence)} on average, above the bias ({num(result.bias)})
          {pct(result.above_bias)} of the time.</div>
        {#if result.settle}
          <div>Lamp {num(result.settle.start)} → {num(result.settle.end)}, highest {num(result.settle.max)}.</div>
        {/if}
        <div class="dim">
          {#each Object.entries(result.scores ?? {}) as [k, v]}{scoreName[k] ?? k} {num(v)}&ensp;{/each}
        </div>
      {/if}
    </div>
  {/if}

  {#if recordings.length}
    <details>
      <summary class="dim">{recordings.length} recording{recordings.length === 1 ? '' : 's'} on the pi</summary>
      {#each recordings as r}
        <div><a href={brainRecordingUrl(r.name)} download>{r.name}</a>
          <span class="dim">{(r.bytes / 1e6).toFixed(1)} MB</span></div>
      {/each}
    </details>
  {/if}
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
      red is tenser. The lamps follow their average, weighted in the Signals
      panel. Set the bias just under where evidence sits once
      you have genuinely settled. Watching this closely is itself enough to
      keep you tense, so glance rather than stare.
    </p>
  {/if}
</div>
{/if}

<div class="panel">
  <h2>Lamp curve</h2>
  <div class="sliders">
    <span class="sname">Brightness at 0.50</span>
    <input type="range" min="0" max="0.9" step="0.01" bind:value={startLevel}
           on:change={() => tune({ start: startLevel })} />
    <span class="sval">{startLevel.toFixed(2)}</span>
    <span class="dim shint">where a session starts once the baseline is learned —
      lamp {startLamp}/255{running ? ', moved there now to judge it' : ''}</span>

    <span class="sname">Increase factor</span>
    <input type="range" min="0.01" max="0.5" step="0.01" bind:value={up}
           on:change={() => tune({ up })} />
    <span class="sval">{up.toFixed(2)}</span>
    <span class="dim shint">at full calm, halfway to full every {(Math.LN2 / up).toFixed(1)} s</span>

    <span class="sname">Degradation factor</span>
    <input type="range" min="0.01" max="0.5" step="0.01" bind:value={down}
           on:change={() => tune({ down })} />
    <span class="sval">{down.toFixed(2)}</span>
    <span class="dim shint">at full tension, halves every {(Math.LN2 / down).toFixed(1)} s</span>
  </div>
  <p class="hint">Both ways are fast at first and flatten out: toward full when
    calmer than you arrived, toward dark when tenser. These apply live.</p>
</div>

<div class="panel">
  <h2>Signals</h2>
  <div class="presets">
    {#each Object.entries(presets.builtin) as [name, w]}
      <button class="reset" class:on={activePreset === name} on:click={() => applyPreset(w)}>{name}</button>
    {/each}
    {#each Object.entries(presets.saved) as [name, w]}
      <span class="saved">
        <button class="reset" class:on={activePreset === name} on:click={() => applyPreset(w)}>{name}</button>
        <button class="link" title="delete this preset" on:click={() => deletePreset(name)}>×</button>
      </span>
    {/each}
    <button class="link" on:click={savePreset}>save as new preset</button>
  </div>
  <div class="sliders">
    {#each signals as sig}
      <span class="sname">{sig.name}</span>
      <input type="range" min="0" max="3" step="0.1" bind:value={weights[sig.key]}
             on:change={() => weigh({ [sig.key]: weights[sig.key] })} />
      <span class="sval">{Number(weights[sig.key]).toFixed(1)}</span>
      <span class="dim shint">{sig.what}{Number(weights[sig.key]) === 0 ? ' — left out'
        : s.scores?.[sig.key] != null ? ` — now ${s.scores[sig.key].toFixed(2)}` : ''}</span>
    {/each}
  </div>
  <p class="hint">Each signal is scored against how you arrived, 0.50 being exactly
    that. The lamps follow their average, weighted here: the calmer, the
    brighter. 0 leaves a signal out. These apply live and are kept on the pi.</p>
</div>

<fieldset class="panel" disabled={running}>
  <legend>Settings</legend>
  <label>Baseline window <input type="number" bind:value={anchor} min="5" max="300" /> s</label>
  <label>Bias <input type="number" bind:value={bias} min="0.3" max="0.9" step="0.05" />
    <span class="dim">evidence needed to rise — lower is easier</span></label>
  <label>Brightness on a head <input type="number" bind:value={wornValue} min="0" max="254" />
    <span class="dim">the lamp at settle 0 — 13 is about 5%, off a head is dark</span></label>
  <label>Max brightness <input type="number" bind:value={maxValue} min="1" max="255" /></label>
  <label class="check"><input type="checkbox" bind:checked={ppg} /> Heart rate (PPG)</label>
  <p class="hint">Locked while listening — stop, change, then start again.</p>
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
  .reset { background: #444; }
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
  .sliders {
    display: grid; grid-template-columns: auto 1fr 3rem; gap: 0.2rem 0.8rem;
    align-items: center; font-size: 0.9rem;
  }
  .sliders input[type="range"] { width: 100%; }
  .shint { grid-column: 1 / -1; font-size: 0.8rem; margin-bottom: 0.6rem; }
  .presets { display: flex; flex-wrap: wrap; gap: 0.4rem; align-items: center; margin-bottom: 0.9rem; }
  .presets button.reset { font-size: 0.85rem; padding: 0.35rem 0.8rem; }
  .presets button.on { background: #2d7d46; }
  .presets .saved { display: inline-flex; align-items: center; gap: 0.15rem; }
  .tests {
    display: grid; grid-template-columns: 9rem 1fr; gap: 0.5rem 0.8rem;
    align-items: center; margin-bottom: 0.8rem; font-size: 0.9rem;
  }
  .tests button, .test-run button { font-size: 0.9rem; padding: 0.4rem 0.8rem; }
  .test-run .task { font-size: 1.1rem; color: #eee; }
  .test-run .big { font-size: 1.6rem; color: #eee; font-variant-numeric: tabular-nums; }
  .result {
    margin-top: 0.8rem; padding-top: 0.6rem; border-top: 1px solid #333;
    color: #ccc; font-size: 0.9rem; line-height: 1.5;
  }
  details { margin-top: 0.8rem; font-size: 0.85rem; }
  details a { color: #6b9bd1; }
</style>
