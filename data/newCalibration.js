

calibrationStatus = 1;
data = {
  status: calibrationStatus,
  distance: 5.3, // Example distance
  clapId: '12345' // Example clap ID
};
updateCalibrationCard(data);
data.status = 3;
updateCalibrationCard(data);

  function updateCalibrationCard(data) {
    console.log("Updating calibration card with data:", data);
    calibrationCard = document.getElementById('calibrationCard');
    if (!calibrationCard) {
      console.error("Calibration card not found.");
      return;
    }
    if (data.status != calibrationStatus) {
      console.log("Updating calibration card status from", calibrationStatus, "to", data.status);
      calibrationStatus = data.status;
      element = document.getElementById('calibrationCard');
      newElement = element.cloneNode(true); // Clone the element
      element.parentNode.replaceChild(newElement, element); // Replace the old element with the clone
    } else {
      console.log("No change in calibration status:", data.status);
      return; // No need to update if the status hasn't changed
    }
    // Clear the current contents of the card
    calibrationCard = document.getElementById('calibrationCard');
    calibrationCard.innerHTML = '';

    // Update the card based on the status
    switch (data.status) {
      case 1:
        statusUncalibrated();
        break;
      case 2:
        statusCalibrationOpen();
        break;
      case 3:
        statusClapHappened(data);
        break;
      case 4:
        calibrationCard.innerHTML = `<p>Status: Waiting for Calculation to finish</p>`;
        break;
      case 5:
        calibrationCard.innerHTML = `<p>Status: Calibrated! Check <a href="/addressList.html">Address List</a></p>`;
        break;
      default:
        calibrationCard.innerHTML = `<p>Status: Unknown</p>`;
        break;
    }
  }

function calibrationButton(target, status) {
    document.getElementById('calibrateButton').addEventListener('click', () => {
      fetch(target)
        .then(response => {
          if (response.ok) {
            console.log("Calibration command sent successfully.");
            updateCalibrationCard(status);
          } else {
            console.error("Failed to send calibration command.");
          }
        })
        .catch(error => console.error("Error:", error));
    });
}


function statusUncalibrated() {
    calibrationCard.innerHTML = `<p>Status: Uncalibrated</p>
    <div class='inputs'>
      <button id="calibrateButton">Calibrate</button>
    </div>`;
    calibrationButton('/commandStartCalibration', 2);
  } 

  function statusCalibrationOpen() {
    calibrationCard.innerHTML = `<p>Status: Waiting for Clap</p>
    <div class='inputs'>
      <button id="calibrateButton">Cancel</button>
    </div>`;
    calibrationButton('/commandCancelCalibration', 1);

  }

  function statusClapHappened() {
    calibrationCard.innerHTML = `<p>Status: Clap Detected</p>
    <p> Happened at a distance of ${data.distance} meters to Master</p>
    <div class='inputs'>
    <input type='text' id='CDxpos' placeholder="X position" value="-1">
    <input type='text' id='CDypos' placeholder="Y position" value="-1">
    <input type='hidden' id='clapId' value='${data.clapId}'>
    <button id="continueButton" class="red-button half-button">Cancel</button>
    <button id="cancelButton" class="half-button">Continue</button>
    <button id="resetutton" class="red-button half-button">Reset</button>
    
    <button id="endCalibrationButton" class="blue-button half-button">End Calibration</button>
    </div>`;
    document.getElementById('continueButton').addEventListener('click', () => {
      const x = document.getElementById('CDxpos').value;
      const y = document.getElementById('CDypos').value;
      const z = document.getElementById('CDzpos').value;
      const clapId = document.getElementById('clapId').value;
      fetch(`/commandContinueCalibration?x=${x}&y=${y}&clapId=${clapId}`)
        .then(response => {
          if (response.ok) {
            console.log("Continue command sent successfully.");
            updateCalibrationCard({ status: 5 });
          } else {
            console.error("Failed to send continue command.");
          }
        })
        .catch(error => console.error("Error:", error));
    });
    document.getElementById('cancelButton').addEventListener('click', () => {
      fetch('/commandCancelCalibration')
        .then(response => {
          if (response.ok) { 
            console.log("Cancel command sent successfully.");
            updateCalibrationCard({ status: 1 });
          } else {
            console.error("Failed to send cancel command.");
          } 
        })
        .catch(error => console.error("Error:", error));
      
    }
  
  );
    document.getElementById('resetButton').addEventListener('click', () => {
      fetch('/commandResetCalibration')
        .then(response => {
          if (response.ok) {
            console.log("Reset command sent successfully.");
            updateCalibrationCard({ status: 1 });
          } else {
            console.error("Failed to send reset command.");
          }
        })
        .catch(error => console.error("Error:", error));
    });
    document.getElementById('endCalibrationButton').addEventListener('click', () => {
      fetch('/commandEndCalibration')
        .then(response => {
          if (response.ok) {you
            console.log("End command sent successfully.");
            updateCalibrationCard({ status: 5 });
          } else {
            console.error("Failed to send end command.");
          }
        })
        .catch(error => console.error("Error:", error));
    });
  }


function pollSystemStatus() {
    fetch('/systemStatus')
      .then(response => response.json())
      .then(data => {
        if (data && data.status) {
          updateCalibrationCard(data);
        } else {
          console.error("Invalid response from server:", data);
        }
      })
      .catch(error => console.error("Error fetching system status:", error));
  }
  function setupSystemStatusEventListener() {
    if (!!window.EventSource) {
      const sourceEvents = new EventSource('/events');

      sourceEvents.addEventListener('systemStatus', (e) => {
        const data = JSON.parse(e.data);
        if (data && data.status) {
          updateCalibrationCard(data);
        } else {
          console.error("Invalid event data:", data);
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
    setInterval(pollSystemStatus, 5000); // Poll every 5 seconds
  } else {
    //setupSystemStatusEventListener();
  }