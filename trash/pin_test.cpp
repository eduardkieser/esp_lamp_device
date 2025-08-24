#include <Arduino.h>

// Define the pins for the on-board LEDs
// These may vary depending on your specific board
#define ONBOARD_LED1 8  // System LED on GPIO8
#define ONBOARD_LED2 3  // RGB LED - Red (if present)
#define ONBOARD_LED3 4  // RGB LED - Green (if present)
#define ONBOARD_LED4 5  // RGB LED - Blue (if present)

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("ESP32-C3 LED Control Test");
  
  // Configure all potential LED pins as outputs
  pinMode(ONBOARD_LED1, OUTPUT);
  pinMode(ONBOARD_LED2, OUTPUT);
  pinMode(ONBOARD_LED3, OUTPUT);
  pinMode(ONBOARD_LED4, OUTPUT);
  
  // Turn off all LEDs
  digitalWrite(ONBOARD_LED1, LOW);
  digitalWrite(ONBOARD_LED2, LOW);
  digitalWrite(ONBOARD_LED3, LOW);
  digitalWrite(ONBOARD_LED4, LOW);
  
  Serial.println("All on-board LEDs should now be off");
  
  // For some boards, the LEDs might be active LOW, so try HIGH if LOW doesn't work
  Serial.println("If LEDs are still on, they might be active LOW");
  Serial.println("Try setting them HIGH instead of LOW");
}

void loop() {
  // Test toggling the system LED (GPIO8)
  Serial.println("Testing GPIO8 (System LED)");
  digitalWrite(ONBOARD_LED1, HIGH);
  delay(500);
  digitalWrite(ONBOARD_LED1, LOW);
  delay(500);
  
  // If you want to test the RGB LED (if present)
  Serial.println("Testing GPIO3-5 (RGB LED if present)");
  // Red
  digitalWrite(ONBOARD_LED2, HIGH);
  delay(500);
  digitalWrite(ONBOARD_LED2, LOW);
  delay(500);
  
  // Green
  digitalWrite(ONBOARD_LED3, HIGH);
  delay(500);
  digitalWrite(ONBOARD_LED3, LOW);
  delay(500);
  
  // Blue
  digitalWrite(ONBOARD_LED4, HIGH);
  delay(500);
  digitalWrite(ONBOARD_LED4, LOW);
  delay(1500);
} 