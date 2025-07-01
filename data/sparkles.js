const MODE_STANDARD = 1;
const MODE_ADDRESS_LIST = 2;
const MODE_ANIMATIONS = 3;
const MODE_SETTINGS = 4;
const MODE_CALIBRATION= 5;

export function toggleMenu() {
    const menu = document.getElementById('navMenu');
    menu.style.display = menu.style.display === 'flex' ? 'none' : 'flex';
  }



function boardCards(obj, mode = MODE_STANDARD) {
    let existingCard = document.getElementById("boardCard" + obj.id);
    
    if (!existingCard) {
      createNewCard(obj, sortCards, mode);
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
    document.getElementById("battery" + obj.id).textContent = "Battery: "+obj.batteryPercentage;
    document.getElementById("distCenter"+ obj.id).textContent = "DfC: "+obj.distanceFromCenter;

    document.getElementById("addr" + obj.id).textContent = "Address: "+decimalArrayToMacAddress(obj.address);
    if (obj.delay == 0) {
      document.getElementById("boardCard"+obj.id).classList.remove("active");
      document.getElementById("boardCard"+obj.id).classList.add("inactive");
    }
    else {
      document.getElementById("boardCard"+obj.id).classList.add("active");
      document.getElementById("boardCard"+obj.id).classList.remove("inactive");
    }

    document.getElementById("del"+obj.id).textContent = "Delay: "+obj.delay;
    document.getElementById("dist"+obj.id).textContent = "Distances: "+formatNonZeroFloats(obj.distances);
    document.getElementById("boardCard"+obj.id).addEventListener('click', function() {
    handleUpdateDeviceClick(String(obj.index));
      });
  };

  function sortCards(obj, cardsContainer,newCard) {
    let inserted = false;   
    for (let i = 0; i < cardsContainer.children.length; i++) {
        const child = cardsContainer.children[i];
        const childId = child.id?.replace("boardCard", "");
        
        if (childId && parseInt(childId) > obj.id) {
          cardsContainer.insertBefore(newCard, child);
            inserted = true;
            break;
        }
      }
    return inserted;
  };
  
export function createNewCard(obj, sortCards, mode = MODE_STANDARD) {
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
        <button id='blink_${obj.id}' class='blue-button'>Blink</button>
      </div>`;
    if (mode == MODE_CALIBRATION) {


    const cardsContainer = document.querySelector(".cards");
    let inserted = sortCards(obj, cardsContainer, newCard);
    if (!inserted) {
        cardsContainer.appendChild(newCard);
    }

    document.getElementById('update_' + obj.id).addEventListener('click', () => {
        handleUpdateDeviceClick(String(obj.id));
      });
    document.getElementById('blink_' + obj.id).addEventListener('click', () => {
        const fetchUrl = `/commandBlink?boardId=${obj.id}`;
        fetchData(fetchUrl);
      });
    document.getElementById('submit_' + obj.id).addEventListener('click', () => {
        const input1 = document.getElementById('xpos_' + obj.id).value;
        const input2 = document.getElementById('ypos_' + obj.id).value;
        const input3 = document.getElementById('zpos_' + obj.id).value;
        
        const fetchUrl = `/submitPositions?xpos=${encodeURIComponent(input1)}&ypos=${encodeURIComponent(input2)}&zpos=${encodeURIComponent(input3)}&boardId=${obj.id}`;
        fetchData(fetchUrl);
      });
    }