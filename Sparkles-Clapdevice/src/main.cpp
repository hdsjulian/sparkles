v#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
//#include <myDefines.h>
//#include <ESPAsyncTCP.h>
#include <queue>
#include <mutex>
#include <cstdint>
//#include <helperFuncs.h>
//#include <messaging.h>
//#include <stateMachine.h>
// put function declarations here:
int myFunction(int, int);

void setup() {
  // put your setup code here, to run once:
  int result = myFunction(2, 3);
}

void loop() {
  // put your main code here, to run repeatedly:
}

// put function definitions here:
int myFunction(int x, int y) {
  return x + y;
}