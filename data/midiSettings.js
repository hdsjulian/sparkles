// On page load, fetch current MIDI params and update UI

// Handle MIDI settings submit button
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
      // Get saturation
      const midiSaturationSlider = document.getElementById('midiSaturation');
      if (midiSaturationSlider && midiSaturationSlider.noUiSlider) {
        let satRange = midiSaturationSlider.noUiSlider.get();
        minSat = Number(satRange[0]);
        maxSat = Number(satRange[1]);
        saturation = (minSat + maxSat) / 2;
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
      var minRms = 0.1, maxRms = 8.0;
      if (midiRmsSlider && midiRmsSlider.noUiSlider) {
        let rmsRange = midiRmsSlider.noUiSlider.get();
        minRms = Number(rmsRange[0]);
        maxRms = Number(rmsRange[1]);
      }
      // Add mode (0 for midi, 1 for frequency)
      let modeNum = (typeof mode !== 'undefined' && mode === 'frequency') ? 2 : 1;
      // Build fetchUrl with RMS
      const fetchUrl = `/setMidiParams?minVal=${encodeURIComponent(minVal)}&maxVal=${encodeURIComponent(maxVal)}&minSat=${encodeURIComponent(minSat)}&maxSat=${encodeURIComponent(maxSat)}&rangeMin=${encodeURIComponent(rangeMin)}&rangeMax=${encodeURIComponent(rangeMax)}&minRms=${encodeURIComponent(minRms)}&maxRms=${encodeURIComponent(maxRms)}&mode=${modeNum}`;
      console.log(fetchUrl);
      fetchMe(fetchUrl);
    }
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
    }

    // Initialize midiRms slider as a range (two handles)
    var midiRmsSlider = document.getElementById('midiRms');
    if (window.noUiSlider && midiRmsSlider && !midiRmsSlider.noUiSlider) {
      window.noUiSlider.create(midiRmsSlider, {
        start: [typeof data.minRms !== 'undefined' ? data.minRms : 0.1, typeof data.maxRms !== 'undefined' ? data.maxRms : 8.0],
        connect: true,
        range: {
          'min': 0.1,
          'max': 8.0
        },
        step: 0.1,
        tooltips: [true, true],
        format: {
          to: function (value) { return parseFloat(value).toFixed(1); },
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
        start: [typeof data.minSat !== 'undefined' ? data.minSat : 51, typeof data.maxSat !== 'undefined' ? data.maxSat : 255],
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
    if (midiSaturationSlider && midiSaturationSlider.noUiSlider) {
      let satVals = midiSaturationSlider.noUiSlider.get();
      document.getElementById('midiSatValue').textContent = `${satVals[0]} - ${satVals[1]}`;
      midiSaturationSlider.noUiSlider.on('update', function(values) {
        document.getElementById('midiSatValue').textContent = `${values[0]} - ${values[1]}`;
        midiSaturationSlider.value = values;
        console.log("Updated midiSaturation values:", values);
      });
      midiSaturationSlider.noUiSlider.on('set', function(values) {
        midiSaturationSlider.value = values;
        console.log("Set midiSaturation values:", values);
      });
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