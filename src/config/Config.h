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
    static const int PWM_FREQ = 5000;
    static const int PWM_RESOLUTION = 12;
    static const int ADC_RESOLUTION = 10;
    static const int MAX_ANALOG = 1023;  // 10-bit
    static const int MAX_PWM = 4095;     // 12-bit
    static constexpr float ALPHA = 0.1f;     // Filter constant
    static constexpr float EXP_FACTOR = 4.0f; // Exponential mapping factor
}; 