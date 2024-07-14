#include <webserver.h>
#include <myDefines.h>



webserver::webserver(FS* fs) : server(80), events("/events"), filesystem(fs) {
    // Constructor body. Any additional setup code can go here.
}


void webserver::setup(messaging &Messaging, modeMachine &modeHandler) {
  debugVariable++;
    WiFi.mode(WIFI_AP_STA);
    Serial.println("setting up webserver "+String(ssid));
    WiFi.softAP(ssid, password);  
    debugVariable++;
    configRoutes(); 
    debugVariable++;
    messageHandler = &Messaging;
    server.addHandler(&events);
    server.begin();
    stateMachine = &modeHandler;
    debugVariable++;
}


void webserver::setWifi() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(ssid, password);  
}




void webserver::configRoutes() {
  events.onConnect([this](AsyncEventSourceClient *client){
    // send event with message "hello!", id current millis
    // and set reconnect delay to 1 second
    this->handleClientConnect(client);
  });
    server.onNotFound([this](AsyncWebServerRequest *request) {
        this->serveStaticFile(request);
    });

    server.on("/commandCalibrate", HTTP_GET, [this] (AsyncWebServerRequest *request){
      this->commandCalibrate(request);
    });
    server.on("/commandCalculate", HTTP_GET, [this] (AsyncWebServerRequest *request){
      this->commandCalculate(request);
    });
    server.on("/endCalibration", HTTP_GET, [this] (AsyncWebServerRequest *request){
      this->endCalibration(request);
    } );
    server.on("/commandAnimate", HTTP_GET, [this] (AsyncWebServerRequest *request){
      this->commandAnimate(request);
    });

    server.on("/goodNight", HTTP_GET, [this] (AsyncWebServerRequest *request){
      this->commandGoodNight(request);
    });
    server.on("/goodMorning", HTTP_GET, [this] (AsyncWebServerRequest *request){
      this->commandSetWakeup(request);
    });

    server.on("/submitPositions", HTTP_GET, [this] (AsyncWebServerRequest *request){

      this->submitPositions(request);
    });
    server.on("/setTime", HTTP_GET, [this] (AsyncWebServerRequest *request){
      this->setTime(request);
    });
    server.on("/neutral", HTTP_GET, [this] (AsyncWebServerRequest *request){
      this->setNeutral(request);
    });
    server.on("/sendSyncAsyncAnimation", HTTP_GET, [this] (AsyncWebServerRequest *request){
      this->sendSyncAsyncAnimation(request);
    });
    server.on("/updateDeviceList", HTTP_GET, [this] (AsyncWebServerRequest *request){
      this->updateDeviceList(request);
    });
        server.on("/updateStatus", HTTP_GET, [this] (AsyncWebServerRequest *request){
      this->statusUpdate(request);
    });
        server.on("/confirmClap", HTTP_GET, [this] (AsyncWebServerRequest *request){
      this->confirmClap(request);
    });
      server.on("/cancelClap", HTTP_GET, [this] (AsyncWebServerRequest *request){
    this->cancelClap(request);
    });
    server.on("/resetCalibration", HTTP_GET, [this] (AsyncWebServerRequest *request){
    this->resetCalibration(request);
    });
    server.on("/updateCalibrationStatus", HTTP_GET, [this] (AsyncWebServerRequest *request){
    this->updateCalibrationStatus(request);
    });
    server.on("/submitPdParams", HTTP_GET, [this] (AsyncWebServerRequest *request){ 
      this->submitPdParams(request);
    });   

}

void webserver::handleClientConnect(AsyncEventSourceClient *client) {
    connected = true;
    client->send("");
}


void webserver::commandCalibrate(AsyncWebServerRequest *request) {
  messageHandler->addError("Called Calibrate");
  if (stateMachine->getMode() != MODE_NEUTRAL or stateMachine->getMode() == MODE_CALIBRATE) {  
    request->send(400);
    Serial.println("sending 400");
    return;
  }
    if (stateMachine->getMode() == MODE_NEUTRAL or stateMachine->getMode() == MODE_MASTERCLAP_OCCURRED) {
      messageHandler->sendCommand(CMD_START_CALIBRATION_MODE);
      String jsonString;
      jsonString = "{\"status\" : \""+String(CALIBRATION_IN_PROGRESS)+"\"}";
      calibrationStatus == CALIBRATION_IN_PROGRESS;
      request->send(200, "text/html", jsonString.c_str()); 
      stateMachine->switchMode(MODE_CALIBRATE);
      messageHandler->addError("starting calibration mode\n");
      messageHandler->startCalibrationMode();
    }
}

void webserver::updateCalibrationStatus(AsyncWebServerRequest *request) {
      String jsonString;
      jsonString = "{\"status\" : \""+String(calibrationStatus)+"\"}";
      request->send(200, "text/html", jsonString.c_str()); 
}

void webserver::endCalibration(AsyncWebServerRequest *request) {
      messageHandler->addError("Called Calibrate");
        messageHandler->sendMode(MODE_NEUTRAL);
        Serial.println("ENDING CALIBRATION MODE\n");
        //request->send(204);
        String jsonString;
        calibrationStatus = CALIBRATION_ENDED;
        jsonString = "{\"status\" : \""+String(CALIBRATION_ENDED)+"\"}";
        request->send(200, "text/html", jsonString.c_str()); 


}
void webserver::commandCalculate(AsyncWebServerRequest *request) {
      messageHandler->addError("Called Calculate");
      stateMachine->switchMode(MODE_GET_CALIBRATION_DATA);
      messageHandler->getClapTimes(-1);
      String jsonString;
      jsonString = "{\"status\" : \""+String(CALIBRATION_NOT_HAPPENED)+"\"}";
      request->send(200, "text/html", jsonString.c_str());
}

void webserver::commandAnimate(AsyncWebServerRequest *request) {
  messageHandler->addError("Called Animate");
  String jsonString;

  if (stateMachine->getMode() == MODE_WAIT_FOR_TIMER || stateMachine->getMode() == MODE_CALIBRATE) {
    request->send(400);
    return;
  }
  else if (stateMachine->getMode() == MODE_NEUTRAL) {
    request->send(204);
    jsonString = "{\"status\" : \"true\"}";
    events.send(jsonString.c_str(), "animateStatus", millis()); 
    //messageHandler->pushDataToSendQueue(CMD_START_ANIMATION, -1);
    stateMachine->switchMode(MODE_ANIMATE);
  }
  else if (stateMachine->getMode() == MODE_ANIMATE) {
    request->send(204);
        jsonString = "{\"status\" : \"false\"}";
    events.send(jsonString.c_str(), "animateStatus", millis()); 
    //messageHandler->pushDataToSendQueue(CMD_STOP_ANIMATION, -1);
    stateMachine->switchMode(MODE_NEUTRAL);
  }
  }

void webserver::setNeutral(AsyncWebServerRequest *request) {
    stateMachine->switchMode(MODE_NEUTRAL);
    messageHandler->switchMode(MODE_NEUTRAL);
}


void webserver::serveStaticFile(AsyncWebServerRequest *request) {
  // Get the file path from the request
  String path = request->url();

  // Check if the file exists
  if (path == "/" || path == "/index.html") { // Modify this condition as needed
    path = "/addressList.html"; // Adjust the file path here
  }
  Serial.print("asked for static file");
  Serial.println(path);
  // Check if the file exists
  if (LittleFS.exists(path)) {
      // Open the file for reading
      File file = LittleFS.open(path, "r");
      if (file) {
        // Read the contents of the file into a String
        String fileContent;
        while (file.available()) {
          fileContent += char(file.read());
        }

        // Close the file
        file.close();

        // Send the file content as response
        request->send(200, "text/html", fileContent);
        return;
      }
  }

  // If file not found, send 404
  request->send(404, "text/plain", "File not found");
}

void webserver::submitPositions(AsyncWebServerRequest *request) {
  messageHandler->addError("Called SubmitPositions");
  float xpos = request->getParam("xpos")->value().toFloat();
  float ypos = request->getParam("ypos")->value().toFloat();
  float zpos = request->getParam("zpos")->value().toFloat();
  int boardId = request->getParam("boardId")->value().toInt();

  //messageHandler->setPositions(boardId, xpos, ypos, zpos);
  //rueberschieben
  //messageHandler->pushDataToSendQueue(MSG_SET_POSITIONS, -1);  
  request->send(200, "text/html", "OK");
}

void webserver::confirmClap(AsyncWebServerRequest *request) {
  Serial.println("called ConfirmClap");
  messageHandler->addError("Called ConfirmClap");
  if (request->hasParam("xpos") && request->hasParam("ypos") && request->hasParam("zpos") && request->hasParam("clapId")) {
      float xpos = request->getParam("xpos")->value().toFloat();
      float ypos = request->getParam("ypos")->value().toFloat();
      float zpos = request->getParam("zpos")->value().toFloat();
      int clapId = request->getParam("clapId")->value().toInt();
      Serial.println("calling msghandlerconfirmclap");
      messageHandler->confirmClap(clapId, xpos, ypos, zpos);
      calibrationStatus = CALIBRATION_IN_BETWEEN;
      String jsonString;
      jsonString = "{\"status\" : \""+String(calibrationStatus)+"\"}";
      request->send(200, "text/html", jsonString.c_str( ));

      // Process parameters
  } else {
      // Handle missing parameters
      request->send(400, "text/plain", "Missing parameters");
  }
  //messageHandler->setPositions(boardId, xpos, ypos, zpos);
  //rueberschieben
  //messageHandler->pushDataToSendQueue(MSG_SET_POSITIONS, -1);  
}
void webserver::cancelClap(AsyncWebServerRequest *request) {
  messageHandler->addError("Called CancelClap");
  int clapId = request->getParam("clapId")->value().toInt();
  messageHandler->deleteClap(clapId);
  calibrationStatus = CALIBRATION_IN_BETWEEN;
  request->send(200, "text/html", "{\"status\" : \""+String(calibrationStatus)+"\"}");
  //messageHandler->setPositions(boardId, xpos, ypos, zpos);
  //rueberschiebenudn z
  //messageHandler->pushDataToSendQueue(MSG_SET_POSITIONS, -1);  
}
void webserver::resetCalibration(AsyncWebServerRequest *request) {
  messageHandler->addError("Called CancelClap");
  messageHandler->resetCalibration();
  calibrationStatus = CALIBRATION_NOT_HAPPENED;
  request->send(200, "text/html", "{\"status\" : \""+String(calibrationStatus)+"\"}");

  //messageHandler->setPositions(boardId, xpos, ypos, zpos);
  //rueberschieben
  //messageHandler->pushDataToSendQueue(MSG_SET_POSITIONS, -1);  
}
void webserver::setTime(AsyncWebServerRequest *request) {
  messageHandler->addError("Called SetTime");
  int year = request->getParam("year")->value().toInt();
  int month = request->getParam("month")->value().toInt();
  int day = request->getParam("day")->value().toInt();
  int hours = request->getParam("hours")->value().toInt();
  int minutes = request->getParam("minutes")->value().toInt();
  int seconds = request->getParam("seconds")->value().toInt();
  // Assuming setClock can now handle year, month, and day in addition to hours, minutes, and seconds
  messageHandler->setClock(year, month, day, hours, minutes, seconds);
  request->send(200, "text/html", "OK");
}

void webserver::statusUpdate(AsyncWebServerRequest *request) {
  String returnString = "{\"status\":\""+stateMachine->modeToText(stateMachine->getMode())+"\"}";
  request->send(200, "text/html", returnString.c_str());
  //events.send(stateMachine->modeToText(stateMachine->getMode()).c_str(), "statusUpdate");
}
void webserver::statusUpdate() {
  String returnString = "{\"status\":\""+stateMachine->modeToText(stateMachine->getMode())+"\"}";
  events.send(returnString.c_str(), "statusUpdate");
}

void webserver::commandGoodNight(AsyncWebServerRequest *request) {
  messageHandler->addError("Calling good Night");
  int hours = request->getParam("hours")->value().toInt();
  int minutes = request->getParam("minutes")->value().toInt();
  int seconds = request->getParam("seconds")->value().toInt();
  Serial.println("going to sleep at "+String(hours)+":"+String(minutes)); 
  messageHandler->setGoodNight(hours, minutes, seconds);
  //messageHandler->pushDataToSendQueue(MSG_SET_SLEEP_WAKEUP, -1);
  request->send(200, "text/html", "OK");
}
void webserver::commandSetWakeup(AsyncWebServerRequest *request) {
  messageHandler->addError("Calling good Morning");
  int hours = request->getParam("hours")->value().toInt();
  int minutes = request->getParam("minutes")->value().toInt();
  int seconds = request->getParam("seconds")->value().toInt();
  messageHandler->setWakeup(hours, minutes, seconds);
  request->send(200, "text/html", "OK");
}
void webserver::updateDeviceList(AsyncWebServerRequest *request) {
  if (request->hasParam("id")) {
    messageHandler->updateDevice(request->getParam("id")->value().toInt());
  }
  else {
    messageHandler->addError("Updating Device list");
    String jsonString = messageHandler->allClientAddressesToJson();
    Serial.println(jsonString);
    request->send(200, "text/html", jsonString.c_str( ));
  }
}
void webserver::updateDeviceList() {
  messageHandler->addError("Updating Device list");
  String jsonString = messageHandler->allClientAddressesToJson();
  Serial.println(jsonString);
  events.send(jsonString.c_str( ), "updateDeviceList");
}

void webserver::sendSyncAsyncAnimation(AsyncWebServerRequest *request) {
  messageHandler->addError("Called SendSyncAsyncAnimation");
  message_animate animationMessage;
  //memset(animationMessage, 0, sizeof(animationMessage));
  animationMessage.messageType = MSG_ANIMATION;
  animationMessage.speed = request->getParam("speed")->value().toInt();
  animationMessage.animationType = SYNC_ASYNC_BLINK;
  animationMessage.pause = request->getParam("pause")->value().toInt();
  animationMessage.reps = request->getParam("reps")->value().toInt();
  animationMessage.rgb1[0] = request->getParam("red")->value().toInt();
  animationMessage.rgb1[1] = request->getParam("green")->value().toInt();
  animationMessage.rgb1[2] = request->getParam("blue")->value().toInt();
  animationMessage.spread_time = request->getParam("spread")->value().toInt();
  animationMessage.exponent = request->getParam("exponent")->value().toFloat();
  animationMessage.animationreps = request->getParam("anireps")->value().toInt();
  //messageHandler->setAnimation(&animationMessage);
  //messageHandler->pushDataToSendQueue(MSG_ANIMATION, -1);
  request->send(200, "text/html", "OK");
}

void webserver::updateMode(String modeText) {
  Serial.println("switched to "+modeText);
    events.send(modeText.c_str(), "statusUpdate");
}

void webserver::submitPdParams(AsyncWebServerRequest *request) {
  messageHandler->addError("Called SubmitPdParams");

  lag = request->getParam("lag")->value().toInt();
  threshold = request->getParam("threshold")->value().toInt();
  influence = request->getParam("influence")->value().toFloat();
  Serial.println("called SubmitParams lag"+String(lag)+" threshold "+String(threshold)+" influence "+String(influence));
  PdParamsChanged = true;
  request->send(200, "text/html", "OK");
}
