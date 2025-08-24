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

void configurePowerSaving() {
    // Disable WiFi if not needed
    WiFi.mode(WIFI_OFF);
    
    // Disable Bluetooth
    btStop();
    
    setCpuFrequencyMhz(10);  // Options: 160, 80, 40, 20, 10 MHz
    
    #if SERIAL_DEBUG
    Serial.println("Power saving configuration applied:");
    Serial.printf("CPU Frequency: %d MHz\n", getCpuFrequencyMhz());
    Serial.printf("XTAL Frequency: %d MHz\n", getXtalFrequencyMhz());
    Serial.printf("APB Frequency: %d Hz\n", getApbFrequency());
    #endif
}

void zeroOutPins() {
    const int pins[] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 20, 21};
    const int numPins = sizeof(pins) / sizeof(pins[0]);
    for (int i = 0; i < numPins; i++) {
        pinMode(pins[i], INPUT_PULLDOWN);
    }
}

void setup() {
    zeroOutPins();
    #if SERIAL_DEBUG
    Serial.begin(115200);
    while (!Serial && (millis() < 3000)) delay(10);
    Serial.println("ESP32-C3 Starting up..."); 
    #endif

    // RGB LED initialization is now handled in LampController::begin()

    if (WIPE_EEPROM) {
        wipeEEPROM();
    }
    
    // Apply power saving configuration before other initializations
    configurePowerSaving();
    
    lamp.begin();
}

void loop() {
    lamp.update();
    
    #if REMOTE_CONTROL_ENABLED && DATA_LOGGING_ENABLED
    // Support both features
    network.update();  // Process web server requests
    
    // Handle data logging when needed
    if (lamp.isDataReadyToSend()) {
        network.sendMonitoringData();
    }
    #elif REMOTE_CONTROL_ENABLED
    // Only remote control
    network.update();
    #elif DATA_LOGGING_ENABLED
    // Only data logging
    if (lamp.isDataReadyToSend()) {
        network.update();
        network.sendMonitoringData();
    }
    #endif
    
    lamp.checkTouchStatus();
    delay(lamp.getSleepTime());
}