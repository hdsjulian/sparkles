<script>
  // hue: 0–255 (FastLED HSV), saturation: 0–255
  export let hue = 0;
  export let saturation = 255;
  export let brightness = 200; // for preview only

  function hsv2css(h, s, v) {
    // FastLED HSV (0-255) → CSS rgb
    h = (h / 255) * 360;
    s = s / 255;
    v = v / 255;
    const f = (n) => {
      const k = (n + h / 60) % 6;
      return v - v * s * Math.max(0, Math.min(k, 4 - k, 1));
    };
    const r = Math.round(f(5) * 255);
    const g = Math.round(f(3) * 255);
    const b = Math.round(f(1) * 255);
    return `rgb(${r},${g},${b})`;
  }

  $: previewColor = hsv2css(hue, saturation, brightness);

  // Gradient for hue slider: full spectrum at current saturation
  $: hueGradient = (() => {
    const stops = Array.from({ length: 13 }, (_, i) => {
      const h = Math.round(i * 255 / 12);
      return hsv2css(h, saturation, brightness);
    }).join(', ');
    return `linear-gradient(to right, ${stops})`;
  })();

  // Gradient for saturation slider: grey → hue color
  $: satGradient = `linear-gradient(to right, ${hsv2css(hue, 0, brightness)}, ${hsv2css(hue, 255, brightness)})`;
</script>

<div class="hsp-wrap">
  <div class="hsp-swatch" style="background:{previewColor};" title="Preview"></div>
  <div class="hsp-sliders">
    <div class="hsp-row">
      <span class="hsp-label">Hue</span>
      <input class="hsp-slider" type="range" min="0" max="255" bind:value={hue}
        style="--track-bg:{hueGradient}" />
      <input class="hsp-num" type="number" min="0" max="255" bind:value={hue} />
    </div>
    <div class="hsp-row">
      <span class="hsp-label">Sat</span>
      <input class="hsp-slider" type="range" min="0" max="255" bind:value={saturation}
        style="--track-bg:{satGradient}" />
      <input class="hsp-num" type="number" min="0" max="255" bind:value={saturation} />
    </div>
  </div>
</div>

<style>
  .hsp-wrap {
    display: flex;
    align-items: center;
    gap: 0.75rem;
    margin-bottom: 0.5rem;
  }

  .hsp-swatch {
    width: 2.5rem;
    height: 2.5rem;
    border-radius: 6px;
    border: 1px solid var(--color-border, #444);
    flex-shrink: 0;
  }

  .hsp-sliders {
    display: flex;
    flex-direction: column;
    gap: 0.35rem;
    flex: 1;
  }

  .hsp-row {
    display: flex;
    align-items: center;
    gap: 0.5rem;
  }

  .hsp-label {
    font-size: 0.78rem;
    color: var(--color-text-muted);
    width: 2rem;
    flex-shrink: 0;
  }

  .hsp-slider {
    flex: 1;
    -webkit-appearance: none;
    appearance: none;
    height: 10px;
    border-radius: 5px;
    background: var(--track-bg);
    outline: none;
    cursor: pointer;
  }

  .hsp-slider::-webkit-slider-thumb {
    -webkit-appearance: none;
    width: 18px;
    height: 18px;
    border-radius: 50%;
    background: white;
    border: 2px solid #888;
    cursor: pointer;
  }

  .hsp-slider::-moz-range-thumb {
    width: 18px;
    height: 18px;
    border-radius: 50%;
    background: white;
    border: 2px solid #888;
    cursor: pointer;
  }

  .hsp-num {
    width: 3.5rem;
    flex-shrink: 0;
  }
</style>
