#include "Arduino.h"

#include "esp_now.h"
#include "WiFi.h"
#include "messaging.h"







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
        handleReceive(receivedData.senderAddress, receivedData.incomingData, receivedData.len, receivedData.msgReceiveTime);
    }
}




