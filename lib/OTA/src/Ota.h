#ifndef OTA_H
#define OTA_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>

class OTAHandler {
public:
    static OTAHandler& getInstance();

    void setup();
    void setup(const char* url);
    void performUpdate();

private:
    OTAHandler();
    OTAHandler(const OTAHandler&) = delete;
    OTAHandler& operator=(const OTAHandler&) = delete;
    void connectToWiFi();
    const char* updateUrl;
};

#endif // OTA_H