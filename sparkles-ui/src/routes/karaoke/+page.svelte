<script>
  import { onMount } from 'svelte';

  let songs = [];
  let loading = true;
  let error = '';
  let nowPlaying = '';

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
    try {
      const res = await fetch(`/keyboard/play?song=${encodeURIComponent(file)}`, { method: 'POST' });
      if (res.ok) nowPlaying = prettify(file);
      else error = 'Could not start playback';
    } catch (_) {
      error = 'Could not start playback';
    }
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

<main>
  <h1>Karaoke</h1>
  <p class="sub">Pick a song and the keyboard plays it.</p>

  {#if loading}
    <p class="muted">Loading songs…</p>
  {:else if error}
    <p class="error">{error}</p>
  {:else if songs.length === 0}
    <p class="muted">No songs available yet.</p>
  {:else}
    <ul class="songs">
      {#each songs as file}
        <li>
          <button class="song" class:active={nowPlaying === prettify(file)} on:click={() => play(file)}>
            {prettify(file)}
          </button>
        </li>
      {/each}
    </ul>
  {/if}

  {#if nowPlaying}
    <div class="playing">
      <span>♪ Playing <strong>{nowPlaying}</strong></span>
      <button class="stop" on:click={stop}>Stop</button>
    </div>
  {/if}
</main>

<style>
  main {
    max-width: 540px;
    margin: 0 auto;
    padding: 2.5rem 1.25rem 6rem;
    font-family: system-ui, sans-serif;
  }
  h1 {
    font-size: 2rem;
    margin: 0 0 0.25rem;
  }
  .sub {
    margin: 0 0 2rem;
    opacity: 0.7;
  }
  .muted { opacity: 0.6; }
  .error { color: #f98000; }
  .songs {
    list-style: none;
    padding: 0;
    margin: 0;
    display: flex;
    flex-direction: column;
    gap: 0.75rem;
  }
  .song {
    width: 100%;
    text-align: left;
    padding: 1rem 1.25rem;
    font-size: 1.15rem;
    border: 1px solid rgba(255, 255, 255, 0.15);
    border-radius: 10px;
    background: rgba(255, 255, 255, 0.05);
    color: inherit;
    cursor: pointer;
    transition: background 0.15s, border-color 0.15s;
  }
  .song:hover { background: rgba(255, 255, 255, 0.1); }
  .song.active {
    border-color: #f98000;
    background: rgba(249, 128, 0, 0.15);
  }
  .playing {
    position: fixed;
    left: 0;
    right: 0;
    bottom: 0;
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 1rem;
    padding: 1rem 1.5rem;
    background: rgba(20, 20, 20, 0.95);
    border-top: 1px solid rgba(255, 255, 255, 0.15);
  }
  .stop {
    padding: 0.5rem 1.25rem;
    border: none;
    border-radius: 8px;
    background: #f98000;
    color: #111;
    font-weight: 700;
    cursor: pointer;
  }
</style>
