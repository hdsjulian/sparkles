#include <Arduino.h>
#include <MyDefines.h>
#include <Version.h>

// put function declarations here:
message_data messageData;


void setup()
{
  Serial.begin(115200);
  esp_log_level_set("*", ESP_LOG_INFO);
  unsigned long long startTime = millis();
  while (!Serial)
  {
    if (millis() - startTime > 3000)
    {
      break;
    }
  }


  // put your setup code here, to run once:
}

void loop()
{

}
