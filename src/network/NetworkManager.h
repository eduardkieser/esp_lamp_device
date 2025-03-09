#pragma once
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <EEPROM.h>
#include <ESPmDNS.h>  
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
    const char* loggingServerUrl = "http://your-server-ip:5000/api/log";
    bool sendDataToServer(const String& data);
    #endif
}; 