<script>
  import { onMount, onDestroy, tick } from 'svelte';

  let lines = [];
  let es;
  let logEl;
  let autoScroll = true;

  onMount(() => {
    es = new EventSource('/serialLog');
    es.addEventListener('serial_log', (e) => {
      lines = [...lines, JSON.parse(e.data)];
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
      <label class="autoscroll-label">
        <input type="checkbox" bind:checked={autoScroll} /> Auto-scroll
      </label>
      <button class="btn btn-ghost" on:click={clear}>Clear</button>
    </div>
  </div>

  <div class="log-box" bind:this={logEl} on:scroll={onScroll}>
    {#each lines as line}
      <div class="log-line">{line}</div>
    {/each}
    {#if lines.length === 0}
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

  .log-empty {
    color: var(--color-text-muted);
    font-style: italic;
  }
</style>
