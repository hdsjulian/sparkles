'use strict';
import {toggleMenu} from "./sparkles.js";

var debug = 0;

function setTime() {
  var d = new Date();
  var n = d.toLocaleTimeString();
  document.getElementById("setClock").textContent = "Set System Time to: "+n;
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
document.getElementById("neutral").addEventListener('click', function() {
  var fetchUrl = '/neutral';
  fetchMe(fetchUrl);
  var clickButton = document.getElementById("neutral");
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

document.getElementById('submit_goodnight').addEventListener('click', function() {
  var hours = document.getElementById('hours_goodnight').value;
  var minutes = document.getElementById('minutes_goodnight').value;
  var seconds = document.getElementById('seconds_goodnight').value;
  var fetchUrl = `/goodNight?hours=${encodeURIComponent(hours)}&minutes=${encodeURIComponent(minutes)}&seconds=${encodeURIComponent(seconds)}`;
  console.log(fetchUrl);
    fetchMe(fetchUrl);
    var clickButton = document.getElementById("cmd_goodnight");
  clickButton.classList.add("active");
  setTimeout(function() {
        clickButton.classList.remove('active');
      }, 1000);
});
document.getElementById('submit_goodmorning').addEventListener('click', function() {
  var hours = document.getElementById('hours_goodmorning').value;
  var minutes = document.getElementById('minutes_goodmorning').value;
  var seconds = document.getElementById('seconds_goodmorning').value;
  var fetchUrl = `/goodMorning?hours=${encodeURIComponent(hours)}&minutes=${encodeURIComponent(minutes)}&seconds=${encodeURIComponent(seconds)}`;
  console.log(fetchUrl);
    fetchMe(fetchUrl);
    var clickButton = document.getElementById("cmd_goodmorning");
  clickButton.classList.add("active");
  setTimeout(function() {
        clickButton.classList.remove('active');
      }, 1000);
});

document.getElementById('submit_animation_1').addEventListener('click', function() {
      handleAnimation('1');
  });

  document.getElementById('submit_animation_2').addEventListener('click', function() {
      handleAnimation('2');
  });
  function animateUpdate(obj) {
  console.log ("Animate update called");
    if (obj) {
      console.log(obj);
    if (obj.status == "true") {
      console.log("true");
      document.getElementById('cmd_animate').textContent = "END ANIMATION";
    }
    else {
      console.log("false");
      document.getElementById('cmd_animate').textContent = "ANIMATE";
    } 
  } 
};
  function handleAnimation(id) {
    const minSpeed = document.getElementById('minSpeed').value;
    const maxSpeed = document.getElementById('maxSpeed').value;

    const minPause = document.getElementById('minPause').value;
    const maxPause = document.getElementById('maxPause').value;

    const minReps = document.getElementById('minReps').value;
    const maxReps = document.getElementById('maxReps').value;

    const minSpread = document.getElementById('minSpread').value;
    const maxSpread = document.getElementById('maxSpread').value;
    const minRed = document.getElementById('minRed').value;
    const minGreen = document.getElementById('minGreen').value;
    const minBlue = document.getElementById('minBlue').value;

    const maxRed = document.getElementById('maxRed').value;
    const maxGreen = document.getElementById('maxGreen').value;
    const maxBlue = document.getElementById('maxBlue').value;

    const minAniReps = document.getElementById('minAniReps').value;
    const maxAniReps = document.getElementById('maxAniReps').value;

  // Handle form submission logic here
  var fetchUrl = `/setSyncAsyncParams?minSpeed=${encodeURIComponent(minSpeed)}&maxSpeed=${encodeURIComponent(maxSpeed)}&minPause=${encodeURIComponent(minPause)}&maxPause=${encodeURIComponent(maxPause)}&minReps=${encodeURIComponent(minReps)}&maxReps=${encodeURIComponent(maxReps)}&minSpread=${encodeURIComponent(minSpread)}&maxSpread=${encodeURIComponent(maxSpread)}&minRed=${encodeURIComponent(minRed)}&maxRed=${encodeURIComponent(maxRed)}&minGreen=${encodeURIComponent(minGreen)}&maxGreen=${encodeURIComponent(maxGreen)}&minBlue=${encodeURIComponent(minBlue)}&maxBlue=${encodeURIComponent(maxBlue)}&minAniReps=${encodeURIComponent(minAniReps)}&maxAniReps=${encodeURIComponent(maxAniReps)}`;
  fetchMe(fetchUrl);
  console.log(fetchUrl);
  };
if (debug == 0) {
  console.log("aha");
  if (!!window.EventSource) {
  var sourceEvents = new EventSource('/events');
  sourceEvents.addEventListener('num_devices', function(e) {
    var obj = JSON.parse(e.data);
    console.log(obj);
    document.getElementById("num_devices").textContent = obj.num_devices;
  }, false);
  console.log("ok worked");
  };
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
document.addEventListener('DOMContentLoaded', function() {fetchMe('/getParamsJson', setLedParams)});
document.addEventListener('DOMContentLoaded', function() {fetchMe('/updateDeviceNum', updateDeviceNum)});
function updateDeviceNum(obj) {
document.getElementById('num_devices').value = obj.num_devices;
}
function setLedParams(obj) {
console.log("setLedParams called");
console.log(obj);
document.getElementById('minSpeed').value = obj.minS;
document.getElementById('maxSpeed').value = obj.maxS;
document.getElementById('minPause').value = obj.minP;
document.getElementById('maxPause').value = obj.maxP;
document.getElementById('minSpread').value = obj.minSp;
document.getElementById('maxSpread').value = obj.maxSp;
document.getElementById('minReps').value = obj.minR;
document.getElementById('maxReps').value = obj.maxR;
document.getElementById('minAniReps').value = obj.minAR;
document.getElementById('maxAniReps').value = obj.maxAR;
document.getElementById('minRed').value = obj.minRGBR;
document.getElementById('maxRed').value = obj.maxRGBR;
document.getElementById('minGreen').value = obj.minRGBG;
document.getElementById('maxGreen').value = obj.maxRGBG;
document.getElementById('minBlue').value = obj.minRGBB;
document.getElementById('maxBlue').value = obj.maxRGBB;
}