<script>
  import { onMount, onDestroy } from 'svelte';
  import { commandTimerTest } from '$lib/api.js';

  let samples = new Map(); // boardId -> [{deltaUs, rttUs}]
  let results = new Map(); // boardId -> { bestUs, medianUs, n }
  let known = new Set();   // active boards seen via update_board events
  let missing = [];        // boards that didn't answer the last run
  let lastRun = '';
  let running = false;
  let error = '';
  let es;
  let timer;

  function recompute() {
    const out = new Map();
    for (const [id, list] of samples) {
      // min-rtt sample has the least queueing noise, so its delta is the most honest
      const withRtt = list.filter((s) => s.rttUs != null);
      const best = withRtt.length
        ? withRtt.reduce((a, b) => (b.rttUs < a.rttUs ? b : a))
        : list.reduce((a, b) => (b.deltaUs < a.deltaUs ? b : a));
      const sorted = [...list].map((s) => s.deltaUs).sort((a, b) => a - b);
      out.set(id, { bestUs: best.deltaUs, medianUs: sorted[Math.floor(sorted.length / 2)], n: list.length });
    }
    results = out;
  }

  onMount(() => {
    es = new EventSource('/events');
    es.addEventListener('timer_test_result', (e) => {
      try {
        const d = JSON.parse(e.data);
        if (!samples.has(d.boardId)) samples.set(d.boardId, []);
        samples.get(d.boardId).push({ deltaUs: d.deltaUs, rttUs: d.rttUs ?? null });
        recompute();
        missing = missing.filter((id) => id !== d.boardId);
      } catch {}
    });
    es.addEventListener('update_board', (e) => {
      try {
        const d = JSON.parse(e.data);
        if (d.status === 'active') known.add(d.id);
      } catch {}
    });
  });

  onDestroy(() => { es?.close(); clearTimeout(timer); });

  async function runTest() {
    running = true;
    error = '';
    const expected = new Set([...known, ...results.keys()]);
    samples = new Map();
    results = new Map();
    missing = [];
    try {
      await commandTimerTest();
    } catch (e) {
      error = e.message;
    }
    clearTimeout(timer);
    timer = setTimeout(() => {
      missing = [...expected].filter((id) => !results.has(id)).sort((a, b) => a - b);
      running = false;
      lastRun = new Date().toLocaleTimeString();
    }, 3000);
  }

  function rowClass(deltaUs) {
    const ms = deltaUs / 1000;
    if (ms < 5)  return 'ok';
    if (ms < 20) return 'warn';
    return 'bad';
  }

  function formatDelta(deltaUs) {
    const ms = deltaUs / 1000;
    return ms < 1 ? `${deltaUs} µs` : `${ms.toFixed(1)} ms`;
  }

  $: rows = [...results.entries()].sort((a, b) => a[0] - b[0]);
</script>

<main>
  <h1>Timer Test</h1>
  <p class="hint">Bursts queries at each client and keeps the lowest-noise sample per board.</p>

  <button class="btn" on:click={runTest} disabled={running}>
    {running ? 'Querying…' : 'Run Test'}
  </button>

  {#if lastRun}
    <p class="hint">Last run: {lastRun}</p>
  {/if}

  {#if error}
    <p class="error">{error}</p>
  {/if}

  {#if rows.length > 0 || missing.length > 0}
    <table>
      <thead>
        <tr><th>Device</th><th>Error</th><th>Median</th><th>Samples</th><th>Status</th></tr>
      </thead>
      <tbody>
        {#each rows as [boardId, r]}
          <tr class={rowClass(r.bestUs)}>
            <td>#{boardId}</td>
            <td>{formatDelta(r.bestUs)}</td>
            <td class="muted">{formatDelta(r.medianUs)}</td>
            <td class="muted">{r.n}</td>
            <td class="dot">
              {#if rowClass(r.bestUs) === 'ok'}✓{:else if rowClass(r.bestUs) === 'warn'}~{:else}✗{/if}
            </td>
          </tr>
        {/each}
        {#each missing as boardId}
          <tr class="bad">
            <td>#{boardId}</td>
            <td>no reply</td>
            <td class="muted">—</td>
            <td class="muted">0</td>
            <td class="dot">✗</td>
          </tr>
        {/each}
      </tbody>
    </table>
  {:else if !running}
    <p class="hint">No results yet — press Run Test.</p>
  {/if}
</main>

<style>
  main { max-width: 480px; margin: 2rem auto; padding: 0 1rem; }
  h1   { font-size: 1.4rem; margin-bottom: 0.25rem; }
  .hint { color: var(--color-text-muted, #888); font-size: 0.85rem; margin-bottom: 1.5rem; }

  .btn {
    padding: 0.5rem 1.25rem;
    background: var(--color-accent, #4a9eff);
    color: #fff;
    border: none;
    border-radius: 6px;
    cursor: pointer;
    font-size: 0.95rem;
    margin-bottom: 1.5rem;
  }
  .btn:disabled { opacity: 0.5; cursor: not-allowed; }

  table { width: 100%; border-collapse: collapse; }
  th    { text-align: left; padding: 0.4rem 0.75rem; font-size: 0.8rem;
          color: var(--color-text-muted, #888); border-bottom: 1px solid #333; }
  td    { padding: 0.5rem 0.75rem; font-size: 0.95rem; border-bottom: 1px solid #222; }

  tr.ok   td { color: #4caf50; }
  tr.warn td { color: #ffc107; }
  tr.bad  td { color: #f44336; }

  td.muted { color: var(--color-text-muted, #888); font-size: 0.85rem; }

  .dot { font-size: 1.1rem; }
  .error { color: #f44336; font-size: 0.9rem; }
</style>
