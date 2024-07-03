#include "Arduino.h"

#include "esp_now.h"
#include "WiFi.h"
#include "messaging.h"


void messaging::setTimerReceiver(const uint8_t * incomingData) {
    memcpy(&addressMessage,incomingData,sizeof(addressMessage));
    globalModeHandler->switchMode( MODE_SENDING_TIMER);
    addError("setting timer receiver to "+stringAddress(addressMessage.address));
    if (memcmp(&addressMessage.address, clapDeviceAddress, 6) == 0) {
        addError("setting timer receiver to clap device");
        memcpy(&timerReceiver, addressMessage.address, 6);
        addPeer(timerReceiver);
        return;
    }
    for (int i = 0; i < NUM_DEVICES; i++) {
        if (memcmp(&clientAddresses[i].address, emptyAddress, 6) == 0) {
            //printAddress(addressMessage.address);
            memcpy(&clientAddresses[i].address, addressMessage.address, 6);
            clientAddresses[i].id = i;
            memcpy(&timerReceiver, addressMessage.address, 6);
            addPeer(timerReceiver);
            timerMessage.addressId = i;
            addressCounter++;
            handleLed->flash(0, 125, 0, 100, 2, 50);
            break;
        }
        else if (memcmp(&clientAddresses[i].address, addressMessage.address, 6) == 0) {
            memcpy(&timerReceiver, addressMessage.address, 6);
            timerMessage.addressId = i;
            addPeer(timerReceiver);
            break;
        }
    }
}




void messaging::processDataFromReceivedQueue() {

    std::vector<ReceivedData> receivedDataList; // To store data temporarily
    {
        std::lock_guard<std::mutex> lock(receiveQueueMutex); // Lock the mutex
        while (!dataQueue.empty()) {
            ReceivedData receivedData = dataQueue.front(); // Get the front element
            // Process the received data here...
            dataQueue.pop(); // Remove the front element from the queue
            receivedDataList.push_back(receivedData);
        }
    } // Mutex is unlocked here

    // Call handleReceive outside the mutex scope
    for (const auto& receivedData : receivedDataList) {
        handleReceive(receivedData.mac, receivedData.incomingData, receivedData.len, receivedData.msgReceiveTime);
    }
}




