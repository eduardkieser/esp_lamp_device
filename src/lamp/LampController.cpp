#include "LampController.h"
#include <Arduino.h>
#include <math.h>

LampController::LampController() {}

void LampController::begin() {
    // Configure RGB LED pins as outputs and set them LOW BEFORE PWM setup
    pinMode(LampConfig::LED_R, OUTPUT);
    pinMode(LampConfig::LED_G, OUTPUT);
    pinMode(LampConfig::LED_B, OUTPUT);
    digitalWrite(LampConfig::LED_R, LOW);
    digitalWrite(LampConfig::LED_G, LOW);
    digitalWrite(LampConfig::LED_B, LOW);
    
    // Initialize RGB status LED PWM channels
    ledcSetup(LampConfig::RGB_R_CHANNEL, LampConfig::PWM_FREQ, LampConfig::PWM_RESOLUTION);
    ledcSetup(LampConfig::RGB_G_CHANNEL, LampConfig::PWM_FREQ, LampConfig::PWM_RESOLUTION);
    ledcSetup(LampConfig::RGB_B_CHANNEL, LampConfig::PWM_FREQ, LampConfig::PWM_RESOLUTION);
    
    // Set PWM duty cycle to 0 before attaching pins to prevent bootup flash
    ledcWrite(LampConfig::RGB_R_CHANNEL, 0);
    ledcWrite(LampConfig::RGB_G_CHANNEL, 0);
    ledcWrite(LampConfig::RGB_B_CHANNEL, 0);
    
    ledcAttachPin(LampConfig::LED_R, LampConfig::RGB_R_CHANNEL);
    ledcAttachPin(LampConfig::LED_G, LampConfig::RGB_G_CHANNEL);
    ledcAttachPin(LampConfig::LED_B, LampConfig::RGB_B_CHANNEL);
    
    setStatusLedColor(false, false, false);  // Ensure all LEDs are off
    
    analogReadResolution(LampConfig::ADC_RESOLUTION);
    analogSetAttenuation(ADC_11db);
    
    ledcSetup(LampConfig::PWM_CHANNEL, 
              LampConfig::PWM_FREQ, 
              LampConfig::PWM_RESOLUTION);
    ledcAttachPin(LampConfig::PWM_PIN, LampConfig::PWM_CHANNEL);
    
    // Configure voltage monitoring pin
    analogSetPinAttenuation(LampConfig::VOLTAGE_PIN, ADC_11db);

    
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
    
    // Always update main PWM output (never block it)
    pwmValue = mapExponential(filteredValue+1, LampConfig::EXP_FACTOR);
    ledcWrite(LampConfig::PWM_CHANNEL, (int)pwmValue);
    
    // Update battery indicator animation if active (runs in parallel)
    if (indicatorState != BatteryIndicatorState::IDLE) {
        updateBatteryIndicator();
    }

    updateBatteryVoltage();

    // Check low voltage warning
    checkLowVoltageWarning();

    #if SERIAL_DEBUG
    // Print only every 10th cycle with consistent formatting
    if (++printCounter >= 10) {
        char pwmStr[6];
        char voltStr[6];
        char rawPwmStr[6];
        char filteredValueStr[6];

        snprintf(pwmStr, sizeof(pwmStr), "%04.1f", (pwmValue / LampConfig::MAX_PWM) * 100.0f);
        snprintf(voltStr, sizeof(voltStr), "%04.2f", batteryVoltage);
        snprintf(rawPwmStr, sizeof(rawPwmStr), "%04d", (int)pwmValue);
        snprintf(filteredValueStr, sizeof(filteredValueStr), "%04d", (int)filteredValue);
        #if SUPPORT_TOUCH
        int touchValue = touchRead(LampConfig::TOUCH_PIN);
        Serial.printf("Input: %04d, PWM: %s%%, Voltage: %sV, Touch: %d\n", 
                rawValue,
                pwmStr,
                voltStr,
                touchValue
        );
        #else
        Serial.printf("Input: %04d, PWM: %s%%, Raw PWM: %s, Filtered: %s, Voltage: %sV\n", 
                rawValue,
                pwmStr,
                rawPwmStr,
                filteredValueStr,
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


// Helper to set RGB LED color with PWM control
void LampController::setStatusLedColor(bool red, bool green, bool blue) {
    // Calculate PWM values with 30% brightness scaling
    int redValue = red ? (int)(LampConfig::MAX_PWM * LampConfig::RGB_BRIGHTNESS_SCALE) : 0;
    int greenValue = green ? (int)(LampConfig::MAX_PWM * LampConfig::RGB_BRIGHTNESS_SCALE) : 0;
    int blueValue = blue ? (int)(LampConfig::MAX_PWM * LampConfig::RGB_BRIGHTNESS_SCALE) : 0;
    
    // Set PWM values for each channel
    ledcWrite(LampConfig::RGB_R_CHANNEL, redValue);
    ledcWrite(LampConfig::RGB_G_CHANNEL, greenValue);
    ledcWrite(LampConfig::RGB_B_CHANNEL, blueValue);
}

void LampController::showBatteryStatus() {
    // Calculate voltage per cell for 3-cell LiPo
    float cellVoltage = batteryVoltage / LampConfig::BATTERY_CELLS;
    
    // Start the battery indicator animation (color will be set in updateBatteryIndicator)
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

        // Check battery immediately when turning on
        updateBatteryVoltage();
        
        showBatteryStatus();
    }
    
    // Update previous state for next cycle
    wasLampOn = isCurrentlyOn;
    

    
    // Periodic voltage check (less frequent to save power)
    if (currentTime - lastVoltageCheckTime >= LampConfig::VOLTAGE_CHECK_INTERVAL_MS) {
        lastVoltageCheckTime = currentTime;
        updateBatteryVoltage();
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
                // Continue ramping up with exponential curve
                float progress = (float)elapsed / LampConfig::RAMP_DURATION_MS;
                indicatorBrightness = pow(progress, LampConfig::EXP_FACTOR);
            }
            break;
            
        case BatteryIndicatorState::RAMP_DOWN:
            if (elapsed >= LampConfig::RAMP_DURATION_MS) {
                // Animation complete, return to idle
                indicatorState = BatteryIndicatorState::IDLE;
                setStatusLedColor(false, false, false);
                return;  // Exit early to prevent overwriting the LED states
            } else {
                // Continue ramping down with exponential curve
                float progress = (float)elapsed / LampConfig::RAMP_DURATION_MS;
                indicatorBrightness = pow(1.0f - progress, LampConfig::EXP_FACTOR);
            }
            break;
            
        default:
            indicatorState = BatteryIndicatorState::IDLE;
            break;
    }
    
    // Apply the calculated brightness to the RGB LEDs
    if (indicatorState != BatteryIndicatorState::IDLE) {
        // Get the current color state and apply brightness
        float cellVoltage = batteryVoltage / LampConfig::BATTERY_CELLS;
        bool red = false, green = false, blue = false;
        
        if (cellVoltage < LampConfig::BATTERY_LOW_THRESHOLD) {
            red = true;  // Red LED
        } else if (cellVoltage < LampConfig::BATTERY_MEDIUM_THRESHOLD) {
            red = true; green = true;  // Yellow LED
        } else {
            green = true;  // Green LED
        }
        
        // Calculate PWM values with exponential brightness scaling
        int redValue = red ? (int)(LampConfig::MAX_PWM * LampConfig::RGB_BRIGHTNESS_SCALE * indicatorBrightness) : 0;
        int greenValue = green ? (int)(LampConfig::MAX_PWM * LampConfig::RGB_BRIGHTNESS_SCALE * indicatorBrightness) : 0;
        int blueValue = blue ? (int)(LampConfig::MAX_PWM * LampConfig::RGB_BRIGHTNESS_SCALE * indicatorBrightness) : 0;

        Serial.printf("Red: %d, Green: %d, Blue: %d\n", redValue, greenValue, blueValue);
        
        // Set PWM values for each channel
        ledcWrite(LampConfig::RGB_R_CHANNEL, redValue);
        ledcWrite(LampConfig::RGB_G_CHANNEL, greenValue);
        ledcWrite(LampConfig::RGB_B_CHANNEL, blueValue);
    }
} 