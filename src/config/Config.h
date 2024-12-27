#pragma once

struct WiFiConfig {
    char ssid[32];
    char password[64];
    bool configured;
};

struct LampConfig {
    // Use build flags for pin configuration
    static const int DIMMER_ANALOG_PIN = BOARD_DIMMER_ANALOG_PIN;
    static const int VOLTAGE_PIN = BOARD_VOLTAGE_PIN;
    static const int PWM_PIN = BOARD_PWM_PIN;
    static const int STATUS_LED = BOARD_LED_PIN;
    
    #if SUPPORT_TOUCH
    static const int TOUCH_PIN = BOARD_TOUCH_PIN;
    static const int TOUCH_THRESHOLD = 40;
    #endif

    static const int PWM_CHANNEL = 0;
    
    #if defined(BOARD_ESP32_C3)
    static const int PWM_FREQ = 5000;      // Lower frequency for C3
    static const int PWM_RESOLUTION = 10;   // Lower resolution for C3
    static const int MAX_PWM = 1023;        // 2^10 - 1
    #else
    static const int PWM_FREQ = 10000;      // Original ESP32 frequency
    static const int PWM_RESOLUTION = 12;   // Original ESP32 resolution
    static const int MAX_PWM = 4095;        // 2^12 - 1
    #endif

    static const int ADC_RESOLUTION = 10;
    static const int MAX_ANALOG = 1023;     // 10-bit
    static constexpr float ALPHA = 0.1f;    // Filter constant
    static constexpr float EXP_FACTOR = 3.0f; // Exponential mapping factor

    // Battery voltage monitoring
    static constexpr float R_UP = 100000.0f;    // 100kΩ
    static constexpr float R_DOWN = 10000.0f;   // 10kΩ
    static constexpr float VOLTAGE_REFERENCE = 3.3f;  // ESP32 reference voltage
    static constexpr float VOLTAGE_DIVIDER_RATIO = (R_UP + R_DOWN) / R_DOWN;
    static constexpr float VOLTAGE_ALPHA = 0.1f;  // Voltage filter constant
    
    // Voltage calibration parameters
    static constexpr float VOLTAGE_SCALE =  0.9602f;  // Multiplicative correction
    static constexpr float VOLTAGE_OFFSET = 1.3695f; // Additive correction

    // Battery status thresholds (per cell)
    static constexpr float BATTERY_LOW_THRESHOLD = 3.4f;
    static constexpr float BATTERY_MEDIUM_THRESHOLD = 4.0f;
    
    // Animation configuration
    static constexpr int RAMP_DURATION_MS = 200;  // Duration for each ramp up/down
    static constexpr int FLASH_PAUSE_MS = 0.0;    // Pause between flashes
    static constexpr float FLASH_BRIGHTNESS = 0.25f;  // Duration of each flash
}; 