<script>
  import { onMount, onDestroy } from 'svelte';
  import { devices } from '$lib/stores.js';
  import { latencyTest, stopLatencyTest, getLatencyStatus, getLatencyRuns, clearLatencyRuns } from '$lib/api.js';

  // A spread of the per-burst delay under 0.3 ms — 10 cm of sound — is worth
  // trying as a fixed correction; anything more is not.
  const STEADY_MS = 0.3;
  const CM_PER_MS = 34.3;

  let boardId = 0;
  let distance = 1.0;
  let bursts = 60;
  let gap = 0;
  let label = '';

  let running = false;
  let ending = false;
  let remote = null;     // a run this page did not start: opened mid-run, or another browser
  let remoteTimer = null;
  let es = null;
  let error = '';
  let run = null;        // the run on screen: live, or one picked from the saved list
  let pastRuns = [];
  let openBurst = null;
  let showAll = false;

  $: boardIds = [...$devices.keys()].map(Number).filter((n) => !isNaN(n)).sort((a, b) => a - b);
  $: s = run?.summary;
  $: v = verdict(s);
  $: chart = buildChart(run);

  onMount(() => {
    loadRuns();
    checkRemote();
  });
  onDestroy(() => {
    es?.close();
    clearTimeout(remoteTimer);
  });

  // a run started elsewhere still shows, and can still be ended, from here
  async function checkRemote() {
    clearTimeout(remoteTimer);
    try {
      const st = await getLatencyStatus();
      if (st.running && !running) {
        remote = st;
        ending = !!st.ending;
        remoteTimer = setTimeout(checkRemote, 3000);
        return;
      }
    } catch {
      // no status, no banner
    }
    if (remote) loadRuns();
    remote = null;
    if (!running) ending = false;
  }

  async function endTest() {
    ending = true;
    try {
      await stopLatencyTest();
    } catch (e) {
      error = e.message;
    }
    // our own run finishes through latency_done; a remote one is followed by checkRemote
    if (remote) checkRemote();
  }

  async function loadRuns() {
    try {
      pastRuns = (await getLatencyRuns()).slice().reverse();
    } catch {
      // the list is a convenience, the test runs without it
    }
  }

  function start() {
    error = '';
    openBurst = null;
    run = { label, boardId, distance, requested: bursts, gap, started: Date.now() / 1000, bursts: [], summary: null };
    running = true;
    es = latencyTest({ boardId, distance, bursts, gap, label });
    es.addEventListener('latency_start', (e) => {
      run = { ...run, ...JSON.parse(e.data), bursts: [] };
    });
    es.addEventListener('latency_result', (e) => {
      const d = JSON.parse(e.data);
      run = { ...run, bursts: [...run.bursts, d.burst], summary: d.summary };
    });
    es.addEventListener('latency_error', (e) => {
      error = JSON.parse(e.data).detail;
    });
    es.addEventListener('latency_done', (e) => {
      const d = JSON.parse(e.data);
      run = { ...run, summary: d.summary, finished: d.finished };
      finish();
    });
    // closed on any error: an EventSource reconnects by itself, and here a
    // reconnect would quietly start a second test
    es.onerror = () => {
      if (running && !error) {
        error = run.bursts.length
          ? 'Connection to the test was lost — the Pi saved what was measured'
          : 'Could not start the test — is another one already running?';
      }
      finish();
    };
  }

  function finish() {
    running = false;
    ending = false;
    es?.close();
    es = null;
    loadRuns();
  }

  const toggleBurst = (n) => (openBurst = openBurst === n ? null : n);

  async function clearRuns() {
    if (!confirm('Delete every saved latency run on the Pi?')) return;
    await clearLatencyRuns();
    pastRuns = [];
  }

  function verdict(sum) {
    if (!sum || !(sum.measured >= 5)) return null;
    return sum.rangeMs <= STEADY_MS
      ? { ok: true, text: `Held still to within ${sum.rangeCm} cm across ${sum.measured} bursts. Repeat after a power cycle and a volume change before relying on it.` }
      : { ok: false, text: `Moved by ${sum.rangeCm} cm across ${sum.measured} bursts — a fixed correction will not hold.` };
  }

  // ---- chart: every chirp as a dot, each burst's average as the line ----
  const W = 640;
  const H = 190;
  const LEFT = 52;
  const RIGHT = 8;
  const BOTTOM = 20;
  const TOP = 8;

  function buildChart(r) {
    if (!r?.bursts?.length) return null;
    const dots = [];
    const means = [];
    for (const b of r.bursts) {
      (b.delays ?? []).forEach((d, i) => { if (d != null) dots.push([b.t + i * 0.5, d]); });
      if (b.meanDelayMs != null) means.push([b.t + 2.25, b.meanDelayMs]);
    }
    if (!dots.length) return null;
    const xs = dots.map((p) => p[0]);
    const ys = dots.map((p) => p[1]);
    const x0 = Math.min(...xs);
    const x1 = Math.max(Math.max(...xs), x0 + 1);
    let y0 = Math.min(...ys);
    let y1 = Math.max(...ys);
    if (y1 - y0 < 0.2) { const mid = (y0 + y1) / 2; y0 = mid - 0.1; y1 = mid + 0.1; }
    const pad = (y1 - y0) * 0.08;
    y0 -= pad;
    y1 += pad;
    const sx = (x) => LEFT + ((x - x0) / (x1 - x0)) * (W - LEFT - RIGHT);
    const sy = (y) => H - BOTTOM - ((y - y0) / (y1 - y0)) * (H - BOTTOM - TOP);
    return {
      dots: dots.map(([x, y]) => [sx(x), sy(y)]),
      line: means.map(([x, y]) => `${sx(x).toFixed(1)},${sy(y).toFixed(1)}`).join(' '),
      y0, y1, span: x1 - x0
    };
  }

  // ---- formatting ----
  const num = (x, digits = 2) => (x == null ? '—' : Number(x).toFixed(digits));
  const cm = (x) => (x == null ? '—' : (x * CM_PER_MS).toFixed(1));
  const clock = (sec) => `${Math.floor(sec / 60)}:${String(Math.floor(sec % 60)).padStart(2, '0')}`;
  const when = (t) => new Date(t * 1000).toLocaleString();

  // ---- downloads, everything that was measured ----
  function save(name, text, type) {
    const a = document.createElement('a');
    a.href = URL.createObjectURL(new Blob([text], { type }));
    a.download = name;
    a.click();
    setTimeout(() => URL.revokeObjectURL(a.href), 1000);
  }
  const stamp = (r) => new Date(r.started * 1000).toISOString().slice(0, 19).replace(/[:T]/g, '-');
  const csvCell = (x) => {
    const str = x == null ? '' : String(x);
    return /[",\n]/.test(str) ? `"${str.replace(/"/g, '""')}"` : str;
  };
  function toCsv(runs) {
    const rows = [['run_started', 'label', 'board', 'true_distance_m', 'burst', 't_s', 'status', 'chirp',
                   'measured_m', 'delay_ms', 'burst_mean_ms', 'burst_drift_ppm', 'burst_scatter_ms']];
    for (const r of runs) {
      for (const b of r.bursts ?? []) {
        (b.d ?? []).forEach((m, i) => rows.push([
          new Date(r.started * 1000).toISOString(), r.label, r.boardId, r.distance, b.n, b.t, b.status, i,
          m || '', b.delays?.[i], b.meanDelayMs, b.driftPpm, b.jitterMs
        ]));
      }
    }
    return rows.map((row) => row.map(csvCell).join(',')).join('\n');
  }
  const saveJson = (runs, name) => save(`${name}.json`, JSON.stringify(runs, null, 2), 'application/json');
  const saveCsv = (runs, name) => save(`${name}.csv`, toCsv(runs), 'text/csv');
</script>

<div class="card" style="margin-bottom:1.25rem;">
  <div class="card-title">Speaker Latency Test</div>
  <p class="hint">
    Is the speaker's delay steady enough to correct for in code? Put one board at a measured
    distance in front of the speaker — 1 m works well — and enter it here. Everything beyond
    that distance is delay in the audio path. A burst takes about 9 s, so 60 bursts is roughly
    9 minutes. Then power-cycle the speaker and run again, change its volume and run again,
    and do one run with earbuds taped to the board's microphone: that run shows the boards'
    own clock drift, which every other run includes. Nothing is recorded on the master, and
    every run is saved on the Pi, also when stopped.
  </p>

  {#if error}
    <div class="status-msg error">{error}</div>
  {/if}

  <div class="form-row">
    <div class="form-group">
      <label for="lt-board">Board</label>
      <select id="lt-board" bind:value={boardId} disabled={running}>
        {#each boardIds as id}
          <option value={id}>#{id}</option>
        {:else}
          <option value={0}>#0</option>
        {/each}
      </select>
    </div>
    <div class="form-group">
      <label for="lt-distance">Distance to speaker (m)</label>
      <input id="lt-distance" type="number" min="0" step="0.01" bind:value={distance} disabled={running} />
    </div>
    <div class="form-group">
      <label for="lt-bursts">Bursts</label>
      <input id="lt-bursts" type="number" min="1" max="500" step="1" bind:value={bursts} disabled={running} />
    </div>
    <div class="form-group">
      <label for="lt-gap">Pause between (s)</label>
      <input id="lt-gap" type="number" min="0" max="60" step="1" bind:value={gap} disabled={running} />
    </div>
    <div class="form-group">
      <label for="lt-label">Label</label>
      <input id="lt-label" type="text" maxlength="80" placeholder="boombox 2, volume 60%" bind:value={label} disabled={running} />
    </div>
  </div>

  {#if remote}
    <div class="status-msg info">
      A test is running: {remote.label || 'unlabelled'} · board #{remote.boardId} at {remote.distance} m ·
      burst {remote.done} of {remote.requested}. It was started in another browser or before this page
      was opened, so its readings show up under Saved runs once it ends.
    </div>
  {/if}

  <div class="btn-row">
    {#if running || remote}
      <button class="btn btn-danger" on:click={endTest} disabled={ending}>
        {ending ? 'Ending after this burst…' : 'End test'}
      </button>
      {#if running}
        <span class="progress">Burst {run.bursts.length + 1} of {run.requested}…</span>
      {/if}
    {:else}
      <button class="btn btn-primary" on:click={start}>Start</button>
    {/if}
    {#if run?.bursts?.length}
      <button class="btn btn-ghost btn-sm" on:click={() => saveJson([run], `latency-${stamp(run)}`)}>Download JSON</button>
      <button class="btn btn-ghost btn-sm" on:click={() => saveCsv([run], `latency-${stamp(run)}`)}>Download CSV</button>
    {/if}
  </div>

  {#if run && !running && run.bursts.length}
    <div class="shown">
      Showing: {run.label || 'unlabelled'} · board #{run.boardId} at {run.distance} m · {when(run.started)}{run.stopped ? ' · stopped' : ''}
    </div>
  {/if}

  {#if s?.measured}
    <div class="summary">
      <div><span>Bursts measured</span><b>{s.measured} / {s.bursts}</b></div>
      <div><span>Delay</span><b>{num(s.meanDelayMs, 3)} ± {num(s.sdDelayMs, 3)} ms</b><em>{cm(s.meanDelayMs)} ± {cm(s.sdDelayMs)} cm</em></div>
      <div><span>Range of burst averages</span><b>{num(s.minDelayMs, 3)} – {num(s.maxDelayMs, 3)} ms</b><em>{s.rangeCm} cm</em></div>
      <div><span>Trend over the run</span><b>{num(s.trendMsPerMin, 3)} ms/min</b><em>{cm(s.trendMsPerMin)} cm/min</em></div>
      <div><span>Drift within a burst</span><b>{s.meanDriftPpm ?? '—'} ± {s.sdDriftPpm ?? '—'} ppm</b><em>µs of delay gained per second</em></div>
      <div><span>Scatter within a burst</span><b>{num(s.meanJitterMs, 3)} ms</b><em>{cm(s.meanJitterMs)} cm around the burst's line</em></div>
    </div>
    {#if v}
      <div class="status-msg" class:success={v.ok} class:error={!v.ok}>{v.text}</div>
    {/if}
  {/if}

  {#if chart}
    <svg class="chart" viewBox="0 0 {W} {H}" role="img" aria-label="Delay per chirp over time">
      <line x1={LEFT} y1={H - BOTTOM} x2={W - RIGHT} y2={H - BOTTOM} class="axis-line" />
      <line x1={LEFT} y1={TOP} x2={LEFT} y2={H - BOTTOM} class="axis-line" />
      <text x={LEFT - 4} y={TOP + 8} class="axis" text-anchor="end">{chart.y1.toFixed(2)}</text>
      <text x={LEFT - 4} y={H - BOTTOM} class="axis" text-anchor="end">{chart.y0.toFixed(2)}</text>
      <text x={4} y={(H - BOTTOM) / 2} class="axis">ms</text>
      <text x={LEFT} y={H - 4} class="axis">0:00</text>
      <text x={W - RIGHT} y={H - 4} class="axis" text-anchor="end">{clock(chart.span)}</text>
      {#each chart.dots as [x, y]}
        <circle cx={x} cy={y} r="1.8" class="dot" />
      {/each}
      {#if chart.line}
        <polyline points={chart.line} class="mean" />
      {/if}
    </svg>
    <div class="legend">Dots: every chirp's delay · line: each burst's average</div>
  {/if}

  {#if run?.bursts?.length}
    <label class="all-toggle"><input type="checkbox" bind:checked={showAll} /> show every chirp</label>
    <div class="table-wrap">
      <table class="grid">
        <thead>
          <tr><th>#</th><th>Time</th><th>Delay ms</th><th>cm</th><th>Drift ppm</th><th>Scatter ms</th><th>Heard</th><th>Status</th></tr>
        </thead>
        <tbody>
          {#each [...run.bursts].reverse() as b (b.n)}
            <tr class:bad={b.meanDelayMs == null} class="row" role="button" tabindex="0"
                on:click={() => toggleBurst(b.n)} on:keydown={(e) => e.key === 'Enter' && toggleBurst(b.n)}>
              <td>{b.n}</td>
              <td>{clock(b.t)}</td>
              <td>{num(b.meanDelayMs, 3)}</td>
              <td>{cm(b.meanDelayMs)}</td>
              <td>{b.driftPpm ?? '—'}</td>
              <td>{num(b.jitterMs, 3)}</td>
              <td>{b.heard}/{b.chirpsSent ?? 10}</td>
              <td>{b.status}{b.reason ? ` — ${b.reason}` : ''}{b.source === 'client_clap' ? ' (from per-chirp lines)' : ''}</td>
            </tr>
            {#if showAll || openBurst === b.n}
              <tr class="detail">
                <td colspan="8">
                  {#each b.d ?? [] as m, i}
                    <span class="chirp">#{i} {m ? `${m.toFixed(3)} m · ${num(b.delays?.[i], 3)} ms` : '—'}</span>
                  {/each}
                </td>
              </tr>
            {/if}
          {/each}
        </tbody>
      </table>
    </div>
  {/if}

  {#if pastRuns.length}
    <div class="past-head">
      <span>Saved runs</span>
      <button class="btn btn-ghost btn-sm" on:click={() => saveJson([...pastRuns].reverse(), 'latency-runs')}>All as JSON</button>
      <button class="btn btn-ghost btn-sm" on:click={() => saveCsv([...pastRuns].reverse(), 'latency-runs')}>All as CSV</button>
      <button class="btn btn-ghost btn-sm" on:click={clearRuns} disabled={running}>Clear</button>
    </div>
    <div class="table-wrap">
      <table class="grid">
        <thead>
          <tr><th>Started</th><th>Label</th><th>Board</th><th>m</th><th>Bursts</th><th>Delay ms</th><th>Range cm</th><th>Trend ms/min</th><th>Drift ppm</th><th></th></tr>
        </thead>
        <tbody>
          {#each pastRuns as r (r.started)}
            <tr>
              <td>{when(r.started)}</td>
              <td>{r.label || '—'}{r.stopped ? ' (stopped)' : ''}</td>
              <td>#{r.boardId}</td>
              <td>{r.distance}</td>
              <td>{r.summary?.measured ?? 0}/{r.summary?.bursts ?? 0}</td>
              <td>{num(r.summary?.meanDelayMs, 3)}</td>
              <td>{r.summary?.rangeCm ?? '—'}</td>
              <td>{num(r.summary?.trendMsPerMin, 3)}</td>
              <td>{r.summary?.meanDriftPpm ?? '—'}</td>
              <td class="nowrap">
                <button class="btn btn-ghost btn-sm" on:click={() => { run = r; openBurst = null; }} disabled={running}>Show</button>
                <button class="btn btn-ghost btn-sm" on:click={() => saveCsv([r], `latency-${stamp(r)}`)}>CSV</button>
              </td>
            </tr>
          {/each}
        </tbody>
      </table>
    </div>
  {/if}
</div>

<style>
  .hint {
    font-size: 0.82rem;
    color: var(--color-text-muted);
    margin: 0 0 0.9rem;
    line-height: 1.45;
  }

  .form-row select,
  .form-row input {
    width: 100%;
  }

  .progress,
  .shown,
  .legend {
    font-size: 0.8rem;
    color: var(--color-text-muted);
  }

  .progress {
    align-self: center;
  }

  .shown {
    margin-top: 0.75rem;
  }

  .summary {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(180px, 1fr));
    gap: 0.6rem;
    margin: 0.9rem 0;
  }

  .summary div {
    background: var(--color-surface);
    border: 1px solid var(--color-border);
    border-radius: 6px;
    padding: 0.5rem 0.65rem;
    display: flex;
    flex-direction: column;
    gap: 0.15rem;
  }

  .summary span {
    font-size: 0.72rem;
    color: var(--color-text-muted);
  }

  .summary b {
    font-size: 0.95rem;
    font-variant-numeric: tabular-nums;
  }

  .summary em {
    font-size: 0.72rem;
    font-style: normal;
    color: var(--color-text-muted);
  }

  .chart {
    width: 100%;
    height: auto;
    margin-top: 0.75rem;
    background: var(--color-surface);
    border: 1px solid var(--color-border);
    border-radius: 6px;
  }

  .axis {
    font-size: 10px;
    fill: var(--color-text-muted);
  }

  .axis-line {
    stroke: var(--color-border);
    stroke-width: 1;
  }

  .dot {
    fill: var(--color-mid);
    opacity: 0.75;
  }

  .mean {
    fill: none;
    stroke: var(--color-accent);
    stroke-width: 1.5;
  }

  .all-toggle {
    display: flex;
    align-items: center;
    gap: 0.4rem;
    font-size: 0.8rem;
    color: var(--color-text-muted);
    margin: 0.9rem 0 0.4rem;
  }

  .table-wrap {
    overflow-x: auto;
    max-height: 420px;
    overflow-y: auto;
    border: 1px solid var(--color-border);
    border-radius: 6px;
  }

  .grid {
    width: 100%;
    border-collapse: collapse;
    font-size: 0.78rem;
    font-variant-numeric: tabular-nums;
  }

  .grid th,
  .grid td {
    padding: 0.3rem 0.5rem;
    text-align: left;
    border-bottom: 1px solid var(--color-border);
    white-space: nowrap;
  }

  .grid th {
    position: sticky;
    top: 0;
    background: var(--color-surface);
    color: var(--color-text-muted);
    font-weight: 600;
  }

  .row {
    cursor: pointer;
  }

  .row.bad td {
    color: var(--color-low);
  }

  .detail td {
    white-space: normal;
    background: rgba(255, 255, 255, 0.03);
  }

  .chirp {
    display: inline-block;
    margin: 0 0.9rem 0.2rem 0;
    font-family: monospace;
    font-size: 0.74rem;
  }

  .past-head {
    display: flex;
    align-items: center;
    gap: 0.5rem;
    margin: 1.1rem 0 0.4rem;
    font-size: 0.85rem;
  }

  .past-head span {
    margin-right: auto;
    color: var(--color-text-muted);
  }

  .nowrap {
    white-space: nowrap;
  }
</style>
