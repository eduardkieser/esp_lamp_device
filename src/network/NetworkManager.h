#pragma once
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <EEPROM.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include "../config/Config.h"
#include "../lamp/LampController.h"

class NetworkManager {
public:
    NetworkManager(LampController& lampCtrl);
    void begin();
    void update();
    bool isConfigured();
    #if DATA_LOGGING_ENABLED
    void sendMonitoringData();
    #endif
    
private:
    WebServer server{80};
    DNSServer dnsServer;
    static const byte DNS_PORT = 53;
    bool inAPMode = false;
    WiFiConfig wifiConfig;
    LampController* lamp;
    String deviceName;
    bool setupMDNS();
    void setupAP();
    void setupStation();
    void setupWebServer();
    void handleRoot();
    void handleSave();
    void handleNotFound();
    void handleStatus();
    void handleControl();
    String getSetupHtml();
    String getControlHtml();
    bool loadConfig();
    void saveConfig(const char* ssid, const char* pass);
    bool tryConnect(const char* ssid, const char* pass, int timeout = 30);
    #if DATA_LOGGING_ENABLED
    const char* loggingServerUrl = "http://192.168.1.XXX:5000/api/log";  // Replace with your computer's IP
    bool sendDataToServer(const String& data);
    void enableWiFi();
    void disableWiFi();
    unsigned long wifiStartTime = 0;
    #endif
    #if REMOTE_CONTROL_ENABLED && DATA_LOGGING_ENABLED
    unsigned long lastActivityTime = 0;
    static const unsigned long WIFI_IDLE_TIMEOUT = 1800000; // 30 minutes
    bool isWifiIdle();
    void handleWifiPowerSaving();
    #endif
}; 