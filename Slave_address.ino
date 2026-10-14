#include <WiFi.h>

void setup() {
  Serial.begin(115200);
  delay(2000);              // Give Serial time
  WiFi.mode(WIFI_STA);
  delay(1000);              // Give WiFi time

  Serial.print("MAC: ");
  Serial.println(WiFi.macAddress());
}

void loop() {}