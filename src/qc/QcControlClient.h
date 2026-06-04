#pragma once

#include "../config/Config.h"

#if QC_CONTROL_ENABLED

#include <Arduino.h>
#include <EEPROM.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#include "../lamp/LampController.h"

struct QcProvisioningConfig {
    uint32_t magic;
    char ssid[32];
    char password[64];
    char controllerHost[64];
    uint16_t controllerPort;
    char controllerPath[32];
    char lampId[32];
};

class QcControlClient {
public:
    explicit QcControlClient(LampController& lampCtrl);
    void begin();
    void update();
    void handleProvisioningWrite(const std::string& value);

private:
    static const uint32_t CONFIG_MAGIC = 0x51434C50; // QCLP
    static const int EEPROM_SIZE = 512;
    static const int CONFIG_OFFSET = 128;
    static const unsigned long WIFI_RETRY_MS = 15000;
    static const unsigned long WIFI_CONNECT_TIMEOUT_MS = 10000;
    static const unsigned long WS_RETRY_MS = 3000;
    static const unsigned long TELEMETRY_INTERVAL_MS = 5000;

    enum class Status {
        UNPROVISIONED,
        WIFI_CONNECTING,
        WIFI_NO_AP,
        WIFI_AUTH_FAILED,
        WIFI_HANDSHAKE,
        WIFI_ASSOC_FAILED,
        WIFI_TIMEOUT,
        WIFI_FAILED,
        WS_CONNECTING,
        WS_FAILED,
        CONNECTED,
        INVALID_CONFIG
    };

    LampController* lamp;
    QcProvisioningConfig config{};
    WiFiClient client;
    BLECharacteristic* txCharacteristic = nullptr;
    bool bleStarted = false;
    bool wsConnected = false;
    unsigned long lastWifiAttempt = 0;
    unsigned long lastWsAttempt = 0;
    unsigned long lastTelemetryAt = 0;
    unsigned long lastLedUpdate = 0;
    uint32_t commandSequence = 0;
    Status status = Status::UNPROVISIONED;
    bool statusLedOn = false;
    uint8_t lastWifiDisconnectReason = 0;

    void startBleProvisioning();
    void publishProvisioningStatus(const char* status);
    void handleWifiEvent(arduino_event_id_t event, arduino_event_info_t info);
    void setStatus(Status nextStatus);
    void updateStatusLed();
    void writeStatusLed(bool on);
    Status statusForWifiFailure(wl_status_t wifiStatus) const;
    const char* statusName(Status value) const;
    const char* statusDescription(Status value) const;
    unsigned long statusBlinkInterval(Status value) const;
    bool loadConfig();
    void saveConfig();
    bool parseProvisioningJson(const String& json);
    void connectWifi();
    void connectWebSocket();
    void closeWebSocket();
    void processWebSocket();
    void sendHello();
    void sendTelemetry();
    void sendTextFrame(const String& payload);
    void handleTextFrame(const String& payload);
    void applySetCommand(const String& payload);
    bool parseControllerUrl(const String& url);
    String lampId() const;
    String deviceId() const;
    String readStringValue(const String& json, const char* key) const;
    float readFloatValue(const String& json, const char* key, float fallback) const;
};

#endif
