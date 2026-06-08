#import <Arduino.h>
void setup() {
  Serial.begin(115200);
}

void loop() {
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) return;

    // Parse CSV: counter,elapsed_ms,pitch,rms
    int firstComma = line.indexOf(',');
    int secondComma = line.indexOf(',', firstComma + 1);
    int thirdComma = line.indexOf(',', secondComma + 1);

    if (firstComma > 0 && secondComma > firstComma && thirdComma > secondComma) {
      // Extract values
      String counterStr = line.substring(0, firstComma);
      String msStr = line.substring(firstComma + 1, secondComma);
      String pitchStr = line.substring(secondComma + 1, thirdComma);
      String rmsStr = line.substring(thirdComma + 1);

      float pitch = pitchStr.toFloat();
      float rms = rmsStr.toFloat();

      // You can use the parsed values here for further processing
      // For now, do nothing
    }
  }
}