// On page load, fetch current darkroom params and update UI

document.addEventListener('DOMContentLoaded', () => {
  // Match updated HTML IDs
  const strobeEl = document.getElementById('timeframeRange');
  const strobeLabel = document.getElementById('rangeValue');

  const candleChk = document.getElementById('candlelight');
  const candleEl = document.getElementById('candleBrightness');
  const candleLabel = document.getElementById('candleBrightnessValue');

  const redChk = document.getElementById('redlight');
  const redEl = document.getElementById('redlight_value');
  const redLabel = document.getElementById('redlightValue');

  const submitBtn = document.getElementById('submit_settingsDarkroom');

  // Brightness slider (UI-only): match HTML ids
  const brightnessEl = document.getElementById('brightnessVal');
  const brightnessLabel = document.getElementById('brightnessValValue');

  // Util: create or update a noUiSlider
  function ensureRangeSlider(el, startMin, startMax, min = 0, max = 1023, step = 1, labelEl) {
    if (!el) return;
    if (!window.noUiSlider) {
      console.error('noUiSlider not loaded');
      return;
    }
    if (!el.noUiSlider) {
      window.noUiSlider.create(el, {
        start: [startMin, startMax],
        connect: true,
        range: { min, max },
        step,
        tooltips: [true, true],
        format: {
          to: v => Math.round(Number(v)),
          from: v => Number(v)
        }
      });
      el.noUiSlider.on('update', (values) => {
        if (labelEl) labelEl.textContent = `${values[0]} - ${values[1]}`;
      });
    } else {
      el.noUiSlider.updateOptions({
        start: [startMin, startMax],
        range: { min, max },
        step
      }, true);
      if (labelEl) labelEl.textContent = `${startMin} - ${startMax}`;
    }
  }

  // Fetch and hydrate current darkroom params
  function hydrate() {
    fetch('/getDarkroomParams')
      .then(r => r.json())
      .then(data => {
        // Expecting: strobeMin, strobeMax, redlightMin, redlightMax, candlelightMin, candlelightMax
        const sMin = Number(data.strobeMin ?? 1);
        const sMax = Number(data.strobeMax ?? 120);
        const rMin = Number(data.redlightMin ?? 10);
        const rMax = Number(data.redlightMax ?? 255);
        const cMin = Number(data.candlelightMin ?? 51);
        const cMax = Number(data.candlelightMax ?? 255);

        // Timeframe (1–120)
        ensureRangeSlider(strobeEl, Math.max(1, sMin), Math.min(120, sMax), 1, 120, 1, strobeLabel);
        // Candle brightness (51–255)
        ensureRangeSlider(candleEl, Math.max(51, cMin), Math.min(255, cMax), 51, 255, 1, candleLabel);
        // Redlight brightness (10–255)
        ensureRangeSlider(redEl, Math.max(10, rMin), Math.min(255, rMax), 10, 255, 1, redLabel);
        // Overall brightness (100–255) — UI only for now
        ensureRangeSlider(brightnessEl, 100, 255, 100, 255, 1, brightnessLabel);

        if (typeof candleChk?.checked !== 'undefined') candleChk.checked = true;
        if (typeof redChk?.checked !== 'undefined') redChk.checked = true;
      })
      .catch(e => console.error('Failed to fetch darkroom params:', e));
  }

  // Submit current UI state (uses timeframe, candle, red)
  function submit() {
    const [sMin, sMax] = strobeEl?.noUiSlider ? strobeEl.noUiSlider.get().map(Number) : [1, 120];
    const [cMin, cMax] = candleEl?.noUiSlider ? candleEl.noUiSlider.get().map(Number) : [51, 255];
    const [rMin, rMax] = redEl?.noUiSlider ? redEl.noUiSlider.get().map(Number) : [10, 255];
    const redChkEnabled = redChk?.checked ?? true;
    const candleChkEnabled = candleChk?.checked ?? true;
    const url = `/setDarkroomParams?strobeMin=${encodeURIComponent(sMin)}&strobeMax=${encodeURIComponent(sMax)}&redlightMin=${encodeURIComponent(rMin)}&redlightMax=${encodeURIComponent(rMax)}&candlelightMin=${encodeURIComponent(cMin)}&candlelightMax=${encodeURIComponent(cMax)}&redLightEnabled=${encodeURIComponent(redChkEnabled)}&candleLightEnabled=${encodeURIComponent(candleChkEnabled)} `;
    console.log('Submitting darkroom params:', url);
    fetch(url)
      .then(r => r.text())
      .then(txt => {
        console.log('SetDarkroomParams:', txt);
        hydrate(); // Refresh UI after successful submit
      })
      .catch(e => console.error('Failed to set darkroom params:', e));
  }

  // Enable/disable sliders with checkboxes (UI only)
  function setSliderDisabled(sliderDiv, disabled) {
    if (!sliderDiv || !sliderDiv.noUiSlider) return;
    if (disabled) sliderDiv.setAttribute('disabled', true);
    else sliderDiv.removeAttribute('disabled');
  }

  candleChk?.addEventListener('change', () => setSliderDisabled(candleEl, !candleChk.checked));
  redChk?.addEventListener('change', () => setSliderDisabled(redEl, !redChk.checked));

  submitBtn?.addEventListener('click', submit);

  hydrate();
});