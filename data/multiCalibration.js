function toggleInputsForClapCard(shouldAdd) {
    const clapCard = document.getElementById("clapCard");
  
    if (!clapCard) {
      console.error(`ClapCard with ID boardCard${obj.id} not found.`);
      return;
    }
  
    const existingInputs = clapCard.querySelector(".inputs");
  
    if (shouldAdd) {
      // Add the inputs if they don't already exist
      if (!existingInputs) {
        const placeholderXText = obj.xpos == 0 ? "placeholder='X position'" : `value='${obj.xpos}'`;
        const placeholderYText = obj.ypos == 0 ? "placeholder='Y position'" : `value='${obj.ypos}'`;
        const placeholderZText = obj.zpos == 0 ? "placeholder='Z position'" : `value='${obj.zpos}'`;
  
        const inputsHTML = `
          <div class='inputs'>
            <input type='text' id='xpos_${obj.id}' ${placeholderXText}>
            <input type='text' id='ypos_${obj.id}' ${placeholderYText}>
            <input type='text' id='zpos_${obj.id}' ${placeholderZText}>
            <button id='submit_${obj.id}'>Submit</button>
            <button id='update_${obj.id}' class='blue-button'>Update Device</button>
            <button id='blink_${obj.id}' class='blue-button'>Blink</button>
          </div>`;
  
        clapCard.insertAdjacentHTML("beforeend", inputsHTML);
  
        // Add event listeners for the buttons
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
    } else {
      // Remove the inputs if they exist
      if (existingInputs) {
        existingInputs.remove();
      }
    }
  }


  function formatNonZeroFloats(floatArray) {
    // Filter out zero values and format the remaining values
    const formattedArray = floatArray
        .filter(value => value !== 0) // Filter out zero values
        .map(value => value.toFixed(1)); // Format each value to one decimal place

    return formattedArray;
}
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
