const MODE_STANDARD = 1;
const MODE_ADDRESS_LIST = 2;
const MODE_ANIMATIONS = 3;
const MODE_SETTINGS = 4;
const MODE_CALIBRATION= 5;

export function toggleMenu() {
    const menu = document.getElementById('navMenu');
    menu.style.display = menu.style.display === 'flex' ? 'none' : 'flex';
  }



export function boardCards(obj, mode = MODE_STANDARD) {

    let existingCard = document.getElementById("boardCard" + obj.id);
    console.log(obj);
    
    if (!existingCard) {
      existingCard = createNewCard(obj, sortCards, mode);
    }
    
    // Update card status
    if (obj.status === "active") {
      existingCard.classList.add("active");
      existingCard.classList.remove("inactive");
    } else {
      existingCard.classList.remove("active");
      existingCard.classList.add("inactive");
    }
    // Update card information

    const batteryElem = document.getElementById("battery" + obj.id);
    if (batteryElem) batteryElem.textContent = "Battery: " + obj.batteryPercentage;

    const distCenterElem = document.getElementById("distCenter" + obj.id);
    if (distCenterElem) distCenterElem.textContent = "DfC: " + obj.distanceFromCenter;

    //const addrElem = document.getElementById("addr" + obj.id);
    //if (addrElem) addrElem.textContent = "Address: " + decimalArrayToMacAddress(obj.address);
    const delElem = document.getElementById("del" + obj.id);
    if (delElem) delElem.textContent = "Delay: " + obj.delay;
  
    if (obj.distances ) {
      const distElem = document.getElementById("dist" + obj.id);
      if (distElem) distElem.textContent = "Distances: " + formatNonZeroFloats(obj.distances);
    }
    let numDevices = document.getElementById("boardCards").children.length;
    document.getElementById("t1").textContent = numDevices;

  };

  function sortCards(obj, cardsContainer,newCard) {
    let inserted = false;   
    return false;
    for (let i = 0; i < cardsContainer.children.length; i++) {
      if (!cardsContainer.children[i].id.startsWith("boardCard")) continue;
      const child = cardsContainer.children[i];
      
        
      if (childId && parseInt(childId) > obj.id) {
         cardsContainer.insertBefore(newCard, child);
           inserted = true;
           break;
       }
      }
      console.log("sortCards called for obj.id: " + obj.id + ", inserted: " + inserted);
      console.log("new card innerHTML: " + newCard.innerHTML);
    return inserted;
  };
  
export function createNewCard(obj, sortCards, mode = MODE_STANDARD) {
    let cardsContainer = document.getElementById("boardCards");
    let newCard = document.createElement("div");
    newCard.className = "card";
    newCard.id = "boardCard" + obj.id;
const placeholderXText = (obj.xpos === undefined || obj.xpos === null || obj.xpos == 0)
    ? "placeholder='X position'"
    : `value='${obj.xpos}'`;
const placeholderYText = (obj.ypos === undefined || obj.ypos === null || obj.ypos == 0)
    ? "placeholder='Y position'"
    : `value='${obj.ypos}'`;
const placeholderZText = (obj.zpos === undefined || obj.zpos === null || obj.zpos == 0)
    ? "placeholder='Z position'"
    : `value='${obj.zpos}'`;
    
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
        <button id='blink_${obj.id}' class='blue-button'>Blink</button>
        <button id='message_${obj.id}' class='blue-button'>Message</button>
        <button id='calibrate_${obj.id}' class='blue-button'>Calibrate</button>
      </div>`;
    let inserted = sortCards(obj, cardsContainer, newCard);
    if (!inserted) {
        cardsContainer.appendChild(newCard);

    }


    document.getElementById('blink_' + obj.id).addEventListener('click', () => {
        const fetchUrl = `/commandBlink?boardId=${obj.id}`;
        fetchData(fetchUrl);
        console.log("Blink command sent for board ID: " + obj.id);
      });
    document.getElementById('update_' + obj.id).addEventListener('click', () => {
        handleUpdateDeviceClick(String(obj.id));
      });
      document.getElementById('message_' + obj.id).addEventListener('click', () => {
        const fetchUrl = `/commandMessage?boardId=${obj.id}`;
        fetchData(fetchUrl);
        console.log("Message command sent for board ID: " + obj.id);
      });
      document.getElementById('calibrate_' + obj.id).addEventListener('click', () => {
        const fetchUrl = `/commandCalibrate?boardId=${obj.id}`;
        fetchData(fetchUrl);
        console.log("Calibration command sent for board ID: " + obj.id);
      });
    document.getElementById('submit_' + obj.id).addEventListener('click', () => {
        const input1 = document.getElementById('xpos_' + obj.id).value;
        const input2 = document.getElementById('ypos_' + obj.id).value;
        const input3 = document.getElementById('zpos_' + obj.id).value;
        
        const fetchUrl = `/submitPositions?xpos=${encodeURIComponent(input1)}&ypos=${encodeURIComponent(input2)}&zpos=${encodeURIComponent(input3)}&boardId=${obj.id}`;
        fetchData(fetchUrl);
      });

    return newCard;
  };

    function decimalArrayToMacAddress(decimalArray) {
    // Ensure the array has exactly 6 elements
    if (decimalArray.length !== 6) {
        throw new Error("Array must contain exactly 6 elements");
    }

    // Convert each decimal value to a 2-digit hexadecimal string
    const hexArray = decimalArray.map(value => {
        // Ensure the value is a number and within the valid range for MAC addresses
        if (typeof value !== 'number' || value < 0 || value > 255) {
            throw new Error("Each element must be a number between 0 and 255");
        }
        return value.toString(16).padStart(2, '0');
    });

    // Join the hexadecimal strings with colons
    return hexArray.join(':');
}

  function formatNonZeroFloats(floatArray) {
    // Filter out zero values and format the remaining values
    const formattedArray = floatArray
        .filter(value => value !== 0) // Filter out zero values
        .map(value => value.toFixed(1)); // Format each value to one decimal place

    return formattedArray;
}

function handleUpdateDeviceClick(id) {
  sendDeviceUpdateRequest(id === -1 ? -1 : id);
}

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

function fetchData(fetchUrl) {
  console.log("Fetching URL: " + fetchUrl);
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