'use strict';
import {toggleMenu, boardCards} from "./sparkles.js";

// Create or update board cards

toggleMenu();





// Update calibration button text
function syncUpdate(obj) {
  document.getElementById('cmd_sync_all').textContent = 
    obj.status ==="true" ? "SYNCING" : "SYNC";
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
        
    
    sourceEvents.addEventListener('syncStatus', (e) => {
      const obj = JSON.parse(e.data);
      syncUpdate(obj);
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
      console.log("Update board event received");
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

function handleCommandSyncClick() {
  //TODO: don't do anything if already syncing
  fetchData('/commandSync');
}

function handleCommandSyncAllClick() {
  fetchData('/commandSyncAll');
}

function handleCommandAnimateClick() {
  fetchData('/commandAnimate');
}

// Initialize the page
document.addEventListener('DOMContentLoaded', () => {
  // Add event listeners
  document.getElementById('t1').addEventListener('click', () => handleUpdateDeviceClick(-1));
  document.getElementById('cmd_sync').addEventListener('click', handleCommandSyncClick);
  document.getElementById('cmd_animate').addEventListener('click', handleCommandAnimateClick);
  document.getElementById('settings').addEventListener('click', () => {
    window.location.href = "settings.html";
  });
  
  // Initial data load
  setupEventSource();
  handleUpdateDeviceClick(-1);
});