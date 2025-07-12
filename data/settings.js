'use strict';
import {toggleMenu} from "./sparkles.js";

//toggleMenu();
function setTime() {
  var d = new Date();
  var n = d.toLocaleTimeString();
  document.getElementById("setClock").textContent = "Set System Time to: "+n;
  console.log("setClock called");
}  
function sendTime() {
var d = new Date();
var year = d.getFullYear();
var month = d.getMonth() + 1; // Adding 1 to make the month human-readable (1-12 instead of 0-11)
var day = d.getDate();
var hours = d.getHours();
var minutes = d.getMinutes();
var seconds = d.getSeconds();
var fetchUrl = '/setTime?year=' + year + '&month=' + month + '&day=' + day + '&hours=' + hours + '&minutes=' + minutes + '&seconds=' + seconds;
fetchMe(fetchUrl);
}
setInterval(setTime, 1000);

document.getElementById("setClock").addEventListener('click', function() {
  console.log("setClock called");
  sendTime();
  var setClockButton = document.getElementById("setClock");
  setClockButton.classList.add("active");
  setTimeout(function() {
        setClockButton.classList.remove('active');
      }, 1000);
}
);
function statusUpdate(obj) {
  document.getElementById('status').textContent = obj.status;
}
document.getElementById('resetSystem').addEventListener('click', function() {
  var fetchUrl = '/resetSystem';
  var userConfirmed = confirm("Are you sure? You will reset the entire system!");
  if (userConfirmed) {
    fetchMe(fetchUrl);
  }
  var clickButton = document.getElementById("resetSystem");
  clickButton.classList.add("active");
  setTimeout(function() {
        clickButton.classList.remove('active');
      }, 1000);
});

document.getElementById('cmd_animate').addEventListener('click', function() {

  var fetchUrl = '/commandAnimate?brightness=' + document.getElementById('brightness').value;
  console.log("cmd_animate called");
  console.log(fetchUrl);
  fetchMe(fetchUrl, animateUpdate);
  var clickButton = document.getElementById("cmd_animate");
  clickButton.classList.add("active");
  setTimeout(function() {
        clickButton.classList.remove('active');
      }, 1000);
});

document.getElementById("addressList").addEventListener('click', function() {
  window.location.href = "addressList.html";
});
document.getElementById("lightsOff").addEventListener('click', function() {
  var fetchUrl = '/lightsOff';
  fetchMe(fetchUrl);
  var clickButton = document.getElementById("lightsOff");
  clickButton.classList.add("active");
  setTimeout(function() {
        clickButton.classList.remove('active');
      }, 1000);
});
document.getElementById("triggerSync").addEventListener('click', function() {
  var fetchUrl = '/triggerSync';
  fetchMe(fetchUrl);
  var clickButton = document.getElementById("triggerSync");
  clickButton.classList.add("active");
  setTimeout(function() {
        clickButton.classList.remove('active');
      }, 1000);
});

document.getElementById('submit_sleepTime').addEventListener('click', function() {
  var hours = document.getElementById('hours_sleepTime').value;
  var minutes = document.getElementById('minutes_sleepTime').value;
  var seconds = document.getElementById('seconds_sleepTime').value;
  var fetchUrl = `/setSleepTime?hours=${encodeURIComponent(hours)}&minutes=${encodeURIComponent(minutes)}&seconds=${encodeURIComponent(seconds)}`;
  console.log(fetchUrl);
    fetchMe(fetchUrl);
    var clickButton = document.getElementById("cmd_sleepTime");
  clickButton.classList.add("active");
  setTimeout(function() {
        clickButton.classList.remove('active');
      }, 1000);
});
document.getElementById('submit_wakeupTime').addEventListener('click', function() {
  var hours = document.getElementById('hours_wakeupTime').value;
  var minutes = document.getElementById('minutes_wakeupTime').value;
  var seconds = document.getElementById('seconds_wakeupTime').value;
  var fetchUrl = `/setWakeupTime?hours=${encodeURIComponent(hours)}&minutes=${encodeURIComponent(minutes)}&seconds=${encodeURIComponent(seconds)}`;
  console.log(fetchUrl);
    fetchMe(fetchUrl);
    var clickButton = document.getElementById("cmd_wakeupTime");
  clickButton.classList.add("active");
  setTimeout(function() {
        clickButton.classList.remove('active');
      }, 1000);
});








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
document.addEventListener('DOMContentLoaded', function() {fetchMe('/getParamsJson', setLedParams)});
document.addEventListener('DOMContentLoaded', function() {fetchMe('/updateDeviceNum', updateDeviceNum)});
function updateDeviceNum(obj) {
document.getElementById('num_devices').value = obj.num_devices;
}
