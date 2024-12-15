#include "lamp/LampController.h"
#include "network/NetworkManager.h"

const bool WIPE_EEPROM = false;  // Set to true when you want to wipe EEPROM

LampController lamp;
NetworkManager network(lamp);

void wipeEEPROM() {
    EEPROM.begin(512);
    for (int i = 0; i < 512; i++) {
        EEPROM.write(i, 0);
    }
    EEPROM.commit();
    Serial.println("EEPROM wiped!");
    ESP.restart();  // Restart device
}

void setup() {
    Serial.begin(115200);

    if (WIPE_EEPROM) {
        wipeEEPROM();
    }
    
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