#pragma once
#include "../config/Config.h"

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
    uint64_t getSerialNumber() const;
#if DATA_LOGGING_ENABLED
    String getMonitoringData() const;
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
    bool shouldLogData() const;
#endif
}; 