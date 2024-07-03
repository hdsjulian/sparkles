#include <webserver.h>
#include <myDefines.h>



webserver::webserver(FS* fs) : server(80), events("/events"), filesystem(fs) {
    // Constructor body. Any additional setup code can go here.
}


void webserver::setup(messaging &Messaging, modeMachine &modeHandler) {
    WiFi.mode(WIFI_STA);
    configRoutes(); 
    messageHandler = &Messaging;
    server.addHandler(&events);
    server.begin();
    stateMachine = &modeHandler;
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
     server.on("/updateDeviceList", HTTP_GET, [this] (AsyncWebServerRequest *request){
        this->updateDeviceList(request);
    });
    server.on("/commandCalibrate", HTTP_GET, [this] (AsyncWebServerRequest *request){
      this->commandCalibrate(request);
    });
    server.on("/commandAnimate", HTTP_GET, [this] (AsyncWebServerRequest *request){
      this->commandAnimate(request);
    });

    server.on("/goodNight", HTTP_GET, [this] (AsyncWebServerRequest *request){
      this->commandGoodNight(request);
    });
    server.on("/goodMorning", HTTP_GET, [this] (AsyncWebServerRequest *request){
      this->commandGoodMorning(request);
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

}

void webserver::handleClientConnect(AsyncEventSourceClient *client) {
    connected = true;
    client->send("");
}

void webserver::updateDeviceList(AsyncWebServerRequest *request) {
    messageHandler->addError("Called UpdateDeviceList");
    msgType = ADDRESS_LIST;

    messageHandler->pushDataToSendQueue(CMD_MSG_SEND_ADDRESS_LIST, request->hasParam("id") ? request->getParam("id")->value().toInt() : -1);
     request->send(200, "text/html", "OK");

}

void webserver::commandCalibrate(AsyncWebServerRequest *request) {
      messageHandler->addError("Called Calibrate");
      if (stateMachine->getMode() == MODE_WAIT_FOR_TIMER) {
        request->send(400);
        return;
      }
      if (stateMachine->getMode() != MODE_CALIBRATE && stateMachine->getMode() != MODE_WAIT_FOR_TIMER) {
        messageHandler->pushDataToSendQueue(CMD_GET_TIMER, 0);
        request->send(204);
        String jsonString;
        jsonString = "{\"status\" : \"true\"}";
        events.send(jsonString.c_str(), "calibrateStatus", millis()); 
        stateMachine->switchMode(MODE_WAIT_FOR_TIMER);
        messageHandler->addError("starting timer mode");

      }
      else if (stateMachine->getMode() == MODE_CALIBRATE) {
        messageHandler->pushDataToSendQueue(CMD_END_CALIBRATION_MODE, -1);
        Serial.println("ENDING CALIBRATION MODE");
        request->send(204);
        String jsonString;
        jsonString = "{\"status\" : \"false\"}";
        events.send(jsonString.c_str(), "calibrateStatus", millis());
        stateMachine->switchMode(MODE_NEUTRAL);
        messageHandler->addError ("Ending calibration. Claps: ");
        messageHandler->addError(String(messageHandler->sendClapTimes.clapCounter));
      }
      
    
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
    messageHandler->pushDataToSendQueue(CMD_START_ANIMATION, -1);
    stateMachine->switchMode(MODE_ANIMATE);
  }
  else if (stateMachine->getMode() == MODE_ANIMATE) {
    request->send(204);
        jsonString = "{\"status\" : \"false\"}";
    events.send(jsonString.c_str(), "animateStatus", millis()); 
    messageHandler->pushDataToSendQueue(CMD_STOP_ANIMATION, -1);
    stateMachine->switchMode(MODE_NEUTRAL);
  }
  }

void webserver::setNeutral(AsyncWebServerRequest *request) {
    messageHandler->pushDataToSendQueue(CMD_STOP_ANIMATION, -1);
    stateMachine->switchMode(MODE_NEUTRAL);

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

  messageHandler->setPositions(boardId, xpos, ypos, zpos);
  //rueberschieben
  messageHandler->pushDataToSendQueue(MSG_SET_POSITIONS, -1);  
  request->send(200, "text/html", "OK");
}
void webserver::setTime(AsyncWebServerRequest *request) {
  messageHandler->addError("Called SetTime");
  int hours = request->getParam("hours")->value().toInt();
  int minutes = request->getParam("minutes")->value().toInt();
  int seconds = request->getParam("seconds")->value().toInt();
  messageHandler->setSetTimeMessage(hours, minutes, seconds);
  messageHandler->pushDataToSendQueue(MSG_SET_TIME, -1);
  request->send(200, "text/html", "OK");
}

void webserver::commandGoodNight(AsyncWebServerRequest *request) {
  messageHandler->addError("Calling good Night");
  int hours = request->getParam("hours")->value().toInt();
  int minutes = request->getParam("minutes")->value().toInt();
  int seconds = request->getParam("seconds")->value().toInt();
  Serial.println("going to sleep at "+String(hours)+":"+String(minutes)); 
  messageHandler->setGoodNightWakeUp(hours, minutes, seconds, true);
  messageHandler->pushDataToSendQueue(MSG_SET_SLEEP_WAKEUP, -1);
}
void webserver::commandGoodMorning(AsyncWebServerRequest *request) {
  messageHandler->addError("Calling good Morning");
  int hours = request->getParam("hours")->value().toInt();
  int minutes = request->getParam("minutes")->value().toInt();
  int seconds = request->getParam("seconds")->value().toInt();
  messageHandler->setGoodNightWakeUp(hours, minutes, seconds, false);
  messageHandler->pushDataToSendQueue(MSG_SET_SLEEP_WAKEUP, -1);
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
  messageHandler->setAnimation(&animationMessage);
  messageHandler->pushDataToSendQueue(MSG_ANIMATION, -1);
  request->send(200, "text/html", "OK");
}