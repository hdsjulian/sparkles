export function toggleMenu() {
    const menu = document.getElementById('navMenu');
    menu.style.display = menu.style.display === 'flex' ? 'none' : 'flex';
  }

export function createNewCard(obj) {
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
    

    const cardsContainer = document.querySelector(".cards");
    let inserted = sortCards(obj, cardsContainer);
    if (!inserted) {
        cardsContainer.appendChild(newCard);
    }
    cardsContainer.appendChild(newCard);
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