'use strict';

// Toggle navigation menu
function toggleMenu() {
  const menu = document.getElementById('navMenu');
  menu.style.display = menu.style.display === 'flex' ? 'none' : 'flex';
}

// Create or update board cards

function createNewCard(obj) {
    const newCard = document.createElement("div");
    newCard.className = "card";
    newCard.id = "boardCard" + obj.id;
    
    const placeholderXText = obj.xpos == 0 ? "placeholder='X position'" : `value='${obj.xpos}'`;
    const placeholderYText = obj.ypos == 0 ? "placeholder='Y position'" : `value='${obj.ypos}'`;
    const placeholderZText = obj.zpos == 0 ? "placeholder='Z position'" : `value='${obj.zpos}'`;
    
    newCard.innerHTML = `
      <span id='b${obj.id}'>Board ID ${obj.id}:</span>
      <span id='addr${obj.id}'></span>
      <span id='del${obj.id}'></span>
      <span id='dist${obj.id}'></span>
      <span id='battery${obj.id}'></span>
      
      <div class='inputs'>
        <input type='text' id='xpos_${obj.id}' ${placeholderXText}>
        <input type='text' id='ypos_${obj.id}' ${placeholderYText}>
        <input type='text' id='zpos_${obj.id}' ${placeholderZText}>
        <button id='submit_${obj.id}'>Submit</button>
        <button id='update_${obj.id}' class='blue-button'>Update Device</button>
      </div>`;
    
    const cardsContainer = document.querySelector(".cards");
    let inserted = sortCards(obj);
    if (!inserted) {
        cardsContainer.appendChild(newCard);
    }
    cardsContainer.appendChild(newCard);
    document.getElementById('update_' + obj.id).addEventListener('click', () => {
        handleUpdateDeviceClick(String(obj.id));
      });
      document.getElementById('submit_' + obj.id).addEventListener('click', () => {
        const input1 = document.getElementById('xpos_' + obj.id).value;
        const input2 = document.getElementById('ypos_' + obj.id).value;
        const input3 = document.getElementById('zpos_' + obj.id).value;
        
        const fetchUrl = `/submitPositions?xpos=${encodeURIComponent(input1)}&ypos=${encodeURIComponent(input2)}&zpos=${encodeURIComponent(input3)}&boardId=${obj.id}`;
        fetchData(fetchUrl);
      });
  
function boardCards(obj) {
  let existingCard = document.getElementById("boardCard" + obj.id);
  
  if (!existingCard) {
    createNewCard(obj);
  }
  
  // Update card status
  if (obj.status === "active") {
    document.getElementById("boardCard" + obj.id).classList.add("active");
    document.getElementById("boardCard" + obj.id).classList.remove("inactive");
  } else {
    document.getElementById("boardCard" + obj.id).classList.remove("active");
    document.getElementById("boardCard" + obj.id).classList.add("inactive");
  }
  
  // Update card information
  document.getElementById("addr" + obj.id).textContent = "Address: " + obj.address;
  document.getElementById("del" + obj.id).textContent = "Delay: " + obj.delay;
  document.getElementById("dist" + obj.id).textContent = "Distance: " + obj.distance;
  document.getElementById("battery" + obj.id).textContent = "Battery: " + obj.battery;
}


function sortCards(obj) {
    let inserted = false;   
    for (let i = 0; i < cardsContainer.children.length; i++) {
        const child = cardsContainer.children[i];
        const childId = child.id?.replace("boardCard", "");
        
        if (childId && parseInt(childId) > obj.id) {
          cardsContainer.insertBefore(newCard, child);
            inserted = true;
            break;
        }
    return inserted;
}

// Update status display
function statusUpdate(obj) {
  document.getElementById('status').textContent = obj.status;
}

// Update calibration button text
function calibrateUpdate(obj) {
  document.getElementById('cmd_calibrate').textContent = 
    obj.status === "true" ? "END CALIB" : "CALIBRATE";
}

// Update animation button text
function animateUpdate(obj) {
  document.getElementById('cmd_animate').textContent = 
    obj.status === "true" ? "END ANIM" : "ANIMATE";
}

// Update device count
function numDevicesUpdate(obj) {
  document.getElementById('t1').textContent = obj.numDevices;
}

// Setup EventSource for real-time updates
function setupEventSource() {
  if (!!window.EventSource) {
    const sourceEvents = new EventSource('/events');
    
    sourceEvents.addEventListener('open', () => {
      console.log("Events Connected");
    }, false);
    
    sourceEvents.addEventListener('error', (e) => {
      if (e.target.readyState !== EventSource.OPEN) {
        console.log("Events Disconnected");
      }
    }, false);
    
    sourceEvents.addEventListener('new_board', (e) => {
      const obj = JSON.parse(e.data);
      boardCards(obj);
    });
    
    sourceEvents.addEventListener('calibrationStatus', (e) => {
      const obj = JSON.parse(e.data);
      if (obj.calibrateEnd === "true") {
        document.getElementById("cmd_calibrate").classList.add("active");
      }
    });
    
    sourceEvents.addEventListener('new_status', (e) => {
      const obj = JSON.parse(e.data);
      statusUpdate(obj);
    });
    
    sourceEvents.addEventListener('calibrateStatus', (e) => {
      const obj = JSON.parse(e.data);
      calibrateUpdate(obj);
    });
    
    sourceEvents.addEventListener('animateStatus', (e) => {
      const obj = JSON.parse(e.data);
      animateUpdate(obj);
    });
    sourceEvents.addEventListener('numDevices', (e) => {
      const obj = JSON.parse(e.data);
      numDevicesUpdate(obj);
    });
    sourceEvents.addEventListener('update_board', (e) => {
      const obj = JSON.parse(e.data);
      boardCards(obj);
    });
  }
}

// Send device update request
function sendDeviceUpdateRequest(id) {
  const fetchUrl = id !== -1 ? `/getAddressList?id=${id}` : '/getAddressList';
  
  fetch(fetchUrl)
    .then(response => {
      if (!response.ok) {
        throw new Error('Network response was not ok');
      }
      return response.json();
    })
    .then(data => {
      numDevicesUpdate(data);
      data.addresses.forEach(boardData => {
        boardCards(boardData);
      });
    })
    .catch(error => {
      console.error('Fetch error:', error);
    });
}

// Generic fetch function
function fetchData(fetchUrl) {
  fetch(fetchUrl)
    .then(response => {
      if (!response.ok) {
        throw new Error('Network response was not ok');
      }
    })
    .catch(error => {
      console.error('Fetch error:', error);
    });
}

// Event handler functions
function handleUpdateDeviceClick(id) {
  sendDeviceUpdateRequest(id === -1 ? -1 : id);
}

function handleCommandCalibrateClick() {
  fetchData('/commandCalibrate');
}

function handleCommandAnimateClick() {
  fetchData('/commandAnimate');
}

// Initialize the page
document.addEventListener('DOMContentLoaded', () => {
  // Add event listeners
  document.getElementById('t1').addEventListener('click', () => handleUpdateDeviceClick(-1));
  document.getElementById('cmd_calibrate').addEventListener('click', handleCommandCalibrateClick);
  document.getElementById('cmd_animate').addEventListener('click', handleCommandAnimateClick);
  document.getElementById('settings').addEventListener('click', () => {
    window.location.href = "settings.html";
  });
  
  // Initial data load
  setupEventSource();
  handleUpdateDeviceClick(-1);
});