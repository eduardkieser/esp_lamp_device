#include "LampController.h"
#include <Arduino.h>
#include <math.h>

LampController::LampController() {}

void LampController::begin() {
    analogReadResolution(LampConfig::ADC_RESOLUTION);
    analogSetAttenuation(ADC_11db);
    
    ledcSetup(LampConfig::PWM_CHANNEL, 
              LampConfig::PWM_FREQ, 
              LampConfig::PWM_RESOLUTION);
    ledcAttachPin(LampConfig::PWM_PIN, LampConfig::PWM_CHANNEL);
    
    // Configure voltage monitoring pin
    analogSetPinAttenuation(LampConfig::VOLTAGE_PIN, ADC_11db);

    // Get ESP32-C3 unique hardware ID (chip ID)
    esp_serial_number = ESP.getEfuseMac();
    
}

void LampController::update() {
    static int printCounter = 0;
    int rawValue = analogRead(LampConfig::DIMMER_ANALOG_PIN);
    
    switch(mode) {
        case ControlMode::POTENTIOMETER:
            handlePotentiometerMode(rawValue);
            break;
            
        case ControlMode::REMOTE:
            handleRemoteMode(rawValue);
            break;
    }
    
    // Only update PWM if we're not in battery indication mode
    if (indicatorState == BatteryIndicatorState::IDLE) {
        pwmValue = mapExponential(filteredValue, LampConfig::EXP_FACTOR);
        ledcWrite(LampConfig::PWM_CHANNEL, (int)pwmValue);
    }

    updateBatteryVoltage();

    #if SERIAL_DEBUG
    // Print only every 10th cycle with consistent formatting
    if (++printCounter >= 10) {
        char pwmStr[6];
        char voltStr[6];
        
        snprintf(pwmStr, sizeof(pwmStr), "%04.1f", (pwmValue / LampConfig::MAX_PWM) * 100.0f);
        snprintf(voltStr, sizeof(voltStr), "%04.2f", batteryVoltage);

        #if SUPPORT_TOUCH
        int touchValue = touchRead(LampConfig::TOUCH_PIN);
        Serial.printf("Input: %04d, PWM: %s%%, Voltage: %sV, Touch: %d\n", 
                rawValue,
                pwmStr,
                voltStr,
                touchValue
        );
        #else
        Serial.printf("Input: %04d, PWM: %s%%, Voltage: %sV\n", 
                rawValue,
                pwmStr,
                voltStr
        );
        #endif
        
        printCounter = 0;
    }
    #endif
}

bool LampController::isActive() const {
    return pwmValue > (LampConfig::MAX_PWM * 0.001f); // 0.1% threshold
}

float LampController::mapExponential(int input, float exponent) {
    float normalized = static_cast<float>(input) / LampConfig::MAX_ANALOG;
    float exponential = pow(normalized, exponent);
    return exponential * LampConfig::MAX_PWM;
}

void LampController::updateTimings(int rawValue) {
    if(abs(rawValue - lastAnalogValue) > 5) {
        // Significant change detected
        lastChangeTime = millis();
        lastAnalogValue = rawValue;
        sleepTime = 10;  // Fast updates
        inSlowMode = false;
    } else {
        // Check if we should switch to slow mode
        if(!inSlowMode && (millis() - lastChangeTime > SLOW_MODE_TIMEOUT)) {
            sleepTime = 100;  // Slow updates
            inSlowMode = true;
        }
    }
}

void LampController::handlePotentiometerMode(int rawValue) {
    updateTimings(rawValue);
    filteredValue = (LampConfig::ALPHA * rawValue) + 
                    ((1 - LampConfig::ALPHA) * filteredValue);
}

void LampController::handleRemoteMode(int rawValue) {
    // Check if potentiometer has moved significantly
    float percentChange = abs(rawValue - lastPotValue) / (float)LampConfig::MAX_ANALOG * 100.0f;
    if (percentChange > 10.0f) {  // 10% threshold
        mode = ControlMode::POTENTIOMETER;
        lastPotValue = rawValue;
    }
}

void LampController::setRemoteValue(float percentage) {
    // Convert percentage (0-100) to filtered value range (0-MAX_ANALOG)
    float targetValue = (percentage / 100.0f) * LampConfig::MAX_ANALOG;
    filteredValue = targetValue;  // Set initial value
    lastPotValue = analogRead(LampConfig::DIMMER_ANALOG_PIN);
    mode = ControlMode::REMOTE;
}

void LampController::updateBatteryVoltage() {
    int rawVoltage = analogRead(LampConfig::VOLTAGE_PIN);
    
    // Convert ADC reading to voltage
    float pinVoltage = (rawVoltage * LampConfig::VOLTAGE_REFERENCE) / LampConfig::MAX_ANALOG;
    
    // Calculate actual battery voltage using voltage divider formula
    float newVoltage = pinVoltage * LampConfig::VOLTAGE_DIVIDER_RATIO;
    
    // Apply calibration correction
    newVoltage = newVoltage * LampConfig::VOLTAGE_SCALE + LampConfig::VOLTAGE_OFFSET;
    
    // Apply alpha filter
    batteryVoltage = (LampConfig::VOLTAGE_ALPHA * newVoltage) + 
                     ((1 - LampConfig::VOLTAGE_ALPHA) * batteryVoltage);
}

void LampController::checkTouchStatus() {
    #if SUPPORT_TOUCH
    int touchValue = touchRead(LampConfig::TOUCH_PIN);
    
    // Only trigger if lamp is off and touch is detected
    if (touchValue < LampConfig::TOUCH_THRESHOLD && !isActive()) {
        showBatteryStatus();
    }
    #endif
}

void LampController::showBatteryStatus() {
    #if SUPPORT_TOUCH
    int flashes = calculateRequiredFlashes();
    digitalWrite(LampConfig::STATUS_LED, HIGH);

    for (int flash = 0; flash < flashes; flash++) {
        // Ramp up
        for (unsigned long elapsed = 0; elapsed < LampConfig::RAMP_DURATION_MS; elapsed += 5) {
            float brightness = static_cast<float>(elapsed) / LampConfig::RAMP_DURATION_MS;
            int analogEquivalent = brightness * LampConfig::MAX_ANALOG;
            float pwm = mapExponential(analogEquivalent * LampConfig::FLASH_BRIGHTNESS, LampConfig::EXP_FACTOR) ;
            ledcWrite(LampConfig::PWM_CHANNEL, static_cast<int>(pwm));
            delay(5);
        }

        // Ramp down
        for (unsigned long elapsed = 0; elapsed < LampConfig::RAMP_DURATION_MS; elapsed += 5) {
            float brightness = 1.0f - (static_cast<float>(elapsed) / LampConfig::RAMP_DURATION_MS);
            int analogEquivalent = brightness * LampConfig::MAX_ANALOG;
            float pwm = mapExponential(analogEquivalent * LampConfig::FLASH_BRIGHTNESS, LampConfig::EXP_FACTOR) ;
            ledcWrite(LampConfig::PWM_CHANNEL, static_cast<int>(pwm));
            delay(5);
        }

        // Pause between flashes (if not the last flash)
        if (flash < flashes - 1) {
            ledcWrite(LampConfig::PWM_CHANNEL, 0);
            delay(LampConfig::FLASH_PAUSE_MS);
        }
    }

    // Ensure LED and output are off when done
    digitalWrite(LampConfig::STATUS_LED, LOW);
    ledcWrite(LampConfig::PWM_CHANNEL, 0);
    #endif
}

int LampController::calculateRequiredFlashes() const {
    float cellVoltage = batteryVoltage / 4.0f;  // 4-cell setup
    
    if (cellVoltage < LampConfig::BATTERY_LOW_THRESHOLD) {
        return 1;
    } else if (cellVoltage < LampConfig::BATTERY_MEDIUM_THRESHOLD) {
        return 2;
    }
    return 3;
}

uint64_t LampController::getSerialNumber() const { return esp_serial_number; } 