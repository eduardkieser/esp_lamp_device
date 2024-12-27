#pragma once

struct WiFiConfig {
    char ssid[32];
    char password[64];
    bool configured;
};

struct LampConfig {
    static const int ANALOG_PIN = 34;
    static const int PWM_PIN = 13;
    static const int PWM_CHANNEL = 0;
    static const int PWM_FREQ = 10000;
    static const int PWM_RESOLUTION = 12;
    static const int ADC_RESOLUTION = 10;
    static const int MAX_ANALOG = 1023;  // 10-bit
    static const int MAX_PWM = 4095;     // 12-bit
    static constexpr float ALPHA = 0.1f;     // Filter constant
    static constexpr float EXP_FACTOR = 3.0f; // Exponential mapping factor

    // Battery voltage monitoring
    static const int VOLTAGE_PIN = 35;
    static constexpr float R_UP = 100000.0f;    // 100kΩ
    static constexpr float R_DOWN = 10000.0f;   // 10kΩ
    static constexpr float VOLTAGE_REFERENCE = 3.3f;  // ESP32 reference voltage
    static constexpr float VOLTAGE_DIVIDER_RATIO = (R_UP + R_DOWN) / R_DOWN;
    static constexpr float VOLTAGE_ALPHA = 0.1f;  // Voltage filter constant
    
    // Voltage calibration parameters
    static constexpr float VOLTAGE_SCALE =  0.9602f;  // Multiplicative correction
    static constexpr float VOLTAGE_OFFSET = 1.3695f; // Additive correction

    static const int TOUCH_THRESHOLD = 40;
    static const int BUILTIN_LED = 2; 
    // static const int TOUCH_PIN = T0;

    // Battery status thresholds (per cell)
    static constexpr float BATTERY_LOW_THRESHOLD = 3.4f;
    static constexpr float BATTERY_MEDIUM_THRESHOLD = 4.0f;
    
    // Animation configuration
    static constexpr int RAMP_DURATION_MS = 200;  // Duration for each ramp up/down
    static constexpr int FLASH_PAUSE_MS = 0.0;    // Pause between flashes
    static constexpr float FLASH_BRIGHTNESS = 0.25f;  // Duration of each flash
}; 