
// #include "../../include/myDefines.h"
#if DEVICE_MODE == MASTER
#include <MyDefines.h>
#include <WebServer.h>
#include <MessageHandler.h>


WebServer::WebServer(FS* fs) : server(80), events("/events"), filesystem(fs) {
    // Constructor body. Any additional setup code can go here.
}

WebServer& WebServer::getInstance(FS* fs) {
    static WebServer instance(fs); // Guaranteed to be destroyed and instantiated on first use
    return instance;
}

void WebServer::setup(MessageHandler &globalMessageHandler) {
    setWifi();
    configRoutes(); 
    messageHandlerInstance = &globalMessageHandler;
    server.addHandler(&events);
    server.begin();
}


void WebServer::setWifi() {
  WiFi.mode(WIFI_AP_STA);
  ESP_LOGI("WEB", "Setting up WiFi in AP mode");
  ESP_LOGI("WEB", "SSID: %s", WIFI_SSID);
  ESP_LOGI("WEB", "Password: %s", WIFI_PASSWORD);
  WiFi.softAP(WIFI_SSID, WIFI_PASSWORD);  
}

void WebServer::end() {
  server.end();
}
void WebServer::begin() {
  server.begin();
}
void WebServer::configRoutes() {
  events.onConnect([this](AsyncEventSourceClient *client){ 
    // send event with message "hello!", id current millis
    // and set reconnect delay to 1 second
    this->handleClientConnect(client);
  });

    //call animation
    server.on("/commandAnimate", HTTP_GET, [this] (AsyncWebServerRequest *request){
      this->commandAnimate(request);
    });
    //command
    server.on("/setTime", HTTP_GET, [this] (AsyncWebServerRequest *request){
      this->setTime(request);
    });
    //submit board's position manually
    server.on("/submitPositions", HTTP_GET, [this] (AsyncWebServerRequest *request){
      this->submitPositions(request);
    });
    //sync individual board, not implemented in html
    server.on("/commandSync", HTTP_GET, [this] (AsyncWebServerRequest *request){
      this->commandSync(request);
    });
    //sync all boards
    server.on("/commandSyncAll", HTTP_GET, [this] (AsyncWebServerRequest *request){
      this->commandSyncAll(request);
    }); 
    // let one board blink  
    server.on("/commandBlink", HTTP_GET, [this] (AsyncWebServerRequest *request){
      this->commandBlink(request);
    });
    server.on("/commandBlinkAll", HTTP_GET, [this] (AsyncWebServerRequest *request){
      this->commandBlinkAll(request);
    });
    server.on("/commandStartCalibration", HTTP_GET, [this] (AsyncWebServerRequest *request){
      // This is a placeholder for the calibration command
      // You can implement the calibration logic here
      this->commandStartCalibration(request);
      request->send(200, "text/html", "Calibration command received");
    });
    server.on("/commandStartDistanceCalibration", HTTP_GET, [this] (AsyncWebServerRequest *request){
      // This is a placeholder for the distance calibration command
      // You can implement the distance calibration logic here
      this->commandStartDistanceCalibration(request);
      inDistanceCalibration = true;
      request->send(200, "text/html", "Distance Calibration command received");
    });
    server.on("/commandContinueDistanceCalibration", HTTP_GET, [this] (AsyncWebServerRequest *request){
      this->commandContinueDistanceCalibration(request);
      request->send(200, "text/html", "Continue Distance Calibration command received");
    });
    server.on("/commandTestCalibration", HTTP_GET, [this] (AsyncWebServerRequest *request){
      // This is a placeholder for the calibration command
      // You can implement the calibration logic here
      this->commandTestCalibration(request);
      request->send(200, "text/html", "Test Calibration command received");
    });
    server.on("/commandOTAUpdate", HTTP_GET, [this] (AsyncWebServerRequest *request){
      this->commandOTAUpdate(request);
      request->send(200, "text/html", "OTA Update command received");
    });
    server.on("/commandCancelCalibration", HTTP_GET, [this] (AsyncWebServerRequest *request){
      this->commandCancelCalibration(request);
      request->send(200, "text/html", "Cancel Calibration command received");
    });
    server.on("/commandResetCalibration", HTTP_GET, [this] (AsyncWebServerRequest *request){
      this->commandResetCalibration(request);  
      request->send(200, "text/html", "Reset Calibration command received");
    });
    server.on("/commandContinueCalibration", HTTP_GET, [this] (AsyncWebServerRequest *request){
      this->commandContinueCalibration(request);
      request->send(200, "text/html", "Continue Calibration command received");
    });
    server.on("/commandEndCalibration", HTTP_GET, [this] (AsyncWebServerRequest *request){
      this->commandEndCalibration(request);
      request->send(200, "text/html", "End Calibration command received");
    });
    server.on("/commandMessage", HTTP_GET, [this] (AsyncWebServerRequest *request){
      this->commandMessage(request);
      request->send(200, "text/html", "Message command received");
    });
    server.on("/commandCalibrate", HTTP_GET, [this] (AsyncWebServerRequest *request){
      this->commandCalibrate(request);
      request->send(200, "text/html", "Calibration command received");
    });
    server.on("/commandGetMidiParams", HTTP_GET, [this] (AsyncWebServerRequest *request){
      this->getMidiParams(request);
    });
    server.on("/setMidiParams", HTTP_GET, [this] (AsyncWebServerRequest *request){
      this->setMidiParams(request);
      request->send(200, "text/html", "MIDI parameters set");
    });
    server.on("/getMidiParams", HTTP_GET, [this] (AsyncWebServerRequest *request){
      this->getMidiParams(request);
    });
    server.on("/setSleepTime", HTTP_GET, [this] (AsyncWebServerRequest *request){
      this->setSleepTime(request);
      request->send(200, "text/html", "Goodnight command received");
    });
    server.on("/setWakeupTime", HTTP_GET, [this] (AsyncWebServerRequest *request){
      this->setWakeupTime(request);
      request->send(200, "text/html", "Good morning command received");
    });
    //get all addresses
    server.on("/getAddressList", HTTP_GET, [this] (AsyncWebServerRequest *request){
      this->getAddressList(request);
    });
      server.onNotFound([this](AsyncWebServerRequest *request) {
        this->serveOnNotFound(request);
    });
    server.serveStatic("/", LittleFS, "/");


}

void WebServer::handleClientConnect(AsyncEventSourceClient *client) {
    connected = true;
    ESP_LOGI("WEB", "Client connected");
    client->send("");
}

void WebServer::serveOnNotFound(AsyncWebServerRequest *request) {
  String requestedFile = "Not Found: "+request->url();
    request->send(404, "text/plain", requestedFile.c_str());
}

void WebServer::commandAnimate(AsyncWebServerRequest *request) {
  String jsonString;
  message_animation animation;
  if (request->hasParam("brightness")) {
    int brightness = request->getParam("brightness")->value().toInt();
    animation.animationParams.strobe.brightness = brightness/255.0f;
  }

  jsonString = "{\"status\" : \"true\"}";
  request->send(200, "text/html", "{\"status\" : \"true\"}");
  }


void WebServer::commandSyncAll(AsyncWebServerRequest *request) {
    messageHandlerInstance->startAllTimerSyncTask();
    ESP_LOGI("WEB", "Syncing all devices");
    request->send(200, "text/html", "OK");
}

void WebServer::submitPositions(AsyncWebServerRequest *request) {
  int boardId = request->getParam("boardId")->value().toInt();
  float xpos = request->getParam("xpos")->value().toFloat();
  float ypos = request->getParam("ypos")->value().toFloat();
  messageHandlerInstance->setBoardPosition(boardId, xpos, ypos);
  request->send(200, "text/html", "OK");
}

void WebServer::setTime(AsyncWebServerRequest *request) {
  struct tm timeinfo;
  memset(&timeinfo, 0, sizeof(timeinfo));
  timeinfo.tm_year = request->getParam("year")->value().toInt() - 1900;
  timeinfo.tm_mon = request->getParam("month")->value().toInt() - 1;
  timeinfo.tm_mday = request->getParam("day")->value().toInt();
  timeinfo.tm_hour = request->getParam("hours")->value().toInt();
  timeinfo.tm_min = request->getParam("minutes")->value().toInt();
  timeinfo.tm_sec = request->getParam("seconds")->value().toInt();
  struct timeval tv;
  tv.tv_sec = mktime(&timeinfo);
  tv.tv_usec = 0;
  settimeofday(&tv, NULL);
  request->send(200, "text/html", "OK");
}


void WebServer::getAddressList(AsyncWebServerRequest *request) {
  ESP_LOGI("WEB", "getAddressList");
  String jsonString =  "";
  jsonString += "{\"numDevices\":";
  jsonString += String(messageHandlerInstance->getNumDevices());
  if (request->hasParam("id")) {
    int id = request->getParam("id")->value().toInt();
    jsonString += ", \"addresses\":[";
    jsonString += jsonFromAddress(id);
    jsonString += "]}";
    request->send(200, "text/html", jsonString.c_str());
    return;
  }
  for (int i = 0; i < NUM_DEVICES; i++) {
    if (i == 0) {
      jsonString += ", \"addresses\":[";
    }
    if (memcmp(messageHandlerInstance->getItemFromAddressList(i).address, messageHandlerInstance->emptyAddress, 6) == 0) {
      jsonString += "]}";
      break;
    }
    if (i > 0) {
      jsonString +=", ";
    }
    jsonString += jsonFromAddress(i);


  }
  //jsonString = messageHandlerInstance->getLedHandlerParams();
  request->send(200, "text/html", jsonString.c_str());
  ESP_LOGI("WEB", "AddressList: %s", jsonString.c_str());
}

void WebServer::updateAddress(int id) {
  String jsonString = jsonFromAddress(id);
  events.send(jsonString.c_str(), "update_board");
  ESP_LOGI("WEB", "Address Update: %s", jsonString.c_str());
}


String WebServer::jsonFromAddress(int id) {
  client_address address = messageHandlerInstance->getItemFromAddressList(id);
  String jsonString = "";
    jsonString += "{\"id\":\"";
    jsonString += String(id);
    jsonString += "\",\"address\":\"";
    for (int j = 0; j < 6; j++) {
      jsonString += String(address.address[j], HEX);
      if (j < 5) {
        jsonString += ":";
      }
    }
    jsonString += "\",\"status\":\"";
    jsonString += address.active == ACTIVE ? "active" : "inactive";
    ESP_LOGI("WEB", "Active Status %d", address.active );
    jsonString += "\",\"batteryPercentage\":\"";
    jsonString += String(address.batteryPercentage);
    jsonString += "\", \"distance\":\"";
    jsonString += String(address.distanceFromCenter);
    jsonString += "\", \"xpos\":\"";
    jsonString += String(address.xPos);
    jsonString += "\", \"ypos\":\"";
    jsonString += String(address.yPos);
    jsonString += "\", \"lastUpdateTime\":\"";
    jsonString += String(address.lastUpdateTime);
    jsonString += "\", \"timerOffset\":\"";
    jsonString += String(address.timerOffset);
    jsonString += "\", \"delay\":\"";
    jsonString += String(address.delay);
    jsonString += "\"}";
    return jsonString;
}

void WebServer::commandSync(AsyncWebServerRequest *request) {
  int index = request->getParam("index")->value().toInt();
  messageHandlerInstance->setCurrentTimerIndex(index);
  messageHandlerInstance->startTimerSyncTask();
  request->send(200, "text/html", "OK");
}


void WebServer::commandBlink(AsyncWebServerRequest *request) {
  if (!request->hasParam("boardId")) {
    request->send(400, "text/plain", "Missing parameter: boardId");
    return;
  }
  int boardId = request->getParam("boardId")->value().toInt();
  message_animation animation;
  animation.animationType = BLINK;
  animation.animationParams.blink.brightness = 255;
  animation.animationParams.blink.duration = 500;
  animation.animationParams.blink.repetitions = 3;
  animation.animationParams.blink.hue = 0;
  animation.animationParams.blink.saturation = 0;
  animation.animationParams.blink.startTime = micros()+1000000;
  messageHandlerInstance->sendAnimation(animation, boardId);
  request->send(200, "text/html", "OK");
}

void WebServer::commandBlinkAll(AsyncWebServerRequest *request) {
  message_animation animation;
  animation.animationType = BLINK;
  animation.animationParams.blink.brightness = 255;
  animation.animationParams.blink.duration = 500;
  animation.animationParams.blink.repetitions = 3;
  animation.animationParams.blink.hue = 0;
  animation.animationParams.blink.saturation = 0;
  animation.animationParams.blink.startTime = micros()+1000000;
  messageHandlerInstance->sendAnimation(animation,-1);
  request->send(200, "text/html", "OK");
}

void WebServer::updateAddressList() {
  for (int i = 0; i < NUM_DEVICES; i++) {
    if (memcmp(messageHandlerInstance->getItemFromAddressList(i).address, messageHandlerInstance->emptyAddress, 6) == 0) {
      break;
    }
    updateAddress(i);
  }
}

void WebServer::setCalculationDone(bool done) {
  PdParamsChanged = done;
  String jsonString = "{\"calculationStatus\":";
  jsonString += String(done ? "5" : "4");
  jsonString += "}";
  if (done) {
    events.send(jsonString.c_str(), "calculation_done");
  }
}

void WebServer::commandStartCalibration(AsyncWebServerRequest *request) {
  messageHandlerInstance->startCalibrationMaster();
  inCalibration = true;
  request->send(200, "text/html", "Calibration started");
}
void WebServer::commandOTAUpdate(AsyncWebServerRequest *request) {
  messageHandlerInstance->startOTAUpdateTask();
  request->send(200, "text/html", "OTA Update started");
}
void WebServer::commandCancelCalibration(AsyncWebServerRequest *request) {
  messageHandlerInstance->cancelCalibration();

  // Send calibrationStatus event with status 1
  String calibrationJson = "{\"status\":1}";
  events.send(calibrationJson.c_str(), "calibrationStatus");

  // Send distanceStatus event with status 1
  String distanceJson = "{\"status\":1}";
  events.send(distanceJson.c_str(), "distanceStatus");
}

void WebServer::commandResetCalibration(AsyncWebServerRequest *request) {
  messageHandlerInstance->resetCalibration();
  request->send(200, "text/html", "Calibration reset");
}
void WebServer::commandTestCalibration(AsyncWebServerRequest *request) {
  messageHandlerInstance->testCalibration();
  request->send(200, "text/html", "Test Calibration command received");
}
void WebServer::commandStartDistanceCalibration(AsyncWebServerRequest *request) {
  messageHandlerInstance->startDistanceCalibrationMaster();
  request->send(200, "text/html", "Distance Calibration command received");
}
void WebServer::commandContinueDistanceCalibration(AsyncWebServerRequest *request) {
    messageHandlerInstance->continueDistanceCalibration();
    request->send(200, "text/html", "Distance Calibration continued");
}
void WebServer::commandCalibrate(AsyncWebServerRequest *request) {
  if (!request->hasParam("boardId")) {
    request->send(400, "text/plain", "Missing parameter: boardId");
    return;
  }
  int boardId = request->getParam("boardId")->value().toInt();
  messageHandlerInstance->commandCalibrate(boardId);
  request->send(200, "text/html", "Calibration command received");
}
void WebServer::commandContinueCalibration(AsyncWebServerRequest *request) {
  float xPos = request->getParam("x")->value().toFloat();
  float yPos = request->getParam("y")->value().toFloat();
  messageHandlerInstance->continueCalibration(xPos, yPos);
  request->send(200, "text/html", "Calibration continued");
}
void WebServer::commandEndCalibration(AsyncWebServerRequest *request) {
  messageHandlerInstance->endCalibration();
  request->send(200, "text/html", "Calibration ended");
}
void WebServer::clapReceived(int clapId, unsigned long long clapTime) {
    String jsonString = "{";
    jsonString += "\"event\":\"clap\",";
    jsonString += "\"status\":3,";
    jsonString += "\"clapId\":";
    jsonString += String(clapId);
    jsonString += ",\"clapTime\":";
    jsonString += String(clapTime);
    jsonString += "}";
    ESP_LOGI("WEB", "Clap received: %s", jsonString.c_str());
    if (inCalibration) {
      events.send(jsonString.c_str(), "calibrationStatus");
    } else if (inDistanceCalibration) {
      events.send(jsonString.c_str(), "distanceStatus");
    } 

}
void WebServer::clapReceivedClient(int clapId, int boardId, float clapDistance) {
    String jsonString = "{";
    if (inCalibration) {
        jsonString += "\"event\":\"clap\",";
    } else if (inDistanceCalibration) {
        jsonString += "\"event\":\"distanceClap\",";
    } 
    jsonString += "\"clapId\":";
    jsonString += String(clapId);
    jsonString += ",\"boardId\":";
    jsonString += String(boardId);
    jsonString += ",\"clapDistance\":";
    jsonString += String(clapDistance, 3); // 3 decimal places for distance
    jsonString += "}";
    ESP_LOGI("WEB", "Client Clap received: %s", jsonString.c_str());
    events.send(jsonString.c_str(), "clientClap");
}

void WebServer::setMidiParams(AsyncWebServerRequest *request) {
  // Check for all required parameters
  const char* requiredParams[] = {"minVal", "maxVal", "minSat", "maxSat", "rangeMin", "rangeMax", "minRms", "maxRms", "mode"};
  for (int i = 0; i < 9; ++i) {
    if (!request->hasParam(requiredParams[i])) {
      String msg = "Missing parameter: ";
      msg += requiredParams[i];
      request->send(400, "text/plain", msg);
      return;
    }
  }

  // Parse parameters from GET request
  int minVal = request->getParam("minVal")->value().toInt();
  int maxVal = request->getParam("maxVal")->value().toInt();
  int minSat = request->getParam("minSat")->value().toInt();
  int maxSat = request->getParam("maxSat")->value().toInt();
  int rangeMin = request->getParam("rangeMin")->value().toInt();
  int rangeMax = request->getParam("rangeMax")->value().toInt();
  float rmsMin = request->getParam("minRms")->value().toFloat();
  float rmsMax = request->getParam("maxRms")->value().toFloat();
  int mode = request->getParam("mode")->value().toInt();

  ESP_LOGI("WEB", "Set Midi Params minVal %d, maxVal %d, minRms %.2f, maxRMS %.2f", minVal, maxVal, rmsMin, rmsMax);
  messageHandlerInstance->setMidiParams(minVal, maxVal, minSat, maxSat, rangeMin, rangeMax, rmsMin, rmsMax, mode);
  request->send(200, "text/html", "MIDI parameters set");
}

void WebServer::getMidiParams(AsyncWebServerRequest *request) {
  message_midi_params midiParams = messageHandlerInstance->getMidiParams();
  String jsonString = "{";
  jsonString += "\"minVal\":";
  jsonString += String(midiParams.valMin);
  jsonString += ",\"maxVal\":";
  jsonString += String(midiParams.valMax);
  jsonString += ",\"minSat\":";
  jsonString += String(midiParams.satMin);
  jsonString += ",\"maxSat\":";
  jsonString += String(midiParams.satMax);
  jsonString += ",\"rangeMin\":";
  jsonString += String(midiParams.rangeMin);
  jsonString += ",\"rangeMax\":";
  jsonString += String(midiParams.rangeMax);
  jsonString += ",\"minRms\":";
  jsonString += String(midiParams.rmsMin, 2);
  jsonString += ",\"maxRms\":";
  jsonString += String(midiParams.rmsMax, 2);
  jsonString += ",\"mode\":";
  jsonString += String(midiParams.mode);
  jsonString += "}";
  ESP_LOGI("WEB", "GetMidiParams: %s", jsonString.c_str());
  // Send the JSON response
  request->send(200, "text/html", jsonString.c_str());
}

void WebServer::setSleepTime(AsyncWebServerRequest *request) {
  int hours = request->getParam("hours")->value().toInt();
  int minutes = request->getParam("minutes")->value().toInt();
  int seconds = request->getParam("seconds")->value().toInt();
  // Set the time to the specified hours and minutes
  messageHandlerInstance->setSleepTime(hours, minutes, seconds);
}
void WebServer::setWakeupTime(AsyncWebServerRequest *request) {
  int hours = request->getParam("hours")->value().toInt();
  int minutes = request->getParam("minutes")->value().toInt();
  int seconds = request->getParam("seconds")->value().toInt();
  // Set the time to the specified hours and minutes
  messageHandlerInstance->setWakeupTime(hours, minutes, seconds);
}

void WebServer::commandMessage(AsyncWebServerRequest *request) {
  if (!request->hasParam("boardId")) {
    request->send(400, "text/plain", "Missing parameter: boardId");
    return;
  }
  int boardId = request->getParam("boardId")->value().toInt();
  message_data commandMessage = messageHandlerInstance->createCommandMessage(CMD_MESSAGE, false);
  memcpy(commandMessage.targetAddress, messageHandlerInstance->getItemFromAddressList(boardId).address, 6);
  messageHandlerInstance->pushToSendQueue(commandMessage);
  request->send(200, "text/html", "OK");
}
#endif