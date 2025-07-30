
// Unified handler for test buttons
function setupTestButton(buttonId, endpoint) {
  window.addEventListener('DOMContentLoaded', function() {
    const btn = document.getElementById(buttonId);
    if (btn) {
      btn.addEventListener('click', function() {
        fetch(endpoint)
          .then(response => {
            if (response.ok) {
              btn.classList.add('active');
              setTimeout(() => {
                btn.classList.remove('active');
              }, 1000);
            } else {
              console.error('Failed to send calibration test command.');
            }
          })
          .catch(error => console.error('Error sending calibration test command:', error));
      });
    }
  });
}


setupTestButton('calibrateTestButton', '/commandTestCalibration');
setupTestButton('calibrateDistance', '/commandCalibrateDistance');



// Unified state for both calibration types
let calibrationStatus = 0;
let distanceStatus = 0;
let data = {status: 1}; // Default status for the calibration card


window.addEventListener('DOMContentLoaded', function() {
  updateCard('calibration', data);
  updateCard('distance', data);
});



// Card updater
function updateCard(type, data) {
  const cardId = type === 'calibration' ? 'calibrationCard' : 'distanceCard';
  let statusVar = type === 'calibration' ? calibrationStatus : distanceStatus;
  console.log(type);
  console.log(statusVar);
  let setStatus = (val) => {
    if (type === 'calibration') calibrationStatus = val;
    else distanceStatus = val;
  };
  let card = document.getElementById(cardId);
  if (!card) {
    console.error(`${type} card not found.`);
    return;
  }
  if (data.status != statusVar) {
    console.log(`Updating ${type} card2 with status: ${data.status}`);
    setStatus(data.status);
    let element = document.getElementById(cardId);
    let newElement = element.cloneNode(true);
    element.parentNode.replaceChild(newElement, element);
  } else {
    return;
  }
  card = document.getElementById(cardId);
  card.innerHTML = '';
  
  // Unified status switch
  switch (data.status) {
    case 1:
      console.log("Status 1, updating card");
      statusUncalibrated(type);
      break;
    case 2:
      console.log("Status 2, updating card");
      statusCalibrationOpen(type);
      break;
    case 3:
      console.log("Clap happened, updating card");
      statusClapHappened(type, data);
      break;
    case 4:
      card.innerHTML = `<p>Status: Waiting for Calculation to finish</p>`;
      break;
    case 5:
      card.innerHTML = `<p>Status: Calibrated! Check <a href="/addressList.html">Address List</a></p>`;
      break;
    default:
      console.log("Unknown status, updating card");
      console.log(data);
      console.log(data.status);
      card.innerHTML = `<p>Status: Unknown</p>`;
      break;
  }
}



function calibrationButton(type, target, status) {
  const btnId = type === 'calibration' ? 'calibrateButton' : 'calibrateDistanceButton';
  const updater = type === 'calibration' ? updateCard.bind(null, 'calibration') : updateCard.bind(null, 'distance');
  const dataObj = {status: status};
  const btn = document.getElementById(btnId);
  if (btn) {
    btn.addEventListener('click', () => {
      fetch(target)
        .then(response => {
          if (response.ok) {
            updater(dataObj);
          } else {
            console.error("Failed to send calibration command.");
          }
        })
        .catch(error => console.error("Error:", error));
    });
  }
}



function statusUncalibrated(type) {
  const cardId = type === 'calibration' ? 'calibrationCard' : 'distanceCard';
  const btnId = type === 'calibration' ? 'calibrateButton' : 'calibrateDistanceButton';
  const label = type === 'calibration' ? 'Calibrate' : 'Calibrate Distance';
  const endpoint = type === 'calibration' ? '/commandStartCalibration' : '/commandStartDistanceCalibration';
  document.getElementById(cardId).innerHTML = `<p>Status: Uncalibrated</p>
    <div class='inputs'>
      <button id="${btnId}">${label}</button>
    </div>`;
  calibrationButton(type, endpoint, 2);
}

function statusCalibrationOpen(type) {
  const cardId = type === 'calibration' ? 'calibrationCard' : 'distanceCard';
  const btnId = type === 'calibration' ? 'calibrateButton' : 'calibrateDistanceButton';
  const endpoint = type === 'calibration' ? '/commandCancelCalibration' : '/commandCancelDistanceCalibration';
  document.getElementById(cardId).innerHTML = `<p>Status: Waiting for Clap</p>
    <div class='inputs'>
      <button id="${btnId}">Cancel</button>
    </div>`;
  calibrationButton(type, endpoint, 1);
}



function statusClapHappened(type, data) {
  console.log("Clap happened2 , updating card");
  const cardId = type === 'calibration' ? 'calibrationCard' : 'distanceCard';
  const card = document.getElementById(cardId);
  const isCalibration = type === 'calibration';
  let html = `<p>Status: Clap Detected</p>
    <p> Happened at a distance of ${data.distance} meters to Master</p>`;
  if (isCalibration) {
    html += `<div class='inputs'>
      <input type='text' id='CDxpos' placeholder="X position" value="-1">
      <input type='text' id='CDypos' placeholder="Y position" value="-1">
      <button id="cancelButton" class="red-button half-button">Cancel</button>
      <button id="continueButton" class="half-button">Continue</button>
      <button id="resetButton" class="red-button half-button">Reset</button>
      <button id="endCalibrationButton" class="blue-button half-button">End Calibration</button>
    </div>`;
  } else {
    html += `<div class='inputs'>
      <button id="cancelButton" class="red-button half-button">Cancel</button>
      <button id="continueButton" class="half-button">Continue</button>
      <button id="resetButton" class="red-button half-button">Reset</button>
      <button id="endCalibrationButton" class="blue-button half-button">End Distance Calibration</button>
    </div>`;
  }
  card.innerHTML = html;

  // Add event listeners, disable buttons on click to prevent double submissions
  const cancelBtn = document.getElementById('cancelButton');
  const continueBtn = document.getElementById('continueButton');
  const resetBtn = document.getElementById('resetButton');
  const endBtn = document.getElementById('endCalibrationButton');

  continueBtn.addEventListener('click', () => {
    continueBtn.disabled = true;
    if (isCalibration) {
      const x = document.getElementById('CDxpos').value;
      const y = document.getElementById('CDypos').value;
      fetch(`/commandContinueCalibration?x=${x}&y=${y}`)
        .then(response => {
          if (response.ok) {
            updateCard('calibration', { status: 2 });
          }
        })
        .finally(() => { continueBtn.disabled = false; });
    } else {
      console.log("Continue distance calibration");
      fetch(`/commandContinueDistanceCalibration`)
        .then(response => {
          if (response.ok) {
            updateCard('distance', { status: 2 });
          }
        })
        .finally(() => { continueBtn.disabled = false; });
    }
  });
  cancelBtn.addEventListener('click', () => {
    cancelBtn.disabled = true;
      fetch('/commandCancelCalibration')
        .then(response => {
          if (response.ok) {
            updateCard('calibration', { status: 1 });
          }
        })
        .finally(() => { cancelBtn.disabled = false; });
  });
  resetBtn.addEventListener('click', () => {
    resetBtn.disabled = true;
      fetch('/commandResetCalibration')
        .then(response => {
          if (response.ok) {
            updateCard('calibration', { status: 1 });
          }
        })
        .finally(() => { resetBtn.disabled = false; });
  });
  endBtn.addEventListener('click', () => {
    endBtn.disabled = true;
      fetch('/commandEndCalibration')
        .then(response => {
          if (response.ok) {
            updateCard('calibration', { status: 5 });
          }
        })
        .finally(() => { endBtn.disabled = false; });
  });
}



function pollStatus(type) {
  const endpoint = type === 'calibration' ? '/calibrationStatus' : '/distanceCalibrationStatus';
  fetch(endpoint)
    .then(response => response.json())
    .then(data => {
      if (data && data.status) {
        updateCard(type, data);
      } else {
        console.error("Invalid response from server:", data);
      }
    })
    .catch(error => console.error("Error fetching system status:", error));
}

function setupStatusEventListener(type) {
  if (!!window.EventSource) {
    const sourceEvents = new EventSource('/events');
    const eventName = type === 'calibration' ? 'calibrationStatus' : 'distanceStatus';
    console.log(`Setting up EventSource for ${type} with event name: ${eventName}`);
    sourceEvents.addEventListener(eventName, (e) => {
      const data = JSON.parse(e.data);
      if (data && data.status) {
        console.log(`Event received for ${type}:`, data);
        updateCard(type, data);
      } else {
        console.error("Invalid event data:", data);
      }
    });
    // Listen for clientClap event
    sourceEvents.addEventListener('clientClap', (e) => {
      const data = JSON.parse(e.data);
      console.log('clientClap event received:', data);
      // Helper to update or create clap-info node for a card
      function updateClapInfo(card) {
        console.log('update clap info');
        console.log(card);
        if (!card) return;
        const nodeId = `clap-info-board-${data.boardId}`;
        let node = card.querySelector(`#${nodeId}`);
        if (!node) {
          node = document.createElement('div');
          node.className = 'clap-info';
          node.id = nodeId;
          card.appendChild(node);
        }
        node.textContent = `Clap received from board ${data.boardId} at distance ${data.clapDistance} m`;
      }
      if (data.event == 'distanceClap') {
        console.log('calling distance Clap');
        updateClapInfo(document.getElementById('distanceCard'));
      }
      else {
        console.log('calling normal clap');
        updateClapInfo(document.getElementById('calibrationCard'));
      }
      
    });
    sourceEvents.addEventListener('error', (e) => {
      console.error("EventSource error:", e);
    });
  } else {
    console.error("EventSource is not supported in this browser.");
  }
}



const usePolling = false; // Set to false to use EventSource instead
if (usePolling) {
  setInterval(() => pollStatus('calibration'), 5000);
  setInterval(() => pollStatus('distance'), 5000);
} else {
  setupStatusEventListener('calibration');
  setupStatusEventListener('distance');
}
  