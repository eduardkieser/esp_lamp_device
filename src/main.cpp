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
    delay(100);
    Serial.println("ESP32-C3 Starting up..."); 

    pinMode(LampConfig::STATUS_LED, OUTPUT);

    if (WIPE_EEPROM) {
        wipeEEPROM();
    }
    
    // Start network manager first
    // network.begin();
    
    // Then initialize lamp
    lamp.begin();
}

void loop() {
    Serial.println("Loop");
    // network.update();
    lamp.update();
    // lamp.checkTouchStatus();
    
    delay(lamp.getSleepTime());
}