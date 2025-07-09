#include "LampController.h"
#include <Arduino.h>
#include <math.h>

LampController::LampController() {}

void LampController::begin() {
    // Initialize RGB status LED pins FIRST to prevent boot-up flash
    pinMode(LampConfig::LED_R, OUTPUT);
    pinMode(LampConfig::LED_G, OUTPUT);
    pinMode(LampConfig::LED_B, OUTPUT);
    setStatusLedColor(false, false, false);  // Ensure all LEDs are off
    
    analogReadResolution(LampConfig::ADC_RESOLUTION);
    analogSetAttenuation(ADC_11db);
    
    ledcSetup(LampConfig::PWM_CHANNEL, 
              LampConfig::PWM_FREQ, 
              LampConfig::PWM_RESOLUTION);
    ledcAttachPin(LampConfig::PWM_PIN, LampConfig::PWM_CHANNEL);
    
    // Configure voltage monitoring pin
    analogSetPinAttenuation(LampConfig::VOLTAGE_PIN, ADC_11db);

    // Add debug output to verify pin configuration
    Serial.printf("Configuring low voltage warning LED on pin %d\n", LampConfig::LOW_VOLTAGE_LED_PIN_LOW);
    Serial.printf("Configuring low voltage warning LED on pin %d\n", LampConfig::LOW_VOLTAGE_LED_PIN_HIGH);
    
    // configure the low voltage LED pins as output
    pinMode(LampConfig::LOW_VOLTAGE_LED_PIN_LOW, OUTPUT);
    pinMode(LampConfig::LOW_VOLTAGE_LED_PIN_HIGH, OUTPUT);
    
    // Make sure the low power LED is off
    digitalWrite(LampConfig::LOW_VOLTAGE_LED_PIN_LOW, LOW);
    digitalWrite(LampConfig::LOW_VOLTAGE_LED_PIN_HIGH, LOW);
    
    // Initialize voltage reading before any checks are performed
    // Take multiple readings to stabilize the value
    for (int i = 0; i < 10; i++) {
        updateBatteryVoltage();
        delay(10);
    }
    
    Serial.printf("Initial battery voltage: %.2fV\n", batteryVoltage);
    
    // turn off the onboard led
    pinMode(LampConfig::ONBOARD_LED_PIN, OUTPUT);
    digitalWrite(LampConfig::ONBOARD_LED_PIN, HIGH);

    // Get ESP32-C3 unique hardware ID (chip ID)
    esp_serial_number = ESP.getEfuseMac();
    
    // Initialize the last voltage check time
    lastVoltageCheckTime = millis();
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
    
    // Update battery indicator animation if active
    if (indicatorState != BatteryIndicatorState::IDLE) {
        updateBatteryIndicator();
    } else {
        // Normal PWM output
        pwmValue = mapExponential(filteredValue, LampConfig::EXP_FACTOR);
        ledcWrite(LampConfig::PWM_CHANNEL, (int)pwmValue);
    }

    updateBatteryVoltage();

    // Check low voltage warning
    checkLowVoltageWarning();

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

    #if DATA_LOGGING_ENABLED
    if (shouldLogData()) {
        lastLogTime = millis();
        
        // Check if it's also time to report data
        if (shouldReportData()) {
            lastReportTime = millis();
            dataReadyToSend = true;
        }
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

void LampController::turnOnLowVoltageLed() {
    // Configure pins as output
    pinMode(LampConfig::LOW_VOLTAGE_LED_PIN_LOW, OUTPUT);
    pinMode(LampConfig::LOW_VOLTAGE_LED_PIN_HIGH, OUTPUT);
    
    // Set the correct states to turn on the LED
    digitalWrite(LampConfig::LOW_VOLTAGE_LED_PIN_LOW, LOW);
    digitalWrite(LampConfig::LOW_VOLTAGE_LED_PIN_HIGH, HIGH);
}

void LampController::turnOffLowVoltageLed() {
    // Properly turn off the LED by setting both pins LOW
    digitalWrite(LampConfig::LOW_VOLTAGE_LED_PIN_LOW, LOW);
    digitalWrite(LampConfig::LOW_VOLTAGE_LED_PIN_HIGH, LOW);
    
    // If you want to save power, you can set pins to INPUT mode
    // pinMode(LampConfig::LOW_VOLTAGE_LED_PIN_LOW, INPUT);
    // pinMode(LampConfig::LOW_VOLTAGE_LED_PIN_HIGH, INPUT);
}

// Helper to set RGB LED color
void LampController::setStatusLedColor(bool red, bool green, bool blue) {
    // Ensure pins are configured as outputs (defensive programming)
    pinMode(LampConfig::LED_R, OUTPUT);
    pinMode(LampConfig::LED_G, OUTPUT);
    pinMode(LampConfig::LED_B, OUTPUT);
    
    digitalWrite(LampConfig::LED_R, red ? HIGH : LOW);
    digitalWrite(LampConfig::LED_G, green ? HIGH : LOW);
    digitalWrite(LampConfig::LED_B, blue ? HIGH : LOW);
}

void LampController::showBatteryStatus() {
    // Calculate voltage per cell for 3-cell LiPo
    float cellVoltage = batteryVoltage / LampConfig::BATTERY_CELLS;
    
    #if SERIAL_DEBUG
    Serial.printf("Battery status check: %.2fV total, %.2fV per cell\n", batteryVoltage, cellVoltage);
    #endif
    
    // Determine color based on voltage per cell
    if (cellVoltage < LampConfig::BATTERY_LOW_THRESHOLD) {
        // Red LED - Low battery (< 3.3V per cell)
        setStatusLedColor(true, false, false);
        #if SERIAL_DEBUG
        Serial.println("Battery status: LOW (Red LED)");
        #endif
    } else if (cellVoltage < LampConfig::BATTERY_MEDIUM_THRESHOLD) {
        // Yellow LED - Medium battery (< 3.5V per cell)
        setStatusLedColor(true, true, false);
        #if SERIAL_DEBUG
        Serial.println("Battery status: MEDIUM (Yellow LED)");
        #endif
    } else {
        // Green LED - Good battery (>= 3.5V per cell)
        setStatusLedColor(false, true, false);
        #if SERIAL_DEBUG
        Serial.println("Battery status: GOOD (Green LED)");
        #endif
    }
    
    // Start the battery indicator animation
    indicatorState = BatteryIndicatorState::RAMP_UP;
    animationStartTime = millis();
    currentFlash = 0;
    totalFlashes = 1;  // Single flash for battery status
    indicatorBrightness = 0.0f;
}

int LampController::calculateRequiredFlashes() const {
    float cellVoltage = batteryVoltage / LampConfig::BATTERY_CELLS;  // 3-cell LiPo
    
    if (cellVoltage < LampConfig::BATTERY_LOW_THRESHOLD) {
        return 1;  // Red - Low battery
    } else if (cellVoltage < LampConfig::BATTERY_MEDIUM_THRESHOLD) {
        return 2;  // Yellow - Medium battery
    }
    return 3;  // Green - Good battery
}

uint64_t LampController::getSerialNumber() const { return esp_serial_number; }

#if DATA_LOGGING_ENABLED
bool LampController::shouldLogData() const {
    return millis() - lastLogTime >= LampConfig::LOGGING_INTERVAL_MS;
}

bool LampController::shouldReportData() const {
    return millis() - lastReportTime >= LampConfig::REPORTING_INTERVAL_MS;
}

String LampController::getMonitoringData() const {
    // Format: JSON with device ID, voltage, and potentiometer position
    char buffer[128];
    snprintf(buffer, sizeof(buffer), 
             "{\"device_id\":\"%016llX\",\"voltage\":%.2f,\"position\":%.1f}", 
             esp_serial_number,
             batteryVoltage,
             (filteredValue / LampConfig::MAX_ANALOG) * 100.0f);
    return String(buffer);
}
#endif

void LampController::checkLowVoltageWarning() {
    unsigned long currentTime = millis();
    float pwmPercentage = pwmValue / LampConfig::MAX_PWM;
    
    // Track current lamp state (on/off) based on PWM percentage
    bool isCurrentlyOn = pwmPercentage > ON_THRESHOLD;
    
    // Detect the transition from off to on
    if (isCurrentlyOn && !wasLampOn) {
        #if SERIAL_DEBUG
        Serial.println("Lamp transition detected: OFF → ON");
        #endif
        
        // Check battery immediately when turning on
        updateBatteryVoltage();
        
        #if SERIAL_DEBUG
        Serial.printf("Battery check on power-on: %.2fV (threshold: %.2fV)\n", 
                     batteryVoltage, LampConfig::LOW_VOLTAGE_THRESHOLD);
        #endif
        
        // Show battery status when transitioning from off to on
        #if SERIAL_DEBUG
        Serial.println("Lamp transition detected: OFF → ON - showing battery status");
        #endif
        showBatteryStatus();
    }
    
    // Update previous state for next cycle
    wasLampOn = isCurrentlyOn;
    

    
    // Periodic voltage check (less frequent to save power)
    if (currentTime - lastVoltageCheckTime >= LampConfig::VOLTAGE_CHECK_INTERVAL_MS) {
        lastVoltageCheckTime = currentTime;
        updateBatteryVoltage();
        #if SERIAL_DEBUG
        Serial.printf("Periodic voltage check: %.2fV\n", batteryVoltage);
        #endif
    }
} 

void LampController::updateBatteryIndicator() {
    unsigned long currentTime = millis();
    unsigned long elapsed = currentTime - animationStartTime;
    
    switch (indicatorState) {
        case BatteryIndicatorState::RAMP_UP:
            if (elapsed >= LampConfig::RAMP_DURATION_MS) {
                // Ramp up complete, switch to ramp down
                indicatorState = BatteryIndicatorState::RAMP_DOWN;
                animationStartTime = currentTime;
            } else {
                // Continue ramping up
                float progress = (float)elapsed / LampConfig::RAMP_DURATION_MS;
                indicatorBrightness = progress;
            }
            break;
            
        case BatteryIndicatorState::RAMP_DOWN:
            if (elapsed >= LampConfig::RAMP_DURATION_MS) {
                // Animation complete, return to idle
                indicatorState = BatteryIndicatorState::IDLE;
                setStatusLedColor(false, false, false);
            } else {
                // Continue ramping down
                float progress = (float)elapsed / LampConfig::RAMP_DURATION_MS;
                indicatorBrightness = 1.0f - progress;
            }
            break;
            
        default:
            indicatorState = BatteryIndicatorState::IDLE;
            break;
    }
} 