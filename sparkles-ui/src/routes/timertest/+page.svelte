<script>
  import { onMount, onDestroy } from 'svelte';
  import { commandTimerTest } from '$lib/api.js';

  let results = new Map(); // boardId -> { deltaUs, ts }
  let running = false;
  let error = '';
  let es;

  onMount(() => {
    es = new EventSource('/events');
    es.addEventListener('timer_test_result', (e) => {
      try {
        const d = JSON.parse(e.data);
        results.set(d.boardId, { deltaUs: d.deltaUs, ts: Date.now() });
        results = new Map(results);
      } catch {}
    });
  });

  onDestroy(() => es?.close());

  async function runTest() {
    running = true;
    error = '';
    try {
      await commandTimerTest();
    } catch (e) {
      error = e.message;
    }
    setTimeout(() => { running = false; }, 1000);
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
  <p class="hint">Queries each client for its estimated master time and shows the error.</p>

  <button class="btn" on:click={runTest} disabled={running}>
    {running ? 'Querying…' : 'Run Test'}
  </button>

  {#if error}
    <p class="error">{error}</p>
  {/if}

  {#if rows.length > 0}
    <table>
      <thead>
        <tr><th>Device</th><th>Error</th><th>Status</th></tr>
      </thead>
      <tbody>
        {#each rows as [boardId, r]}
          <tr class={rowClass(r.deltaUs)}>
            <td>#{boardId}</td>
            <td>{formatDelta(r.deltaUs)}</td>
            <td class="dot">
              {#if rowClass(r.deltaUs) === 'ok'}✓{:else if rowClass(r.deltaUs) === 'warn'}~{:else}✗{/if}
            </td>
          </tr>
        {/each}
      </tbody>
    </table>
  {:else}
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

  .dot { font-size: 1.1rem; }
  .error { color: #f44336; font-size: 0.9rem; }
</style>
