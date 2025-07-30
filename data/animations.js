'use strict';
import {toggleMenu} from "./sparkles.js";

toggleMenu();
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
  console.log(fetchUrl);
  fetchMe(fetchUrl);
  console.log(fetchUrl);

  
  };

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