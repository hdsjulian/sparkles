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
    jsonString += "\",\"battery\":\"";
    jsonString += String(address.batteryPercentage);
    jsonString += "\", \"distance\":\"";
    jsonString += String(address.distanceFromCenter);
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
  int index = request->getParam("index")->value().toInt();
  message_animation animation;
  animation.animationType = BLINK;
  animation.animationParams.blink.brightness = 255;
  animation.animationParams.blink.duration = 500;
  animation.animationParams.blink.repetitions = 3;
  animation.animationParams.blink.hue = 0;
  animation.animationParams.blink.saturation = 0;
  animation.animationParams.blink.startTime = micros()+1000000;
  messageHandlerInstance->sendAnimation(animation, index);
  request->send(200, "text/html", "OK");
}
#endif