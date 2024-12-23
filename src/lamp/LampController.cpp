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
}

void LampController::update() {
    static int printCounter = 0;
    int rawValue = analogRead(LampConfig::ANALOG_PIN);
    
    switch(mode) {
        case ControlMode::POTENTIOMETER:
            handlePotentiometerMode(rawValue);
            break;
            
        case ControlMode::REMOTE:
            handleRemoteMode(rawValue);
            break;
    }
    
    pwmValue = mapExponential(filteredValue, LampConfig::EXP_FACTOR);
    ledcWrite(LampConfig::PWM_CHANNEL, (int)pwmValue);

    updateBatteryVoltage();

    // Print only every 10th cycle with consistent formatting
    if (++printCounter >= 10) {
        char pwmStr[6];
        char voltStr[6];
        
        // Format PWM percentage to always show 4 chars (including decimal point)
        snprintf(pwmStr, sizeof(pwmStr), "%04.1f", (pwmValue / LampConfig::MAX_PWM) * 100.0f);
        // Format voltage to always show 4 chars (including decimal point)
        snprintf(voltStr, sizeof(voltStr), "%04.2f", batteryVoltage);
        
        Serial.printf("Input: %04d, PWM: %s%%, Voltage: %sV\n", 
            rawValue,
            pwmStr,
            voltStr
        );
        
        printCounter = 0;
    }
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
    lastPotValue = analogRead(LampConfig::ANALOG_PIN);
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