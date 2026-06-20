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
      else error = 'Could not start playback';
    } catch (_) {
      error = 'Could not start playback';
    }
    starting = '';
  }

  async function stop() {
    try {
      await fetch('/keyboard/stop', { method: 'POST' });
    } catch (_) {}
    nowPlaying = '';
  }

  onMount(loadSongs);
</script>

<svelte:head><title>Karaoke</title></svelte:head>

<div class="stage">
  <header>
    <h1>Karaoke</h1>
    <p class="sub">Tap a song — the room plays along.</p>
  </header>

  {#if loading}
    <p class="state">Loading songs…</p>
  {:else if error && songs.length === 0}
    <p class="state error">{error}</p>
  {:else if songs.length === 0}
    <p class="state">No songs yet.</p>
  {:else}
    <ul class="songs">
      {#each songs as file, i}
        <li style="--i: {i}">
          <button
            class="song"
            class:active={nowPlaying === prettify(file)}
            disabled={starting === file}
            on:click={() => play(file)}>
            <span class="play-icon" aria-hidden="true"></span>
            <span class="title">{prettify(file)}</span>
            {#if starting === file}<span class="hint">starting…</span>{/if}
          </button>
        </li>
      {/each}
    </ul>
  {/if}

  {#if error && songs.length > 0}
    <p class="state error floating">{error}</p>
  {/if}
</div>

{#if nowPlaying}
  <div class="bar">
    <div class="eq" aria-hidden="true"><span></span><span></span><span></span><span></span></div>
    <div class="bar-text">Now playing <strong>{nowPlaying}</strong></div>
    <button class="stop" on:click={stop}>Stop</button>
  </div>
{/if}

<style>
  :global(body) { margin: 0; }

  .stage {
    min-height: 100vh;
    box-sizing: border-box;
    padding: clamp(2rem, 6vh, 5rem) 1.25rem 9rem;
    color: #f4ede4;
    font-family: system-ui, -apple-system, "Segoe UI", sans-serif;
    /* layered static glow — cheap, no continuous animation */
    background:
      radial-gradient(900px 500px at 50% -10%, rgba(249, 128, 0, 0.18), transparent 60%),
      radial-gradient(700px 600px at 100% 110%, rgba(120, 40, 200, 0.16), transparent 55%),
      radial-gradient(700px 600px at 0% 110%, rgba(0, 120, 200, 0.14), transparent 55%),
      #0a0a0f;
  }

  header {
    max-width: 680px;
    margin: 0 auto clamp(1.5rem, 4vh, 3rem);
    text-align: center;
  }
  h1 {
    margin: 0;
    font-size: clamp(2.4rem, 7vw, 4rem);
    font-weight: 800;
    letter-spacing: 0.04em;
    text-shadow: 0 0 28px rgba(249, 128, 0, 0.45);
  }
  .sub {
    margin: 0.5rem 0 0;
    font-size: clamp(1rem, 2.5vw, 1.25rem);
    opacity: 0.65;
  }

  .songs {
    max-width: 680px;
    margin: 0 auto;
    list-style: none;
    padding: 0;
    display: flex;
    flex-direction: column;
    gap: 1rem;
  }
  .songs li {
    /* one-time staggered entrance */
    animation: rise 0.5s both;
    animation-delay: calc(var(--i) * 60ms);
  }
  @keyframes rise {
    from { opacity: 0; transform: translateY(14px); }
    to   { opacity: 1; transform: none; }
  }

  .song {
    display: flex;
    align-items: center;
    gap: 1.1rem;
    width: 100%;
    box-sizing: border-box;
    min-height: 76px;
    padding: 1rem 1.5rem;
    text-align: left;
    font-size: clamp(1.2rem, 3vw, 1.6rem);
    font-weight: 600;
    color: inherit;
    cursor: pointer;
    border: 1px solid rgba(255, 255, 255, 0.12);
    border-radius: 16px;
    background: rgba(255, 255, 255, 0.045);
    transition: transform 0.12s, background 0.2s, border-color 0.2s, box-shadow 0.2s;
  }
  .song:hover { background: rgba(255, 255, 255, 0.09); }
  .song:active { transform: scale(0.985); }
  .song:disabled { opacity: 0.6; cursor: default; }
  .song.active {
    border-color: rgba(249, 128, 0, 0.8);
    background: rgba(249, 128, 0, 0.14);
    box-shadow: 0 0 24px rgba(249, 128, 0, 0.25);
  }

  .play-icon {
    flex: none;
    width: 0;
    height: 0;
    border-style: solid;
    border-width: 11px 0 11px 18px;
    border-color: transparent transparent transparent #f98000;
    filter: drop-shadow(0 0 6px rgba(249, 128, 0, 0.5));
  }
  .title { flex: 1; }
  .hint { font-size: 0.85rem; font-weight: 500; opacity: 0.7; }

  .state { text-align: center; opacity: 0.6; font-size: 1.1rem; }
  .state.error { color: #ff9d4d; opacity: 1; }
  .floating { margin-top: 1.5rem; }

  /* now-playing bar */
  .bar {
    position: fixed;
    left: 0; right: 0; bottom: 0;
    display: flex;
    align-items: center;
    gap: 1.25rem;
    padding: 1rem clamp(1rem, 4vw, 2rem);
    background: rgba(12, 12, 18, 0.96);
    border-top: 1px solid rgba(249, 128, 0, 0.35);
    box-shadow: 0 -8px 30px rgba(0, 0, 0, 0.5);
  }
  .bar-text {
    flex: 1;
    font-size: clamp(1rem, 2.5vw, 1.25rem);
    color: #f4ede4;
  }
  .bar-text strong { color: #ffb061; }

  .eq { display: flex; align-items: flex-end; gap: 3px; height: 26px; }
  .eq span {
    width: 5px;
    height: 100%;
    background: linear-gradient(#ffb061, #f98000);
    border-radius: 2px;
    transform-origin: bottom;
    animation: eq 0.9s ease-in-out infinite;
  }
  .eq span:nth-child(2) { animation-delay: 0.2s; }
  .eq span:nth-child(3) { animation-delay: 0.45s; }
  .eq span:nth-child(4) { animation-delay: 0.15s; }
  @keyframes eq {
    0%, 100% { transform: scaleY(0.3); }
    50%      { transform: scaleY(1); }
  }

  .stop {
    flex: none;
    padding: 0.7rem 1.6rem;
    font-size: 1.05rem;
    font-weight: 700;
    color: #111;
    background: #f98000;
    border: none;
    border-radius: 12px;
    cursor: pointer;
  }
  .stop:active { transform: scale(0.96); }

  @media (prefers-reduced-motion: reduce) {
    .songs li { animation: none; }
    .eq span { animation: none; transform: scaleY(0.6); }
  }
</style>
