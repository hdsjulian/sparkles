<script>
  import { onMount } from 'svelte';

  let songs = [];
  let loading = true;
  let error = '';
  let nowPlaying = '';
  let starting = '';

  function prettify(filename) {
    return filename
      .replace(/\.(mid|midi)$/i, '')
      .replace(/[_-]+/g, ' ')
      .replace(/\b\w/g, (c) => c.toUpperCase());
  }

  async function loadSongs() {
    loading = true;
    error = '';
    try {
      const res = await fetch('/keyboard/songs');
      const data = await res.json();
      songs = data.songs ?? [];
    } catch (_) {
      error = 'Could not load songs';
    }
    loading = false;
  }

  async function play(file) {
    starting = file;
    error = '';
    try {
      const res = await fetch(`/keyboard/play?song=${encodeURIComponent(file)}`, { method: 'POST' });
      if (res.ok) nowPlaying = prettify(file);
      else error = 'Could not start';
    } catch (_) {
      error = 'Could not start';
    }
    starting = '';
  }

  async function stop() {
    try { await fetch('/keyboard/stop', { method: 'POST' }); } catch (_) {}
    nowPlaying = '';
  }

  onMount(loadSongs);
</script>

<svelte:head><title>Karaoke</title></svelte:head>

<div class="stage">
  <h1>Karaoke</h1>

  {#if loading}
    <p class="state">loading…</p>
  {:else if error && songs.length === 0}
    <p class="state">{error}</p>
  {:else if songs.length === 0}
    <p class="state">no songs</p>
  {:else}
    <ul class="songs">
      {#each songs as file}
        <li>
          <button
            class="song"
            class:active={nowPlaying === prettify(file)}
            disabled={starting === file}
            on:click={() => play(file)}>
            <span class="title">{prettify(file)}</span>
            {#if starting === file}<span class="hint">…</span>{/if}
          </button>
        </li>
      {/each}
    </ul>
  {/if}
</div>

{#if nowPlaying}
  <div class="bar">
    <div class="eq" aria-hidden="true"><span></span><span></span><span></span></div>
    <span class="bar-text">{nowPlaying}</span>
    <button class="stop" on:click={stop}>stop</button>
  </div>
{/if}

<style>
  /* night / low-emission: pure black, dim amber, no glows, minimal lit area.
     amber (not white/blue) preserves dark-adapted vision and reads with little
     light. brightness is carried by thin strokes on black, so most of the
     panel stays unlit. */
  :global(html), :global(body) { margin: 0; background: #000; }

  :root {
    --amber: #b9762f;        /* primary text */
    --amber-dim: #7a4d20;    /* secondary / borders */
    --amber-hi: #d68a3a;     /* active accent, used sparingly */
  }

  .stage {
    min-height: 100vh;
    box-sizing: border-box;
    background: #000;
    color: var(--amber);
    padding: 1.5rem 1rem 5.5rem;
    font-family: system-ui, -apple-system, "Segoe UI", sans-serif;
  }

  h1 {
    margin: 0 0 1.25rem;
    text-align: center;
    font-size: 1.7rem;
    font-weight: 600;
    letter-spacing: 0.12em;
    text-transform: lowercase;
    color: var(--amber-dim);
  }

  .songs {
    list-style: none;
    margin: 0 auto;
    padding: 0;
    max-width: 30rem;
    display: flex;
    flex-direction: column;
    gap: 0.7rem;
  }

  .song {
    display: flex;
    align-items: center;
    gap: 0.6rem;
    width: 100%;
    box-sizing: border-box;
    min-height: 64px;
    padding: 0.75rem 1.1rem;
    text-align: left;
    font-size: 1.35rem;
    font-weight: 600;
    color: var(--amber);
    background: #000;                          /* unlit */
    border: 1px solid var(--amber-dim);
    border-radius: 12px;
    cursor: pointer;
    transition: border-color 0.15s, color 0.15s;
  }
  .song:active { border-color: var(--amber); }
  .song:disabled { opacity: 0.5; cursor: default; }
  .song.active {
    color: var(--amber-hi);
    border-color: var(--amber-hi);
  }
  .title { flex: 1; }
  .hint { opacity: 0.7; }

  .state {
    text-align: center;
    color: var(--amber-dim);
    font-size: 1.1rem;
    letter-spacing: 0.1em;
  }

  /* now-playing: a thin dim strip, not a bright bar */
  .bar {
    position: fixed;
    left: 0; right: 0; bottom: 0;
    display: flex;
    align-items: center;
    gap: 0.8rem;
    padding: 0.7rem 1rem;
    background: #000;
    border-top: 1px solid var(--amber-dim);
  }
  .bar-text {
    flex: 1;
    color: var(--amber);
    font-size: 1.15rem;
    font-weight: 600;
    overflow: hidden;
    white-space: nowrap;
    text-overflow: ellipsis;
  }

  .eq { display: flex; align-items: flex-end; gap: 2px; height: 18px; }
  .eq span {
    width: 3px;
    height: 100%;
    background: var(--amber);
    transform-origin: bottom;
    animation: eq 0.9s ease-in-out infinite;
  }
  .eq span:nth-child(2) { animation-delay: 0.3s; }
  .eq span:nth-child(3) { animation-delay: 0.15s; }
  @keyframes eq {
    0%, 100% { transform: scaleY(0.25); }
    50%      { transform: scaleY(1); }
  }

  .stop {
    flex: none;
    padding: 0.55rem 1.2rem;
    font-size: 1rem;
    font-weight: 600;
    color: var(--amber);
    background: #000;
    border: 1px solid var(--amber-dim);
    border-radius: 10px;
    cursor: pointer;
  }
  .stop:active { border-color: var(--amber); }

  @media (prefers-reduced-motion: reduce) {
    .eq span { animation: none; transform: scaleY(0.5); }
  }
</style>
