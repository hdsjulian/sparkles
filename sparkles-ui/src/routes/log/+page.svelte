<script>
  import { onMount, onDestroy, tick } from 'svelte';
  import { setLogLevel } from '$lib/api.js';

  let lines = [];
  let es;
  let logEl;
  let autoScroll = true;
  let filter = 'both'; // 'rx' | 'tx' | 'both'
  let masterLogging = true;

  function toggleMasterLog() {
    masterLogging = !masterLogging;
    setLogLevel(masterLogging ? 5 : 0);
  }

  $: visible = lines.filter(l => filter === 'both' || l.dir?.toLowerCase() === filter);

  onMount(() => {
    es = new EventSource('/serialLog');
    es.addEventListener('serial_log', (e) => {
      const entry = JSON.parse(e.data);
      // backwards compat: plain string entries have no dir
      const normalised = typeof entry === 'string'
        ? { dir: 'RX', line: entry }
        : entry;
      lines = [...lines, normalised];
      if (lines.length > 2000) lines = lines.slice(-2000);
      if (autoScroll) tick().then(scrollToBottom);
    });
    es.onerror = () => console.warn('serial log SSE error');
  });

  onDestroy(() => es?.close());

  function scrollToBottom() {
    if (logEl) logEl.scrollTop = logEl.scrollHeight;
  }

  function onScroll() {
    const atBottom = logEl.scrollHeight - logEl.scrollTop - logEl.clientHeight < 40;
    autoScroll = atBottom;
  }

  function clear() { lines = []; }
</script>

<div class="page-content">
  <div class="header-row">
    <h1 class="page-title">Serial Log</h1>
    <div class="header-actions">
      <div class="filter-group">
        <button class="filter-btn" class:active={filter === 'both'} on:click={() => filter = 'both'}>Both</button>
        <button class="filter-btn rx" class:active={filter === 'rx'} on:click={() => filter = 'rx'}>RX</button>
        <button class="filter-btn tx" class:active={filter === 'tx'} on:click={() => filter = 'tx'}>TX</button>
      </div>
      <label class="autoscroll-label">
        <input type="checkbox" bind:checked={autoScroll} /> Auto-scroll
      </label>
      <button class="btn btn-ghost" class:active={masterLogging} on:click={toggleMasterLog}>
        Master log {masterLogging ? 'on' : 'off'}
      </button>
      <button class="btn btn-ghost" on:click={clear}>Clear</button>
    </div>
  </div>

  <div class="log-box" bind:this={logEl} on:scroll={onScroll}>
    {#each visible as entry}
      <div class="log-line" class:rx={entry.dir === 'RX'} class:tx={entry.dir === 'TX'}>
        <span class="dir-tag">{entry.dir ?? 'RX'}</span>{entry.line ?? entry}
      </div>
    {/each}
    {#if visible.length === 0}
      <div class="log-empty">Waiting for serial output...</div>
    {/if}
  </div>
</div>

<style>
  .header-row {
    display: flex;
    align-items: center;
    justify-content: space-between;
    margin-bottom: 1rem;
  }

  .header-actions {
    display: flex;
    align-items: center;
    gap: 1rem;
  }

  .filter-group {
    display: flex;
    gap: 0.25rem;
  }

  .filter-btn {
    padding: 0.3rem 0.75rem;
    border-radius: 4px;
    border: 1px solid #444;
    background: #1a1a1a;
    color: #aaa;
    font-size: 0.8rem;
    cursor: pointer;
  }

  .filter-btn.active {
    background: #333;
    color: #fff;
    border-color: #666;
  }

  .filter-btn.rx.active { border-color: #4a9; color: #4a9; }
  .filter-btn.tx.active { border-color: #a74; color: #a74; }

  .autoscroll-label {
    display: flex;
    align-items: center;
    gap: 0.4rem;
    font-size: 0.85rem;
    color: var(--color-text-muted);
    cursor: pointer;
  }

  .log-box {
    background: #0a0a0a;
    border: 1px solid var(--color-border, #333);
    border-radius: 6px;
    padding: 0.75rem 1rem;
    height: calc(100vh - 160px);
    overflow-y: scroll;
    font-family: monospace;
    font-size: 0.78rem;
    line-height: 1.5;
  }

  .log-line {
    white-space: pre-wrap;
    word-break: break-all;
    color: #c8c8c8;
    border-bottom: 1px solid #1a1a1a;
    padding: 1px 0;
  }

  .log-line.rx .dir-tag { color: #4a9; }
  .log-line.tx .dir-tag { color: #a74; }

  .dir-tag {
    display: inline-block;
    width: 2.5rem;
    font-weight: bold;
    font-size: 0.7rem;
    opacity: 0.8;
    margin-right: 0.4rem;
  }

  .log-empty {
    color: var(--color-text-muted);
    font-style: italic;
  }
</style>
