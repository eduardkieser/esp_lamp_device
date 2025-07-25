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
    static const int LED_R = BOARD_LED_R_PIN;
    static const int LED_G = BOARD_LED_G_PIN;
    static const int LED_B = BOARD_LED_B_PIN;
    // For backward compatibility
    static const int STATUS_LED = LED_R;
    static const int LOW_VOLTAGE_LED_PIN_LOW = 6;
    static const int LOW_VOLTAGE_LED_PIN_HIGH = 9;

    static const int ONBOARD_LED_PIN = 8;
    
    #if SUPPORT_TOUCH
    static const int TOUCH_PIN = BOARD_TOUCH_PIN;
    static const int TOUCH_THRESHOLD = 40;
    #endif

    // PWM channels
    static const int PWM_CHANNEL = 0;        // Main lamp PWM
    static const int RGB_R_CHANNEL = 1;      // Red LED PWM
    static const int RGB_G_CHANNEL = 2;      // Green LED PWM  
    static const int RGB_B_CHANNEL = 3;      // Blue LED PWM
    
    // RGB LED brightness control (30% of full brightness)
    static constexpr float RGB_BRIGHTNESS_SCALE = 0.20f;
    
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
    static constexpr float VOLTAGE_SCALE = 1;  // 1.1/1.3 ≈ 0.846
    static constexpr float VOLTAGE_OFFSET = 0;   // No offset needed if scale is correct

    // Battery status thresholds (per cell) - 3-cell LiPo
    static constexpr float BATTERY_LOW_THRESHOLD = 3.3f;      // Red LED
    static constexpr float BATTERY_MEDIUM_THRESHOLD = 3.5f;   // Yellow LED
    static constexpr int BATTERY_CELLS = 3;                   // 3-cell LiPo battery
    
    // Animation configuration
    static constexpr int RAMP_DURATION_MS = 500;  // Duration for each ramp up/down
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

    // Add these parameters for the low voltage warning
    static constexpr float LOW_VOLTAGE_THRESHOLD = 9.9f;  // Voltage threshold for 3-cell LiPo (3.3V * 3 cells)
    static const unsigned long VOLTAGE_CHECK_INTERVAL_MS = 30000;  // Check voltage every 30 seconds
    static const unsigned long LOW_VOLTAGE_LED_TIMEOUT_MS = 5000;  // LED stays on for 5 seconds
};

// Remove these lines from outside the struct
// #ifndef DEV_MODE
// #define DEV_MODE false
// #endif

// #if DEV_MODE
// static constexpr const char* DEV_WIFI_SSID = "VirusFactory";
// static constexpr const char* DEV_WIFI_PASSWORD = "YourPassword";
// #endif 