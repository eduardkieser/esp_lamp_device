#pragma once
#include "../config/Config.h"
#include <cstdint>
#include <Arduino.h>

class LampController {
public:
    enum class ControlMode {
        POTENTIOMETER,
        REMOTE
    };

    LampController();
    void begin();
    void update();
    bool isActive() const;
    int getSleepTime() const { return sleepTime; }
    bool canDeepSleep() const { return inSlowMode && !isActive(); }
    float getCurrentValue() const { return filteredValue; }
    void setRemoteValue(float percentage);
    float getBatteryVoltage() const { return batteryVoltage; }
    void checkTouchStatus();
    void turnOnLowVoltageLed();
    void turnOffLowVoltageLed();
    uint64_t getSerialNumber() const;
#if DATA_LOGGING_ENABLED
    String getMonitoringData() const;
    bool isDataReadyToSend() const { return dataReadyToSend; }
    void clearDataReadyFlag() { dataReadyToSend = false; }
#endif

private:
    ControlMode mode = ControlMode::POTENTIOMETER;
    float filteredValue = 0;
    float pwmValue = 0;
    int lastAnalogValue = 0;
    int lastPotValue = 0;
    int sleepTime = 10;
    unsigned long lastChangeTime = 0;
    bool inSlowMode = false;
    uint64_t esp_serial_number = 0;
    static const unsigned long SLOW_MODE_TIMEOUT = 5000;
    
    float mapExponential(int input, float exponent);
    void updateTimings(int rawValue);
    void handlePotentiometerMode(int rawValue);
    void handleRemoteMode(int rawValue);
    float batteryVoltage = 0.0f;
    void updateBatteryVoltage();
    void showBatteryStatus();
    int calculateRequiredFlashes() const;

    enum class BatteryIndicatorState {
        IDLE,
        RAMP_UP,
        RAMP_DOWN,
        PAUSE
    };

    BatteryIndicatorState indicatorState = BatteryIndicatorState::IDLE;
    unsigned long animationStartTime = 0;
    int currentFlash = 0;
    int totalFlashes = 0;
    float indicatorBrightness = 0.0f;
    
    void updateBatteryIndicator();
#if DATA_LOGGING_ENABLED
    unsigned long lastLogTime = 0;
    unsigned long lastReportTime = 0;
    bool dataReadyToSend = false;
    bool shouldLogData() const;
    bool shouldReportData() const;
#endif

    // Add these variables
    unsigned long lastVoltageCheckTime = 0;
    unsigned long lowVoltageLedStartTime = 0;
    bool lowVoltageLedActive = false;
    bool wasInactive = true;  // Track if lamp was previously inactive
    bool wasLampOn = false;  // Track previous lamp state
    
    // Add this method
    void checkLowVoltageWarning();

    enum class LampState {
        OFF,
        TURNING_ON,
        ON
    };

    LampState lampState = LampState::OFF;
    int offCycleCount = 0;
    static const int OFF_CYCLE_THRESHOLD = 10;  // Number of cycles to consider lamp "off"
    static constexpr float ON_THRESHOLD = 0.003f;   // 0.3% threshold to consider lamp "on"
    static constexpr float OFF_THRESHOLD = 0.001f;  // 0.1% threshold to consider lamp "off"

    void setStatusLedColor(bool red, bool green, bool blue);
}; 