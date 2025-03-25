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
    // we know that 9765 and 12 bit works well ish. (very good resolution, but slightly audible)
    // 19531 and 11 bit looks good enough and is very quiet.
    static const int PWM_FREQ = 19531;      // Lower frequency for C3
    static const int PWM_RESOLUTION = 11;   // Lower resolution for C3
    static const int MAX_PWM = 2047;        // 2^11 - 1
    #else
    static const int PWM_FREQ = 10000;      // Original ESP32 frequency
    static const int PWM_RESOLUTION = 13;   // Original ESP32 resolution
    static const int MAX_PWM = 8191;        // 2^13 - 1
    #endif

    static const int ADC_RESOLUTION = 10;
    static const int MAX_ANALOG = 1023;     // 10-bit
    static constexpr float ALPHA = 0.1f;    // Filter constant
    static constexpr float EXP_FACTOR = 3.0f; // Exponential mapping factor

    // Battery voltage monitoring
    static constexpr float R_UP = 10000.0f;    // 10kΩ
    static constexpr float R_DOWN = 3000.0f;   // 3kΩ
    static constexpr float VOLTAGE_REFERENCE = 3.3f;  // ESP32 reference voltage
    static constexpr float VOLTAGE_DIVIDER_RATIO = (R_UP + R_DOWN) / R_DOWN;
    static constexpr float VOLTAGE_ALPHA = 0.1f;  // Voltage filter constant
    
    // Calibrated based on actual measurements
    static constexpr float VOLTAGE_SCALE = 0.94339f;  // 1.1/1.3 ≈ 0.846
    static constexpr float VOLTAGE_OFFSET = -1.51f;   // No offset needed if scale is correct

    // Battery status thresholds (per cell)
    static constexpr float BATTERY_LOW_THRESHOLD = 3.4f;
    static constexpr float BATTERY_MEDIUM_THRESHOLD = 3.8f;
    
    // Animation configuration
    static constexpr int RAMP_DURATION_MS = 200;  // Duration for each ramp up/down
    static constexpr int FLASH_PAUSE_MS = 0.0;    // Pause between flashes
    static constexpr float FLASH_BRIGHTNESS = 0.25f;  // Duration of each flash

    // Add to LampConfig struct
    #ifndef DATA_LOGGING_ENABLED
    #define DATA_LOGGING_ENABLED false
    #endif

    // Data logging configuration
    static const bool LOGGING_ENABLED = DATA_LOGGING_ENABLED;
    static const unsigned long LOGGING_INTERVAL_MS = 1000;  // Log data every 10 seconds
    // static const unsigned long REPORTING_INTERVAL_MS = 3600000;  // Send to server every hour
    static const unsigned long REPORTING_INTERVAL_MS = 10000;  // Send to server every 10 seconds
    static const unsigned long WIFI_TIMEOUT_MS = 30000;  // WiFi connection timeout (30 seconds)

    // Add alongside DATA_LOGGING_ENABLED
    #ifndef REMOTE_CONTROL_ENABLED
    #define REMOTE_CONTROL_ENABLED true
    #endif

    static const bool REMOTE_ENABLED = REMOTE_CONTROL_ENABLED;

    // Data server configuration
    static constexpr const char* DEFAULT_LOGGING_SERVER_IP = "192.168.68.109";
    static constexpr int DEFAULT_LOGGING_SERVER_PORT = 4999;

    // Development mode configuration
    #ifndef DEV_MODE
    #define DEV_MODE false
    #endif

    // Development WiFi credentials (only used when DEV_MODE is true)
    #if DEV_MODE
    static constexpr const char* DEV_WIFI_SSID = "VirusFactory";  // Replace with your WiFi SSID
    static constexpr const char* DEV_WIFI_PASSWORD = "Otto&Bobbi";  // Replace with your WiFi password
    #endif

    // Try a different pin if 9 doesn't work
    static const int LOW_VOLTAGE_LED_PIN = 7;  // Alternative pin for low voltage warning LED

    // Add these parameters for the low voltage warning
    static constexpr float LOW_VOLTAGE_THRESHOLD = 10.2f;  // Voltage threshold in volts
    static const unsigned long VOLTAGE_CHECK_INTERVAL_MS = 30000;  // Check voltage every 30 seconds
    static const unsigned long LOW_VOLTAGE_LED_TIMEOUT_MS = 10000;  // LED stays on for 10 seconds
};

// Remove these lines from outside the struct
// #ifndef DEV_MODE
// #define DEV_MODE false
// #endif

// #if DEV_MODE
// static constexpr const char* DEV_WIFI_SSID = "VirusFactory";
// static constexpr const char* DEV_WIFI_PASSWORD = "YourPassword";
// #endif 