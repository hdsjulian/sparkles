// On page load, fetch current MIDI params and update UI

// Handle MIDI settings submit button
const minRMS = 0.003;
const maxRMS = 1.0;
const submitMidiBtn = document.getElementById('submit_settingsMidi');
if (submitMidiBtn) {
  submitMidiBtn.onclick = function() {
    // Get values from sliders
    let val = 0, saturation = 0, rangeMin = 0, rangeMax = 0;
    let minVal = 0, maxVal = 0, minSat = 0, maxSat = 0;
    if (window.noUiSlider) {
      // Get val
      const midiValSlider = document.getElementById('midiVal');
      if (midiValSlider && midiValSlider.noUiSlider) {
        let valRange = midiValSlider.noUiSlider.get();
        minVal = Number(valRange[0]);
        maxVal = Number(valRange[1]);
        val = (minVal + maxVal) / 2;
      }
      // Get saturation (single value)
      const midiSaturationSlider = document.getElementById('midiSaturation');
      if (midiSaturationSlider && midiSaturationSlider.noUiSlider) {
        let satVal = midiSaturationSlider.noUiSlider.get();
        saturation = Number(Array.isArray(satVal) ? satVal[0] : satVal);
      }
      // Get saturation range (two handles)
      const saturationRangeSlider = document.getElementById('saturationRange');
      if (saturationRangeSlider && saturationRangeSlider.noUiSlider) {
        let satRange = saturationRangeSlider.noUiSlider.get();
        minSat = Number(satRange[0]);
        maxSat = Number(satRange[1]);
      }
      const midiHueEl = document.getElementById('midiHue') || document.getElementById('midiHueSlider');
      let midiHue = 0;
      if (midiHueEl) {
        if (midiHueEl.noUiSlider) {
          midiHue = Number(midiHueEl.noUiSlider.get());
        } else {
          midiHue = Number(midiHueEl.value || 0);
        }
      }
      // Get range
      const instrumentSlider = document.getElementById('instrumentRange');
      if (instrumentSlider && instrumentSlider.noUiSlider) {
        let range = instrumentSlider.noUiSlider.get();
        if (Array.isArray(range)) {
          rangeMin = range[0];
          rangeMax = range[1];
        } else {
          rangeMin = rangeMax = range;
        }
      }
      // Get RMS
      const midiRmsSlider = document.getElementById('midiRms');
      var minRms = minRMS, maxRms = maxRMS;
      if (midiRmsSlider && midiRmsSlider.noUiSlider) {
        let rmsRange = midiRmsSlider.noUiSlider.get();
        minRms = Number(rmsRange[0]);
        maxRms = Number(rmsRange[1]);
      }
      // Get Distance (10–100)
      let distance = undefined;
      const distanceEl = document.getElementById('distance');
      if (distanceEl && distanceEl.noUiSlider) {
        const d = distanceEl.noUiSlider.get();
        distance = Number(Array.isArray(d) ? d[0] : d);
      } else if (distanceEl && typeof distanceEl.value !== 'undefined') {
        distance = Number(distanceEl.value);
      }
      // Get Distance switch
      const distanceSwitchEl = document.getElementById('distanceSwitch');
      const distanceSwitch = distanceSwitchEl ? (distanceSwitchEl.checked ? 1 : 0) : 0;
      // Add mode (0 for midi, 1 for frequency)
      let modeNum = (typeof mode !== 'undefined' && mode === 'frequency') ? 2 : 1;
      // Build fetchUrl with RMS, distance, distanceSwitch, and midiHue
      const fetchUrl = `/setMidiParams?minVal=${encodeURIComponent(minVal)}&maxVal=${encodeURIComponent(maxVal)}&minSat=${encodeURIComponent(minSat)}&maxSat=${encodeURIComponent(maxSat)}&midiSaturation=${encodeURIComponent(saturation)}&midiHue=${encodeURIComponent(midiHue)}&rangeMin=${encodeURIComponent(rangeMin)}&rangeMax=${encodeURIComponent(rangeMax)}&minRms=${encodeURIComponent(minRms)}&maxRms=${encodeURIComponent(maxRms)}${(typeof distance !== 'undefined') ? `&distance=${encodeURIComponent(distance)}` : ''}&distanceSwitch=${distanceSwitch}&mode=${modeNum}`;
      console.log(fetchUrl);
      fetchMe(fetchUrl);
    }
  };
}

const submitPianoButton = document.getElementById('submit_settingsPiano');
if (submitPianoButton) {
  submitPianoButton.onclick = function() {
    const volumeRange  = document.getElementById('volumeRange');
      if (volumeRange && volumeRange.noUiSlider) {
        let valRange = volumeRange.noUiSlider.get();
        minVolume = Number(valRange[0]);
        maxVolume = Number(valRange[1]);
        
      }
      const sustainValue = document.getElementById('sustainValue').value;
    const fetchUrl = '/setPianoSettings?minVolume=' + encodeURIComponent(minVolume) +
                     '&maxVolume=' + encodeURIComponent(maxVolume) +
                      '&sustainValue=' + encodeURIComponent(document.getElementById('sustainValue').value);
   fetchMe(fetchUrl);
  };
}

// Toggle between MIDI and Frequency modes
let mode = "midi";
const switchMidiBtn = document.getElementById('submit_switchMidi');
if (switchMidiBtn) {
  switchMidiBtn.style.backgroundColor = 'green';
  switchMidiBtn.style.fontSize = '1.3em';
  switchMidiBtn.style.padding = '0.7em 2em';
  switchMidiBtn.innerHTML = 'Mode: Midi - Switch to Frequency';
  switchMidiBtn.onclick = function() {
    if (mode === "midi") {
      mode = "frequency";
      switchMidiBtn.innerHTML = 'Mode: Frequency - Switch to MIDI';
      switchMidiBtn.style.backgroundColor = 'red';
    } else {
      mode = "midi";
      switchMidiBtn.innerHTML = 'Mode: Midi<br>Switch to Frequency';
      switchMidiBtn.style.backgroundColor = 'green';
    }
  };
}

// 'use strict';
// import {toggleMenu, fetchData} from "./sparkles.js";
// If you need toggleMenu or fetchData, define them above or ensure sparkles.js is loaded via <script> in HTML.
//toggleMenu();


function statusUpdate(obj) {
  document.getElementById('status').textContent = obj.status;
}









function fetchMe(fetchUrl, callback) {

fetch(fetchUrl)
    .then(response => {
        // Check if the response is successful
        if (!response.ok) {
            throw new Error('Network response was not ok');
        }
        // Parse the response as JSON
        console.log("response ok.");
        return response.text()
    })
    .then(data => {
        // Handle the received data
        if (callback) {
          console.log("callback "+fetchUrl);
          console.log(data);
          var obj = JSON.parse(data);
          callback(obj);
        }
        console.log('Data received:', data);
    })
    .catch(error => {
        // Handle errors
        console.error('Fetch error:', error);
    });
}



// Syncs the value of all sliders with their corresponding span
console.log("Setting up all sliders with value spans");
document.querySelectorAll('input[type="range"]').forEach(function(slider) {
  // Find the next sibling span with a matching id pattern
  let valueSpan = slider.parentElement.querySelector('span');
  if (valueSpan) {
    function updateValue() {
      valueSpan.textContent = slider.value;
    }
    updateValue(); // Set initial value
    slider.addEventListener('input', updateValue);
  }
});






// Instrument frequency ranges in Hz (approximate typical ranges)
const instrumentRanges = {
  clarinet:    [147, 1568],   // D3 - G6
  alto_sax:    [138, 830],    // C#3 - G#5
  tenor_sax:   [103, 698],    // G2 - F5
  flute:       [262, 2093],   // C4 - C7
  trumpet:     [165, 988],     // E3 - B5
  voice_low:   [80, 350],      // Low Range Voice (Bass/Baritone)
  voice_mid:   [150, 700],     // Mid Range Voice (Tenor/Alto)
  voice_high:  [300, 1200]     // High Range Voice (Soprano)
};

// MIDI Hue slider color update
const midiHueEl = document.getElementById('midiHue');
const midiHueValue = document.getElementById('midiHueValue');

// Always create the hue slider on page load
if (window.noUiSlider && midiHueEl && !midiHueEl.noUiSlider) {
  window.noUiSlider.create(midiHueEl, {
    start: [25],
    connect: [true, false],
    range: { 'min': 0, 'max': 255 },
    step: 1,
    tooltips: true,
    format: {
      to: function (value) { return Math.round(value); },
      from: function (value) { return Number(value); }
    }
  });
}

if (midiHueEl && midiHueEl.noUiSlider) {
  let hueVal = midiHueEl.noUiSlider.get();
  if (midiHueValue) midiHueValue.textContent = `${hueVal}`;
  midiHueEl.noUiSlider.on('update', function(values) {
    const v = Number(values);
    if (midiHueValue) midiHueValue.textContent = `${v}`;
  // update background color preview
  const rgb = hsvToRgb(v / 255, 1, 1);
  midiHueEl.style.background = `linear-gradient(to right, rgb(${rgb[0]},${rgb[1]},${rgb[2]}), rgb(${rgb[0]},${rgb[1]},${rgb[2]}))`;
    midiHueEl.value = v;
  });
  midiHueEl.noUiSlider.on('set', function(values) {
    midiHueEl.value = Number(values);
  });
}

function hsvToRgb(h, s, v) {
  let r, g, b;
  let i = Math.floor(h * 6);
  let f = h * 6 - i;
  let p = v * (1 - s);
  let q = v * (1 - f * s);
  let t = v * (1 - (1 - f) * s);
  switch (i % 6) {
    case 0: r = v, g = t, b = p; break;
    case 1: r = q, g = v, b = p; break;
    case 2: r = p, g = v, b = t; break;
    case 3: r = p, g = q, b = v; break;
    case 4: r = t, g = p, b = v; break;
    case 5: r = v, g = p, b = q; break;
  }
  return [Math.round(r * 255), Math.round(g * 255), Math.round(b * 255)];
}

function updateMidiHueDisplay(hue) {
  if (!midiHueEl || !midiHueValue) return;
  const h = Number(hue);
  midiHueValue.textContent = h;
  const rgb = hsvToRgb(h / 255, 1, 1);
  // Paint a simple gradient on the element to give a visual cue
  midiHueEl.style.background = `linear-gradient(to right, #fff, rgb(${rgb[0]},${rgb[1]},${rgb[2]}))`;
}

// Attach event listeners depending on whether we have a noUiSlider or a native input
if (midiHueEl) {
  // If it's a noUiSlider placeholder, we'll initialize it later in fetch('/getMidiParams') block.
  // But support a fallback input[type=range] with id 'midiHueSlider'
  if (midiHueEl.tagName === 'INPUT') {
    midiHueEl.addEventListener('input', function() {
      updateMidiHueDisplay(midiHueEl.value);
    });
    updateMidiHueDisplay(midiHueEl.value || 0);
  }
}

// Fetch MIDI params first, then initialize sliders
fetch('/getMidiParams')
  .then(response => response.json())
  .then(data => {
    // Initialize instrumentRange slider
    var instrumentSlider = document.getElementById('instrumentRange');
    if (window.noUiSlider && instrumentSlider && !instrumentSlider.noUiSlider) {
      window.noUiSlider.create(instrumentSlider, {
        start: [data.rangeMin || 110, data.rangeMax || 800],
        connect: true,
        range: {
          'min': 100,
          'max': 2100
        },
        step: 1,
        tooltips: [true, true],
        format: {
          to: function (value) { return Math.round(value); },
          from: function (value) { return Number(value); }
        }
      });
    }
    // Button event listeners
    if (instrumentSlider && instrumentSlider.noUiSlider) {
      document.getElementById('btn_clarinet').onclick = function() {
        instrumentSlider.noUiSlider.set(instrumentRanges.clarinet);
      };
      document.getElementById('btn_alto_sax').onclick = function() {
        instrumentSlider.noUiSlider.set(instrumentRanges.alto_sax);
      };
      document.getElementById('btn_tenor_sax').onclick = function() {
        instrumentSlider.noUiSlider.set(instrumentRanges.tenor_sax);
      };
      document.getElementById('btn_flute').onclick = function() {
        instrumentSlider.noUiSlider.set(instrumentRanges.flute);
      };
      document.getElementById('btn_trumpet').onclick = function() {
        instrumentSlider.noUiSlider.set(instrumentRanges.trumpet);
      };
      document.getElementById('btn_voice_low').onclick = function() {
        instrumentSlider.noUiSlider.set(instrumentRanges.voice_low);
      };
      document.getElementById('btn_voice_mid').onclick = function() {
        instrumentSlider.noUiSlider.set(instrumentRanges.voice_mid);
      };
      document.getElementById('btn_voice_high').onclick = function() {
        instrumentSlider.noUiSlider.set(instrumentRanges.voice_high);
      };
      instrumentSlider.noUiSlider.on('update', function(values) {
        document.getElementById('rangeValue').textContent = `${values[0]} - ${values[1]}`;
      });
      // Set initial value span
      document.getElementById('rangeValue').textContent = `${instrumentSlider.noUiSlider.get()[0]} - ${instrumentSlider.noUiSlider.get()[1]}`;
    }

    // Initialize midiVal slider as a range (two handles)
    var midiValSlider = document.getElementById('midiVal');
    if (window.noUiSlider && midiValSlider && !midiValSlider.noUiSlider) {
      window.noUiSlider.create(midiValSlider, {
        start: [typeof data.minVal !== 'undefined' ? data.minVal : 40, typeof data.maxVal !== 'undefined' ? data.maxVal : 200],
        connect: true,
        range: {
          'min': 0,
          'max': 255
        },
        step: 1,
        tooltips: [true, true],
        format: {
          to: function (value) { return Math.round(value); },
          from: function (value) { return Number(value); }
        }
      });
    }
    if (midiValSlider && midiValSlider.noUiSlider) {
      // Update value span and also update the variable on change
      let valVals = midiValSlider.noUiSlider.get();
      document.getElementById('midiValValue').textContent = `${valVals[0]} - ${valVals[1]}`;
      midiValSlider.noUiSlider.on('update', function(values) {
        document.getElementById('midiValValue').textContent = `${values[0]} - ${values[1]}`;
        midiValSlider.value = values;
        console.log("Updated midiVal values:", values);
      });
      midiValSlider.noUiSlider.on('set', function(values) {
        midiValSlider.value = values;
        console.log("Set midiVal values:", values);
      });
      // Initialize saturationRange slider (two handles for minSat/maxSat) with fixed min/max 0–255
      var saturationRangeSlider = document.getElementById('saturationRange');
      let satMin = 0;
      let satMax = 255;
      if (window.noUiSlider && saturationRangeSlider && !saturationRangeSlider.noUiSlider) {
        window.noUiSlider.create(saturationRangeSlider, {
          start: [satMin, satMax],
          connect: true,
          range: {
            'min': satMin,
            'max': satMax
          },
          step: 1,
          tooltips: [true, true],
          format: {
            to: function (value) { return Math.round(value); },
            from: function (value) { return Number(value); }
          }
        });
      }
      if (saturationRangeSlider && saturationRangeSlider.noUiSlider) {
        let satVals = saturationRangeSlider.noUiSlider.get();
        document.getElementById('saturationRangeValue').textContent = `${satVals[0]} - ${satVals[1]}`;
        saturationRangeSlider.noUiSlider.on('update', function(values) {
          document.getElementById('saturationRangeValue').textContent = `${values[0]} - ${values[1]}`;
          saturationRangeSlider.value = values;
        });
        saturationRangeSlider.noUiSlider.on('set', function(values) {
          saturationRangeSlider.value = values;
        });
      }
    }

    // Initialize midiRms slider as a range (two handles)
    var midiRmsSlider = document.getElementById('midiRms');
    if (window.noUiSlider && midiRmsSlider && !midiRmsSlider.noUiSlider) {
      window.noUiSlider.create(midiRmsSlider, {
        start: [typeof data.minRms !== 'undefined' ? data.minRms : minRMS, typeof data.maxRms !== 'undefined' ? data.maxRms : maxRMS],
        connect: true,
        range: {
          'min': minRMS,
          'max': maxRMS
        },
        step: 0.001,
        tooltips: [true, true],
        format: {
          to: function (value) { return parseFloat(value).toFixed(3); },
          from: function (value) { return Number(value); }
        }
      });
    }
    if (midiRmsSlider && midiRmsSlider.noUiSlider) {
      let rmsVals = midiRmsSlider.noUiSlider.get();
      document.getElementById('midiRmsValue').textContent = `${rmsVals[0]} - ${rmsVals[1]}`;
      midiRmsSlider.noUiSlider.on('update', function(values) {
        document.getElementById('midiRmsValue').textContent = `${values[0]} - ${values[1]}`;
        midiRmsSlider.value = values;
        console.log("Updated midiRms values:", values);
      });
      midiRmsSlider.noUiSlider.on('set', function(values) {
        midiRmsSlider.value = values;
        console.log("Set midiRms values:", values);
      });
    }


    // Initialize midiSaturation slider as a range (two handles)
    var midiSaturationSlider = document.getElementById('midiSaturation');
    if (window.noUiSlider && midiSaturationSlider && !midiSaturationSlider.noUiSlider) {
      window.noUiSlider.create(midiSaturationSlider, {
        start: [typeof data.saturation !== 'undefined' ? data.saturation : 128],
        connect: [true, false],
        range: {
          'min': 0,
          'max': 255
        },
        step: 1,
        tooltips: true,
        format: {
          to: function (value) { return Math.round(value); },
          from: function (value) { return Number(value); }
        }
      });
    }
    if (midiSaturationSlider && midiSaturationSlider.noUiSlider) {
      let satVal = midiSaturationSlider.noUiSlider.get();
      document.getElementById('midiSatValue').textContent = `${satVal}`;
      function updateSaturationSliderColor(values) {
        let v = Array.isArray(values) ? Number(values[0]) : Number(values);
        document.getElementById('midiSatValue').textContent = `${v}`;
        midiSaturationSlider.value = v;
        let midiHueEl = document.getElementById('midiHue');
        let hueVal = 0;
        if (midiHueEl && midiHueEl.noUiSlider) {
          hueVal = Number(midiHueEl.noUiSlider.get());
        } else if (midiHueEl) {
          hueVal = Number(midiHueEl.value || 0);
        }
        const rgb = hsvToRgb(hueVal / 255, v / 255, 1);
        midiSaturationSlider.style.background = `linear-gradient(to right, rgb(${rgb[0]},${rgb[1]},${rgb[2]}), rgb(${rgb[0]},${rgb[1]},${rgb[2]}))`;
      }
      midiSaturationSlider.noUiSlider.on('update', updateSaturationSliderColor);
      midiSaturationSlider.noUiSlider.on('set', function(values) {
        let v = Array.isArray(values) ? Number(values[0]) : Number(values);
        midiSaturationSlider.value = v;
        updateSaturationSliderColor(v);
        console.log("Set midiSaturation value:", v);
      });
    }

    // Initialize midiHue slider (single handle)
    var midiHueSlider = document.getElementById('midiHue');
    if (window.noUiSlider && midiHueSlider && !midiHueSlider.noUiSlider) {
      window.noUiSlider.create(midiHueSlider, {
        start: [typeof data.hue !== 'undefined' ? data.hue : 25],
        connect: [true, false],
        range: { 'min': 0, 'max': 255 },
        step: 1,
        tooltips: true,
        format: {
          to: function (value) { return Math.round(value); },
          from: function (value) { return Number(value); }
        }
      });
    }
    if (midiHueSlider && midiHueSlider.noUiSlider) {
      let hueVal = midiHueSlider.noUiSlider.get();
      document.getElementById('midiHueValue').textContent = `${hueVal}`;
      // update visual display
      midiHueSlider.noUiSlider.on('update', function(values) {
        const v = Number(values);
        document.getElementById('midiHueValue').textContent = `${v}`;
        // update background color preview
        const el = document.getElementById('midiHue');
        if (el) {
          const rgb = hsvToRgb(v / 255, 1, 1);
          el.style.background = `linear-gradient(to right, #fff, rgb(${rgb[0]},${rgb[1]},${rgb[2]}))`;
        }
        midiHueSlider.value = values;
      });
      midiHueSlider.noUiSlider.on('set', function(values) {
        midiHueSlider.value = values;
      });
    }

    // Set Distance from params if available
    var distanceSliderEl = document.getElementById('distance');
    if (distanceSliderEl && distanceSliderEl.noUiSlider && typeof data.distance !== 'undefined') {
      distanceSliderEl.noUiSlider.set(Number(data.distance));
      var dLabel = document.getElementById('distanceValue');
      const dVal = distanceSliderEl.noUiSlider.get();
      if (dLabel) dLabel.textContent = `${dVal}`;
      distanceSliderEl.value = Number(Array.isArray(dVal) ? dVal[0] : dVal);
    }
    // Set Distance switch from params if available
    var distanceSwitchEl = document.getElementById('distanceSwitch');
    if (distanceSwitchEl && typeof data.distanceSwitch !== 'undefined') {
      distanceSwitchEl.checked = !!data.distanceSwitch;
    }
  })
  .catch(e => console.error('Failed to fetch MIDI params or initialize sliders:', e));


// Only fetch and set MIDI params after all sliders are initialized
// (Handled by fetchAndSetMidiParams below, so this block is removed to avoid duplicate calls)




// Fetch and set MIDI params after all sliders are initialized
function fetchAndSetMidiParams() {
  fetch('/getMidiParams')
    .then(response => response.json())
    .then(data => {
      console.log("Fetched MIDI params:", data);
      // Update sliders if data is valid
      if (data) {
        // Set pitch range
        var instrumentSlider = document.getElementById('instrumentRange');
        if (instrumentSlider && instrumentSlider.noUiSlider && typeof data.rangeMin !== 'undefined' && typeof data.rangeMax !== 'undefined') {
          instrumentSlider.noUiSlider.set([data.rangeMin, data.rangeMax]);
        }
        // Set val
        var midiValSlider = document.getElementById('midiVal');
        if (typeof data.val !== 'undefined') {
          if (midiValSlider && midiValSlider.noUiSlider) {
            console.log("Setting MIDI val slider to:", data.val);
            midiValSlider.noUiSlider.set(data.val);
            // Update value span after slider is set
            setTimeout(function() {
              document.getElementById('midiValValue').textContent = midiValSlider.noUiSlider.get();
            }, 0);
          } else if (midiValSlider) {
            midiValSlider.value = data.val;
            document.getElementById('midiValValue').textContent = data.val;
          }
        }
        // Set saturation
        var midiSaturationSlider = document.getElementById('midiSaturation');
        if (typeof data.saturation !== 'undefined') {
          if (midiSaturationSlider && midiSaturationSlider.noUiSlider) {
            console.log("Setting MIDI saturation slider to:", data.saturation);
            midiSaturationSlider.noUiSlider.set(data.saturation);
            setTimeout(function() {
              document.getElementById('midiSatValue').textContent = midiSaturationSlider.noUiSlider.get();
            }, 0);
          } else if (midiSaturationSlider) {
            midiSaturationSlider.value = data.saturation;
            document.getElementById('midiSatValue').textContent = data.saturation;
          }
        }
        // Set hue if provided
        var midiHueSlider = document.getElementById('midiHue');
        if (typeof data.hue !== 'undefined' && midiHueSlider && midiHueSlider.noUiSlider) {
          midiHueSlider.noUiSlider.set(Number(data.hue));
          setTimeout(function() {
            document.getElementById('midiHueValue').textContent = midiHueSlider.noUiSlider.get();
          }, 0);
        }
        // Set distance if provided
        var distanceSliderEl = document.getElementById('distance');
        if (distanceSliderEl && distanceSliderEl.noUiSlider && typeof data.distance !== 'undefined') {
          distanceSliderEl.noUiSlider.set(Number(data.distance));
          // update label and stored value
          var dLabel = document.getElementById('distanceValue');
          const val = distanceSliderEl.noUiSlider.get();
          if (dLabel) dLabel.textContent = `${val}`;
          distanceSliderEl.value = Number(Array.isArray(val) ? val[0] : val);
        }
        // Set distance switch if provided
        var distanceSwitchEl = document.getElementById('distanceSwitch');
        if (distanceSwitchEl && typeof data.distanceSwitch !== 'undefined') {
          distanceSwitchEl.checked = !!data.distanceSwitch;
        }
        // Set mode button if present
        if (typeof data.mode !== 'undefined') {
          if (data.mode === 1 && typeof mode !== 'undefined') {
            mode = 'frequency';
            if (switchMidiBtn) {
              switchMidiBtn.innerHTML = 'Mode: Frequency - Switch to MIDI';
              switchMidiBtn.style.backgroundColor = 'red';
            }
          } else if (typeof mode !== 'undefined') {
            mode = 'midi';
            if (switchMidiBtn) {
              switchMidiBtn.innerHTML = 'Mode: Midi<br>Switch to Frequency';
              switchMidiBtn.style.backgroundColor = 'green';
            }
          }
        }
      }
    })
    .catch(e => console.error('Failed to fetch MIDI params:', e));
}

// Call fetchAndSetMidiParams after all sliders are initialized
if (window.noUiSlider && document.getElementById('midiVale') && document.getElementById('midiVal').noUiSlider &&
    document.getElementById('midiSaturation') && document.getElementById('midiSaturation').noUiSlider &&
    document.getElementById('instrumentRange') && document.getElementById('instrumentRange').noUiSlider) {
  fetchAndSetMidiParams();
} else {
  // If sliders are not ready yet, wait for DOMContentLoaded and a short delay
  document.addEventListener('DOMContentLoaded', function() {
    setTimeout(fetchAndSetMidiParams, 200);
  });
}

// Distance slider (10–100)
var distanceSlider = document.getElementById('distance');
if (window.noUiSlider && distanceSlider && !distanceSlider.noUiSlider) {
  window.noUiSlider.create(distanceSlider, {
    start: 50,
    connect: [true, false],
    range: { min: 10, max: 100 },
    step: 1,
    tooltips: true,
    format: {
      to: function (value) { return Math.round(value); },
      from: function (value) { return Number(value); }
    }
  });
}
if (distanceSlider && distanceSlider.noUiSlider) {
  let dVal = distanceSlider.noUiSlider.get();
  var dLabel = document.getElementById('distanceValue');
  if (dLabel) dLabel.textContent = `${dVal}`;
  // store on element for submit handler compatibility
  distanceSlider.value = Number(dVal);
  distanceSlider.noUiSlider.on('update', function(values) {
    if (dLabel) dLabel.textContent = `${values}`;
    distanceSlider.value = Number(values);
  });
  distanceSlider.noUiSlider.on('set', function(values) {
    distanceSlider.value = Number(values);
  });
}