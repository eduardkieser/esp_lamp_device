#include "lamp/LampController.h"
#include "network/NetworkManager.h"

LampController lamp;
NetworkManager network;

void setup() {
    Serial.begin(115200);
    
    // Start network manager first
    network.begin();
    
    // Then initialize lamp
    lamp.begin();
}

void loop() {
    network.update();
    lamp.update();
    
    if(lamp.canDeepSleep()) {
        // Only enter deep sleep if we're in slow mode and PWM is off
        esp_sleep_enable_timer_wakeup(5000000);
        esp_light_sleep_start();
    } else {
        delay(lamp.getSleepTime());
    }
}