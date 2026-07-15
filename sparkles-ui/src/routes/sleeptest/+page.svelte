<script>
  import { onMount, onDestroy } from 'svelte';
  import { runSleepTest, cancelSleepTest } from '$lib/api.js';

  let sleepSeconds = 15;
  let phaseSeconds = 60;
  let running = false;
  let phase = 'idle'; // idle | resync | sleeping | waking | done
  let result = null;  // final sleep_test_done payload
  let report = null;  // assembled run summary, rendered once the test ends
  let expected = 0;
  let returned = 0;
  let cyclesExpected = 0;
  let cycle = 0;
  let log = [];
  let boards = new Map(); // id -> { status, wokeEarly, missing }
  let error = '';
  let es;

  function addLog(text) {
    log = [...log, `${new Date().toLocaleTimeString()}  ${text}`].slice(-300);
  }

  function board(id) {
    if (!boards.has(id)) boards.set(id, { status: '?', wokeEarly: false, missing: false });
    return boards.get(id);
  }

  const handlers = {
    sleep_test_busy: () => {
      running = false;
      addLog('⚠ a sleep test is already running — cancel it first');
    },
    sleep_test_start: (d) => {
      running = true; phase = 'resync'; result = null;
      returned = 0; cycle = 0; cyclesExpected = 0;
      expected = d.clients;
      report = { clients: d.clients, sleepS: d.sleep_duration_s, phaseS: d.phase_duration_s,
                 resyncS: null, cyclesExpected: 0, cycles: 0, broadcasts: 0, phaseActualS: null,
                 cancelledAtS: null, earlyWakers: [], wakeTookS: null,
                 returned: 0, expected: d.clients, missing: [], success: false };
      for (const b of boards.values()) { b.wokeEarly = false; b.missing = false; }
      boards = new Map(boards);
      addLog(`test started: ${d.clients} clients, sleep ${d.sleep_duration_s}s, phase ${d.phase_duration_s}s`);
    },
    sleep_test_resync_start: () => { phase = 'resync'; addLog('resyncing clients before sleep…'); },
    sleep_test_resync_done: (d) => {
      if (report) report.resyncS = d.elapsed_ms / 1000;
      addLog(`resync done in ${(d.elapsed_ms / 1000).toFixed(1)}s`);
    },
    sleep_test_broadcast_start: (d) => {
      phase = 'sleeping'; cyclesExpected = d.cycles_expected;
      if (report) { report.cyclesExpected = d.cycles_expected; report.phaseS = d.phase_duration_s; }
      addLog(`sleep broadcast running — ${d.cycles_expected} sleep cycles expected`);
    },
    sleep_test_cycle_heartbeat: (d) => {
      cycle = d.cycle;
      if (report) { report.cycles = d.cycle; report.broadcasts = d.broadcasts; }
      addLog(`cycle ${d.cycle}/${cyclesExpected} — ${d.elapsed_s}s elapsed, ${d.broadcasts} broadcasts sent`);
    },
    sleep_test_unexpected_wakeup: (d) => {
      board(d.id).wokeEarly = true;
      boards = new Map(boards);
      if (report) report.earlyWakers.push({ id: d.id, atS: d.elapsed_ms / 1000 });
      addLog(`⚠ board ${d.id} was awake mid-sleep at ${(d.elapsed_ms / 1000).toFixed(0)}s`);
    },
    sleep_test_cancelled: (d) => {
      if (report) report.cancelledAtS = d.elapsed_ms / 1000;
      addLog(`test cancelled at ${(d.elapsed_ms / 1000).toFixed(0)}s — waking clients…`);
    },
    sleep_test_broadcast_end: (d) => {
      if (report) { report.broadcasts = d.broadcasts; report.phaseActualS = d.elapsed_ms / 1000; }
      addLog(`sleep phase over after ${(d.elapsed_ms / 1000).toFixed(0)}s`);
    },
    sleep_test_waiting_for_wakeup: () => { phase = 'waking'; addLog('waiting for clients to wake up…'); },
    sleep_test_client_back: (d) => {
      returned = d.returned; expected = d.expected;
      if (report) report.wakeTookS = d.elapsed_ms / 1000;
      addLog(`${d.returned}/${d.expected} clients back after ${(d.elapsed_ms / 1000).toFixed(1)}s`);
    },
    sleep_test_done: (d) => {
      running = false; phase = 'done'; result = d;
      returned = d.returned; expected = d.expected;
      for (const id of d.missing_ids ?? []) board(id).missing = true;
      boards = new Map(boards);
      if (report) {
        report.returned = d.returned;
        report.expected = d.expected;
        report.missing = d.missing_ids ?? [];
        report.success = !!d.success;
        report = { ...report };
      }
      addLog(d.success
        ? `✓ success — all ${d.returned} clients woke up correctly`
        : `✗ ${d.returned}/${d.expected} returned — missing: ${(d.missing_ids ?? []).join(', ')}`);
    },
  };

  onMount(() => {
    es = new EventSource('/events');
    for (const name of Object.keys(handlers)) {
      es.addEventListener(name, (e) => { try { handlers[name](JSON.parse(e.data)); } catch {} });
    }
    es.addEventListener('update_board', (e) => {
      try {
        const d = JSON.parse(e.data);
        board(d.id).status = d.status;
        boards = new Map(boards);
      } catch {}
    });
  });

  onDestroy(() => es?.close());

  async function start() {
    error = '';
    log = [];
    try {
      await runSleepTest({ sleepSeconds, phaseSeconds });
      running = true;
      addLog(`sleep test requested (sleep ${sleepSeconds}s, phase ${phaseSeconds}s)`);
    } catch (e) {
      error = e.message;
    }
  }

  async function cancel() {
    try {
      await cancelSleepTest();
      addLog('cancel requested — ending phase, wake-up sequence follows');
    } catch (e) {
      error = e.message;
    }
  }

  function boardLabel(b) {
    if (b.missing) return '✗ missing';
    // during the phase we hold the sent-sleep assumption; only a message from the
    // board (flagged by the master) contradicts it
    if (phase === 'sleeping') return b.wokeEarly ? '⚠ woke early' : '💤 sleeping';
    if (phase === 'waking' || phase === 'done') return b.status === 'active' ? '✓ awake' : '… waiting';
    return b.status;
  }

  function boardClass(b) {
    if (b.missing || (phase === 'sleeping' && b.wokeEarly)) return 'bad';
    if (phase === 'sleeping') return 'ok';
    if ((phase === 'waking' || phase === 'done') && b.status === 'active') return 'ok';
    return '';
  }

  $: boardRows = [...boards.entries()].sort((a, b) => a[0] - b[0]);
  $: phaseLabel = {
    idle: 'Idle',
    resync: 'Resyncing clients…',
    sleeping: `Sleeping — cycle ${cycle}/${cyclesExpected}`,
    waking: `Waking up — ${returned}/${expected} back`,
    done: result?.success ? '✓ Done — all clients returned' : `✗ Done — ${returned}/${expected} returned`,
  }[phase];
  $: minPhase = sleepSeconds * 2 + 5;
</script>

<div class="page-content">
  <h1 class="page-title">Sleep Test</h1>
  <p class="hint">
    Resyncs all clients, puts them to sleep in cycles for the test phase, then verifies every
    client wakes up and reports back. Boards are assumed sleeping once the command is sent —
    any message from a board mid-phase flags it as awake.
  </p>

  {#if error}
    <div class="status-msg error">{error}</div>
  {/if}

  <div class="card" style="margin-bottom:1.25rem;">
    <div class="controls">
      <label>
        Sleep duration (s)
        <input type="number" min="5" max="3600" bind:value={sleepSeconds} disabled={running} />
      </label>
      <label>
        Test phase (s)
        <input type="number" min="10" max="86400" bind:value={phaseSeconds} disabled={running} />
      </label>
      <button class="btn btn-primary" on:click={start} disabled={running}>
        {running ? 'Test running…' : 'Start Sleep Test'}
      </button>
      <button class="btn btn-ghost" on:click={cancel}>
        Cancel
      </button>
    </div>
    {#if phaseSeconds < minPhase}
      <p class="hint" style="margin-top:0.5rem;">
        Phase will be extended to {minPhase}s (minimum two sleep cycles).
      </p>
    {/if}
  </div>

  <div class="card" style="margin-bottom:1.25rem;">
    <div class="card-title">Status</div>
    <div class="phase-label" class:running>{phaseLabel}</div>

    {#if boardRows.length > 0}
      <table>
        <thead>
          <tr><th>Board</th><th>State</th></tr>
        </thead>
        <tbody>
          {#each boardRows as [id, b]}
            <tr class={boardClass(b)}>
              <td>#{id}</td>
              <td>{boardLabel(b)}</td>
            </tr>
          {/each}
        </tbody>
      </table>
    {/if}
  </div>

  {#if phase === 'done' && report}
    <div class="card" style="margin-bottom:1.25rem;">
      <div class="card-title">Report</div>
      <ul class="report">
        <li>{report.expected} clients, {report.sleepS}s naps, {Math.round(report.phaseActualS ?? report.phaseS)}s sleep phase
            {report.cyclesExpected ? `(${report.cyclesExpected} cycles planned)` : ''}</li>
        {#if report.resyncS != null}
          <li>resync before sleep took {report.resyncS.toFixed(1)}s</li>
        {/if}
        {#if report.cancelledAtS != null}
          <li class="warn">⚠ cancelled after {Math.round(report.cancelledAtS)}s ({report.broadcasts} sleep broadcasts sent)</li>
        {:else}
          <li>full phase completed — {report.broadcasts} sleep broadcasts, {report.cycles}/{report.cyclesExpected} cycles</li>
        {/if}
        <li class={report.earlyWakers.length ? 'warn' : ''}>
          {report.earlyWakers.length === 0
            ? 'no board was awake mid-sleep'
            : `awake mid-sleep: ${report.earlyWakers.map((w) => `#${w.id} at ${Math.round(w.atS)}s`).join(', ')}`}
        </li>
        <li>wake-up: {report.returned}/{report.expected} boards back{report.wakeTookS != null ? `, slowest after ${report.wakeTookS.toFixed(1)}s` : ''}</li>
        <li class={report.success ? 'good' : 'bad'}>
          {report.success
            ? '✓ every board slept and woke up correctly'
            : `✗ did not return: ${report.missing.map((i) => '#' + i).join(', ') || 'unknown'}`}
        </li>
      </ul>
    </div>
  {/if}

  <div class="card">
    <div class="card-title">Timeline</div>
    {#if log.length === 0}
      <p class="hint">No events yet — start a test.</p>
    {:else}
      <div class="timeline">
        {#each log as line}
          <div class="timeline-line">{line}</div>
        {/each}
      </div>
    {/if}
  </div>
</div>

<style>
  .hint { color: var(--color-text-muted, #888); font-size: 0.85rem; margin-bottom: 1rem; }

  .controls {
    display: flex;
    align-items: flex-end;
    gap: 1.25rem;
    flex-wrap: wrap;
  }

  .controls label {
    display: flex;
    flex-direction: column;
    gap: 0.3rem;
    font-size: 0.85rem;
    color: var(--color-text-muted, #888);
  }

  .controls input {
    width: 110px;
    padding: 0.4rem 0.5rem;
    background: #1a1a1a;
    border: 1px solid #444;
    border-radius: 4px;
    color: var(--color-text, #eee);
    font-size: 0.95rem;
  }

  .phase-label {
    font-size: 1.05rem;
    font-weight: 600;
    margin-bottom: 0.75rem;
  }

  .phase-label.running { color: var(--color-accent, #4a9eff); }

  table { width: 100%; border-collapse: collapse; }
  th    { text-align: left; padding: 0.4rem 0.75rem; font-size: 0.8rem;
          color: var(--color-text-muted, #888); border-bottom: 1px solid #333; }
  td    { padding: 0.5rem 0.75rem; font-size: 0.95rem; border-bottom: 1px solid #222; }

  tr.ok  td { color: #4caf50; }
  tr.bad td { color: #f44336; }

  .report {
    list-style: none;
    padding: 0;
    margin: 0;
    font-size: 0.9rem;
    line-height: 1.9;
  }

  .report .good { color: #4caf50; }
  .report .warn { color: #ffc107; }
  .report .bad  { color: #f44336; }

  .timeline {
    font-family: monospace;
    font-size: 0.78rem;
    line-height: 1.6;
    max-height: 320px;
    overflow-y: auto;
  }

  .timeline-line {
    white-space: pre-wrap;
    border-bottom: 1px solid #1a1a1a;
  }
</style>
