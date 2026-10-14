// ============================================================================
// ESP32 MAC ADDRESS FINDER UTILITY
// ============================================================================
// Use this utility to retrieve the Wi-Fi MAC address of each dancer's ESP32.
// Upload this sketch to an ESP32, open the Serial Monitor at 115200 baud,
// and copy the formatted hex array directly into 'SLAVE_MACS' in Master_1_slave.ino.
// ============================================================================

#include <WiFi.h>

void setup() {
  Serial.begin(115200);
  delay(2000);              // Allow Serial Monitor time to connect
  
  WiFi.mode(WIFI_STA);
  delay(1000);              // Allow Wi-Fi subsystem to initialize

  uint8_t mac[6];
  WiFi.macAddress(mac);

  Serial.println("\n=======================================================");
  Serial.println("            ESP32 MAC ADDRESS FINDER                  ");
  Serial.println("=======================================================");
  Serial.print("Standard String MAC: ");
  Serial.println(WiFi.macAddress());

  Serial.print("C Array (Copy-Paste): {");
  for (int i = 0; i < 6; i++) {
    Serial.printf("0x%02X%s", mac[i], (i < 5) ? ", " : "};\n");
  }

  Serial.println("-------------------------------------------------------");
  Serial.println("INSTRUCTIONS:");
  Serial.println("1. Copy the 'C Array' line above.");
  Serial.println("2. Paste it into 'SLAVE_MACS' in 'Master_1_slave.ino'");
  Serial.println("   for the dancer number (Dancer 1 to 5) this board belongs to.");
  Serial.println("=======================================================\n");
}

void loop() {
  delay(5000);
}