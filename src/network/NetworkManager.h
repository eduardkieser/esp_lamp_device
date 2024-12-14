#pragma once
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include "../config/Config.h"

class NetworkManager {
public:
    NetworkManager();
    void begin();
    void update();
    bool isConfigured();
    
private:
    WebServer server{80};
    DNSServer dnsServer;
    static const byte DNS_PORT = 53;
    
    void setupAP();
    void setupWebServer();
    void handleRoot();
    void handleSave();
    void handleNotFound();
    String getHtmlContent();
}; 