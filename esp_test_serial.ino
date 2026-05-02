/*
 * Minimal ESP32 Serial Test
 * Tests if ESP32 can output to serial and initialize WiFi
 */

#include <WiFi.h>
#include <esp_wifi.h>

void setup() {
  // Initialize serial
  Serial.begin(115200);
  delay(2000);  // Wait for serial monitor to connect
  
  Serial.println("\n\n================================");
  Serial.println("ESP32 Serial Test Starting...");
  Serial.println("================================");
  
  // Test 1: Basic output
  Serial.println("\n✅ Serial is working!");
  Serial.println("Testing WiFi initialization...\n");
  
  // Test 2: WiFi mode
  WiFi.mode(WIFI_STA);
  Serial.println("✅ WiFi mode set to STA");
  
  // Test 3: Disconnect
  WiFi.disconnect();
  Serial.println("✅ WiFi disconnected");
  delay(100);
  
  // Test 4: Promiscuous mode
  esp_err_t err = esp_wifi_set_promiscuous(true);
  if (err == ESP_OK) {
    Serial.println("✅ Promiscuous mode enabled");
  } else {
    Serial.printf("❌ Promiscuous mode failed: %d\n", err);
  }
  
  // Test 5: Set channel
  err = esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
  if (err == ESP_OK) {
    Serial.println("✅ Channel set to 1");
  } else {
    Serial.printf("❌ Channel set failed: %d\n", err);
  }
  
  Serial.println("\n================================");
  Serial.println("All tests passed! Ready for sniffing.");
  Serial.println("================================\n");
}

void loop() {
  // Print status every 5 seconds
  delay(5000);
  Serial.printf("✓ Still alive at %lu ms\n", millis());
  
  // Blink indicator
  digitalWrite(8, HIGH);
  delay(100);
  digitalWrite(8, LOW);
}
