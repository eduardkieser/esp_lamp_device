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

private:
    ControlMode mode = ControlMode::POTENTIOMETER;  // Default mode
    float filteredValue = 0;
    float pwmValue = 0;
    int lastAnalogValue = 0;
    int lastPotValue = 0;  // For mode switching detection
    int sleepTime = 10;  // Start with fast updates
    unsigned long lastChangeTime = 0;
    bool inSlowMode = false;
    static const unsigned long SLOW_MODE_TIMEOUT = 5000; // 5 seconds without change
    
    float mapExponential(int input, float exponent);
    void updateTimings(int rawValue);
    void handlePotentiometerMode(int rawValue);
    void handleRemoteMode(int rawValue);
}; 