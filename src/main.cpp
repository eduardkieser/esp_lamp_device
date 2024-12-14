#include <Arduino.h>
#include <math.h>
#include <WiFi.h>  
#include "esp_wifi.h"
#include "esp_bt.h"
#include "esp_sleep.h"

// Pin definitions
const int analogPin = 15;  // Input pin
const int pwmPin = 13;    // Output pin

// PWM properties
const int pwmChannel = 0;  // LEDC channel (0-15)
const int pwmFreq = 5000;  // PWM frequency in Hz
const int pwmResolution = 12;  // 12-bit PWM (0-4095)

// Maximum values for mapping
const int maxAnalog = 1023; // 10-bit ADC (0-1023)
const int maxPWM = 4095; // 12-bit PWM (0-4095)

// Alpha filter constant (0-1)
// Lower = more smoothing but slower response
// Higher = less smoothing but faster response
const float alpha = 0.1;

// Variable to store filtered value
float filteredValue = 0;

// Variable to track startup time
unsigned long startTime;
const unsigned long SERIAL_TIMEOUT = 60000; // 30 seconds in milliseconds
bool serialActive = true;

// We will also track the last time the PWM was updated
// if it wasn't updated in 10 seconds we will set the delay time to 100ms and when it was updated we will set it to 10ms
unsigned long lastPWMUpdate = 0;
int lastAnalogValue = 0;
int sleepTime = 100;
int lowPowerDelay = 1000;

float mapExponential(int input, float exponent) {
  // Convert input to 0-1 range, apply exponential, then scale back to PWM range
  float normalized = static_cast<float>(input) / maxAnalog;
  float exponential = pow(normalized, exponent);
  return exponential * maxPWM;
}



void setup() {
  // Reduce CPU frequency
  setCpuFrequencyMhz(80);

  startTime = millis();
  lastPWMUpdate = startTime;
  
  // Disable WiFi & Bluetooth
  WiFi.mode(WIFI_OFF);
  btStop();
  
  Serial.begin(115200);
  
  analogReadResolution(10);  // 10-bit ADC (0-1023)
  analogSetAttenuation(ADC_11db);
  
  ledcSetup(pwmChannel, pwmFreq, pwmResolution);
  ledcAttachPin(pwmPin, pwmChannel);
}

void loop() {

  if(serialActive && (millis() - startTime > SERIAL_TIMEOUT)) {
    Serial.println("Disabling Serial output for power saving...");
    Serial.flush();  // Make sure all data is sent
    Serial.end();    // Disable Serial
    serialActive = false;
  }


  // Read raw value
  int rawValue = analogRead(analogPin);

  if(abs(rawValue - lastAnalogValue) > 5) {
    lastPWMUpdate = millis();
    lastAnalogValue = rawValue;
    sleepTime = 10;
  }else{
    if(millis() - lastPWMUpdate > 5000) {
      sleepTime = 100;
    }
  }

  

  
  // Apply alpha filter
  filteredValue = (alpha * rawValue) + ((1 - alpha) * filteredValue);
  
  // Apply exponential mapping to filtered value
  float pwmValue = mapExponential(filteredValue, 4.0);
  
  // Set PWM duty cycle
  ledcWrite(pwmChannel, (int)pwmValue);
  
  // Only print if Serial is still active
  if(serialActive) {
    Serial.println("Sleep time: " + String(sleepTime) +" Filtered value"+String(filteredValue)+ " pwm value:" + String(pwmValue)) ;

  }
  
  // Use light sleep instead of delay
  // esp_sleep_enable_timer_wakeup(50000);   // 50ms
  // esp_light_sleep_start();   

  // if pwm value is 0, we will use esp_sleep instead of delay
  if( (pwmValue < 0.05) && (sleepTime==100)) {
    Serial.println("Sleeping for 5 seconds");
    esp_sleep_enable_timer_wakeup(5000000);
    esp_light_sleep_start();
  }else{
    delay(sleepTime);
  }

  // delay(sleepTime);  // Milliseconds (I think)
}