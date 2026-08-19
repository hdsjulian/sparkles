<script>
  import { onMount } from 'svelte';
  import { devices, deviceListError } from '$lib/stores.js';
  import { getAddressList, commandBlink, commandSync, removeDevice } from '$lib/api.js';

  let error = '';
  let actionMsg = '';
  let selectedId = null;
  let colourBy = 'battery';   // battery | status | distance

  const VIEW = 1000;          // svg units, square so metres are not distorted
  const PAD = 60;

  onMount(async () => {
    try {
      const data = await getAddressList();
      const list = data.addresses ?? [];
      devices.update(map => {
        const next = new Map(map);
        list.forEach(d => next.set(d.id, d));
        return next;
      });
    } catch (e) {
      error = `Failed to load devices: ${e.message}`;
    }
  });

  // The master speaks lowercase xpos/ypos in both the address dump and the
  // live update_board events.
  const posOf = (d) => ({ x: d.xpos ?? d.xPos ?? 0, y: d.ypos ?? d.yPos ?? 0 });
  const isPlaced = (d) => {
    const { x, y } = posOf(d);
    return x !== 0 || y !== 0;
  };

  $: deviceList = Array.from($devices.values());
  $: placed = deviceList.filter(isPlaced);
  $: unplaced = deviceList.filter(d => !isPlaced(d));

  // Fit the cloud into the square view with one scale for both axes, so the map
  // stays a map: equal distances look equal.
  $: bounds = (() => {
    if (!placed.length) return null;
    const pts = placed.map(posOf);
    const minX = Math.min(...pts.map(p => p.x)), maxX = Math.max(...pts.map(p => p.x));
    const minY = Math.min(...pts.map(p => p.y)), maxY = Math.max(...pts.map(p => p.y));
    const span = Math.max(maxX - minX, maxY - minY, 1);
    const scale = (VIEW - 2 * PAD) / span;
    // centre the shorter axis in the square
    const offsetX = PAD + ((VIEW - 2 * PAD) - (maxX - minX) * scale) / 2;
    const offsetY = PAD + ((VIEW - 2 * PAD) - (maxY - minY) * scale) / 2;
    return { minX, maxX, minY, maxY, span, scale, offsetX, offsetY };
  })();

  // y flipped: svg counts downward, a map does not
  const project = (d) => {
    const { x, y } = posOf(d);
    return {
      cx: bounds.offsetX + (x - bounds.minX) * bounds.scale,
      cy: VIEW - bounds.offsetY - (y - bounds.minY) * bounds.scale
    };
  };

  // a round number of metres that covers about a quarter of the view
  $: scaleBar = (() => {
    if (!bounds) return null;
    const target = bounds.span / 4;
    const nice = [0.5, 1, 2, 5, 10, 20, 50, 100].reduce((best, v) =>
      Math.abs(v - target) < Math.abs(best - target) ? v : best, 1);
    return { metres: nice, length: nice * bounds.scale };
  })();

  function markerClass(d) {
    if (colourBy === 'status') return d.status === 'active' ? 'ok' : 'off';
    if (colourBy === 'distance') return 'neutral';
    const pct = d.batteryPercentage ?? 0;
    if (pct >= 60) return 'ok';
    if (pct >= 25) return 'mid';
    return 'low';
  }

  $: selected = selectedId == null ? null : $devices.get(selectedId);
  $: selectedPos = selected && bounds && isPlaced(selected) ? project(selected) : null;

  function select(d) {
    selectedId = selectedId === d.boardId ? null : d.boardId;
  }

  async function act(fn, label) {
    error = '';
    try {
      await fn();
      actionMsg = label;
      setTimeout(() => { actionMsg = ''; }, 2000);
    } catch (e) {
      error = e.message;
    }
  }

  function handleRemove(d) {
    if (!confirm(`Remove board #${d.boardId}? Boards after it shift down one index. It re-adds itself the next time it announces.`)) return;
    selectedId = null;
    act(() => removeDevice(d.boardId), `Removed #${d.boardId}`);
  }

  const fmtMac = (a) => (typeof a === 'string' ? a.toUpperCase() : '—');
</script>

<div class="page-content">
  <h1 class="page-title">Map</h1>

  {#if error}
    <div class="status-msg error">{error}</div>
  {/if}
  {#if $deviceListError}
    <div class="status-msg error">{$deviceListError}</div>
  {/if}
  {#if actionMsg}
    <div class="status-msg success">{actionMsg}</div>
  {/if}

  <div class="map-toolbar">
    <span class="muted">{placed.length} placed{unplaced.length ? ` · ${unplaced.length} without a position` : ''}</span>
    <div class="spacer"></div>
    <label class="muted" for="colour-by">colour</label>
    <select id="colour-by" bind:value={colourBy}>
      <option value="battery">battery</option>
      <option value="status">active</option>
      <option value="distance">plain</option>
    </select>
  </div>

  {#if !bounds}
    <div class="card empty">
      No positioned boards yet. Run a calibration, then send the positions to the boards.
    </div>
  {:else}
    <div class="map-frame">
      <svg viewBox="0 0 {VIEW} {VIEW}" role="img" aria-label="Board map">
        <!-- click the background to dismiss the popover -->
        <rect x="0" y="0" width={VIEW} height={VIEW} class="backdrop"
              role="presentation" on:click={() => (selectedId = null)} />

        {#if scaleBar}
          <g class="scale-bar">
            <line x1={PAD} y1={VIEW - 24} x2={PAD + scaleBar.length} y2={VIEW - 24} />
            <line x1={PAD} y1={VIEW - 30} x2={PAD} y2={VIEW - 18} />
            <line x1={PAD + scaleBar.length} y1={VIEW - 30} x2={PAD + scaleBar.length} y2={VIEW - 18} />
            <text x={PAD + scaleBar.length / 2} y={VIEW - 34}>{scaleBar.metres} m</text>
          </g>
        {/if}

        {#each placed as d (d.boardId)}
          {@const p = project(d)}
          <g class="marker {markerClass(d)}" class:selected={d.boardId === selectedId}
             role="button" tabindex="0"
             aria-label="Board {d.boardId}"
             on:click={() => select(d)}
             on:keydown={(e) => (e.key === 'Enter' || e.key === ' ') && (e.preventDefault(), select(d))}>
            <!-- generous invisible hit area, fingers are bigger than dots -->
            <circle cx={p.cx} cy={p.cy} r="26" class="hit" />
            {#if d.boardId === selectedId}
              <circle cx={p.cx} cy={p.cy} r="20" class="ring" />
            {/if}
            <circle cx={p.cx} cy={p.cy} r="11" class="dot" />
            <text x={p.cx} y={p.cy - 18} class="label">{d.boardId}</text>
          </g>
        {/each}
      </svg>

      {#if selected && selectedPos}
        <div class="popover"
             style="left: {(selectedPos.cx / VIEW) * 100}%; top: {(selectedPos.cy / VIEW) * 100}%;">
          <div class="pop-head">
            <span class="pop-id">#{selected.boardId}</span>
            <span class="badge" class:badge-active={selected.status === 'active'}
                  class:badge-inactive={selected.status !== 'active'}>
              {selected.status === 'active' ? 'Active' : 'Inactive'}
            </span>
            <button class="pop-close" on:click={() => (selectedId = null)} aria-label="Close">×</button>
          </div>

          <div class="pop-mac">{fmtMac(selected.address)}</div>

          <dl class="pop-info">
            <div><dt>position</dt><dd>{posOf(selected).x.toFixed(2)}, {posOf(selected).y.toFixed(2)} m</dd></div>
            <div><dt>from centre</dt><dd>{(selected.distance ?? 0).toFixed(2)} m</dd></div>
            <div><dt>battery</dt><dd>{selected.batteryPercentage != null ? selected.batteryPercentage.toFixed(1) + '%' : '—'}</dd></div>
            <div><dt>delay / offset</dt><dd>{selected.delay ?? '—'} / {selected.timerOffset ?? '—'}</dd></div>
          </dl>

          <div class="pop-actions">
            <button class="btn btn-ghost btn-sm" on:click={() => act(() => commandBlink(selected.boardId), `Blinking #${selected.boardId}`)}>Blink</button>
            <button class="btn btn-ghost btn-sm" on:click={() => act(() => commandSync(selected.boardId), `Syncing #${selected.boardId}`)}>Sync</button>
            <button class="btn btn-ghost btn-sm danger" on:click={() => handleRemove(selected)}>Remove</button>
          </div>
        </div>
      {/if}
    </div>

    <p class="hint">
      Tap a board to blink it — that is how you check the map against the real space, and how
      you find out whether it came out mirrored.
    </p>
  {/if}

  {#if unplaced.length}
    <div class="card unplaced">
      <div class="card-title">No position yet ({unplaced.length})</div>
      <p class="muted" style="margin-bottom:0.6rem;">
        These never got a position, so they are not on the map. They either missed too many
        chirps or were added after the last calibration.
      </p>
      <div class="chip-row">
        {#each unplaced as d (d.boardId)}
          <button class="chip" on:click={() => act(() => commandBlink(d.boardId), `Blinking #${d.boardId}`)}>
            #{d.boardId} <span class="chip-hint">blink</span>
          </button>
        {/each}
      </div>
    </div>
  {/if}
</div>

<style>
  .map-toolbar {
    display: flex;
    align-items: center;
    gap: 0.6rem;
    margin-bottom: 0.75rem;
    font-size: 0.85rem;
  }

  .map-toolbar .spacer { flex: 1; }
  .muted { color: var(--color-text-muted); }

  .map-frame {
    position: relative;
    width: 100%;
    max-width: 700px;
    aspect-ratio: 1;
    background: rgba(255, 255, 255, 0.03);
    border: 1px solid rgba(255, 255, 255, 0.08);
    border-radius: var(--radius);
    overflow: hidden;
    touch-action: manipulation;
  }

  .map-frame svg { display: block; width: 100%; height: 100%; }

  .backdrop { fill: transparent; }

  .marker { cursor: pointer; outline: none; }
  .marker .hit { fill: transparent; }
  .marker .dot { fill: var(--color-text-muted); transition: r 0.1s ease; }
  .marker.ok .dot  { fill: var(--color-ok, #4fbf7a); }
  .marker.mid .dot { fill: var(--color-mid, #e9a04f); }
  .marker.low .dot { fill: var(--color-low, #f44336); }
  .marker.off .dot { fill: #55606e; }
  .marker.neutral .dot { fill: var(--color-accent, #e94560); }
  .marker:hover .dot, .marker:focus-visible .dot { r: 13; }
  .marker .ring { fill: none; stroke: var(--color-accent, #e94560); stroke-width: 2.5; }
  .marker:focus-visible .ring, .marker:focus-visible .dot { stroke: #fff; }

  .marker .label {
    fill: var(--color-text-muted);
    font-size: 20px;
    text-anchor: middle;
    pointer-events: none;
    user-select: none;
  }

  .scale-bar line { stroke: var(--color-text-muted); stroke-width: 2; }
  .scale-bar text {
    fill: var(--color-text-muted);
    font-size: 20px;
    text-anchor: middle;
  }

  /* anchored at the marker, nudged so it never covers the dot it belongs to */
  .popover {
    position: absolute;
    transform: translate(-50%, calc(-100% - 22px));
    min-width: 220px;
    max-width: min(280px, 90%);
    background: var(--color-surface, #1b2130);
    border: 1px solid rgba(255, 255, 255, 0.14);
    border-radius: var(--radius);
    box-shadow: 0 10px 28px rgba(0, 0, 0, 0.45);
    padding: 0.7rem 0.8rem;
    z-index: 5;
  }

  .pop-head {
    display: flex;
    align-items: center;
    gap: 0.5rem;
    margin-bottom: 0.2rem;
  }

  .pop-id {
    font-size: 1.05rem;
    font-weight: 800;
    color: var(--color-accent);
  }

  .pop-close {
    margin-left: auto;
    background: none;
    border: none;
    color: var(--color-text-muted);
    font-size: 1.2rem;
    line-height: 1;
    cursor: pointer;
    padding: 0 0.15rem;
  }

  .pop-mac {
    font-family: monospace;
    font-size: 0.72rem;
    color: var(--color-text-muted);
    margin-bottom: 0.45rem;
  }

  .pop-info { margin: 0 0 0.6rem; font-size: 0.8rem; }
  .pop-info div { display: flex; justify-content: space-between; gap: 0.75rem; padding: 0.1rem 0; }
  .pop-info dt { color: var(--color-text-muted); }
  .pop-info dd { margin: 0; font-weight: 600; }

  .pop-actions { display: flex; gap: 0.4rem; }
  .pop-actions .btn { flex: 1; }
  .danger { color: var(--color-low, #f44336); border-color: var(--color-low, #f44336); }

  .hint {
    font-size: 0.85rem;
    color: var(--color-text-muted);
    margin-top: 0.6rem;
    max-width: 700px;
  }

  .empty {
    text-align: center;
    color: var(--color-text-muted);
    padding: 2rem;
  }

  .unplaced { margin-top: 1.25rem; max-width: 700px; }

  .chip-row { display: flex; flex-wrap: wrap; gap: 0.4rem; }

  .chip {
    background: rgba(255, 255, 255, 0.05);
    border: 1px solid rgba(255, 255, 255, 0.12);
    border-radius: 999px;
    color: var(--color-text);
    font-size: 0.8rem;
    padding: 0.25rem 0.7rem;
    cursor: pointer;
  }

  .chip:hover { border-color: var(--color-accent); }
  .chip-hint { color: var(--color-text-muted); font-size: 0.7rem; }
</style>
