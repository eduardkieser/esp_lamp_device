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
}

void LampController::update() {
    int rawValue = analogRead(LampConfig::ANALOG_PIN);
    
    switch(mode) {
        case ControlMode::POTENTIOMETER:
            handlePotentiometerMode(rawValue);
            break;
            
        case ControlMode::REMOTE:
            handleRemoteMode(rawValue);
            break;
    }
    
    // Apply exponential mapping and update PWM
    pwmValue = mapExponential(filteredValue, LampConfig::EXP_FACTOR);
    ledcWrite(LampConfig::PWM_CHANNEL, (int)pwmValue);
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