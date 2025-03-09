#include "NetworkManager.h"

NetworkManager::NetworkManager(LampController& lampCtrl) : lamp(&lampCtrl) {}

void NetworkManager::begin() {
    EEPROM.begin(512);
    loadConfig();
    
    #if REMOTE_CONTROL_ENABLED && DATA_LOGGING_ENABLED
    // Both features enabled - prioritize remote control
    if (loadConfig() && tryConnect(wifiConfig.ssid, wifiConfig.password)) {
        inAPMode = false;
        setupStation();
    } else {
        inAPMode = true;
        setupAP();
    }
    #elif REMOTE_CONTROL_ENABLED
    // Only remote control enabled - normal operation
    if (loadConfig() && tryConnect(wifiConfig.ssid, wifiConfig.password)) {
        inAPMode = false;
        setupStation();
    } else {
        inAPMode = true;
        setupAP();
    }
    #elif DATA_LOGGING_ENABLED
    // Only data logging enabled - WiFi off by default
    WiFi.mode(WIFI_OFF);
    #else
    // Basic mode - no network features
    WiFi.mode(WIFI_OFF);
    #endif
}

bool NetworkManager::loadConfig() {
    WiFiConfig config;
    EEPROM.get(0, config);
    
    // Add debug prints
    Serial.println("Loaded WiFi config:");
    Serial.print("SSID: ");
    Serial.println(config.ssid);
    Serial.print("Password: ");
    Serial.println(config.password);
    Serial.print("Configured: ");
    Serial.println(config.configured);
    
    wifiConfig = config;  // Store the config
    return config.configured;
}

void NetworkManager::saveConfig(const char* ssid, const char* pass) {
    WiFiConfig config;
    strncpy(config.ssid, ssid, sizeof(config.ssid));
    strncpy(config.password, pass, sizeof(config.password));
    config.configured = true;
    EEPROM.put(0, config);
    EEPROM.commit();
}

bool NetworkManager::tryConnect(const char* ssid, const char* pass, int timeout) {
    Serial.print("Attempting to connect to WiFi SSID: ");
    Serial.println(ssid);
    
    WiFi.begin(ssid, pass);
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < timeout) {
        delay(1000);
        Serial.print(".");
        attempts++;
    }
    Serial.println();
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Successfully connected to WiFi!");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
        return true;
    } else {
        Serial.println("Failed to connect to WiFi");
        return false;
    }
}

void NetworkManager::setupStation() {
    // Add CORS headers to all responses
    server.enableCORS(true);  // If available in your ESP8266WebServer version
    
    // Or manually add CORS headers to each endpoint:
    server.on("/api/status", HTTP_GET, [this]() {
        server.sendHeader("Access-Control-Allow-Origin", "*");
        server.sendHeader("Access-Control-Allow-Methods", "GET");
        server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
        
        float normalizedValue = (lamp->getCurrentValue() / LampConfig::MAX_ANALOG) * 100.0;
        String json = "{\"brightness\":" + String(normalizedValue, 1) + 
                     ",\"deviceName\":\"" + deviceName + "\"" +
                     ",\"batteryVoltage\":" + String(lamp->getBatteryVoltage(), 2) + "}";
        server.send(200, "application/json", json);
    });

    // Add similar headers to other endpoints...

    server.on("/api/test", HTTP_GET, [this]() {
        server.send(200, "application/json", "{\"status\":\"success\"}");
    });

    // to chech this is working you can use curl on the command line:
    // curl http://smartlamp.local/api/test


    server.on("/api/control", HTTP_POST, [this]() {
        if (server.hasArg("brightness")) {
            float brightness = server.arg("brightness").toFloat();
            lamp->setRemoteValue(brightness);
            server.send(200, "application/json", "{\"status\":\"success\"}");
        } else {
            server.send(400, "application/json", "{\"error\":\"missing parameters\"}");
        }
    });

    server.begin();

    if (!setupMDNS()) {
        Serial.println("Failed to start mDNS");
    }
}

void NetworkManager::handleNotFound() {
    server.send(404, "application/json", "{\"error\":\"not found\"}");
}

void NetworkManager::update() {
    if (inAPMode) {
        dnsServer.processNextRequest();
    }
    server.handleClient();
}

void NetworkManager::setupAP() {
    WiFi.mode(WIFI_AP);
    WiFi.softAP("SmartLamp-Setup");
    
    IPAddress apIP(192, 168, 4, 1);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    
    // Configure DNS server to redirect all requests to our IP
    dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
    dnsServer.start(DNS_PORT, "*", apIP);  // Important: catch-all
    
    server.on("/", [this]() { 
        server.send(200, "text/html", getSetupHtml()); 
    });
    
    server.on("/save", [this]() { handleSave(); });
    server.onNotFound([this]() { handleNotFound(); });
    
    server.begin();
}

String NetworkManager::getSetupHtml() {
    return R"html(
        <!DOCTYPE html>
        <html>
        <head>
            <title>SmartLamp Setup</title>
            <meta name="viewport" content="width=device-width, initial-scale=1">
            <style>
                body { font-family: Arial; margin: 20px; }
                input { margin: 10px 0; padding: 5px; width: 100%; }
                button { padding: 10px; margin: 10px 0; }
            </style>
        </head>
        <body>
            <h1>SmartLamp WiFi Setup</h1>
            <form action="/save" method="POST">
                <input type="text" name="ssid" placeholder="WiFi Name" value="VirusFactory">
                <input type="password" name="password" placeholder="WiFi Password" value="Otto&Bobbi">
                <button type="submit">Save</button>
            </form>
        </body>
        </html>
    )html";
}

void NetworkManager::handleSave() {
    if (server.hasArg("ssid") && server.hasArg("password")) {
        saveConfig(
            server.arg("ssid").c_str(),
            server.arg("password").c_str()
        );
        server.send(200, "text/html", "Configuration saved. Device will restart...");
        delay(2000);
        ESP.restart();
    } else {
        server.send(400, "text/plain", "Missing parameters");
    }
}

bool NetworkManager::setupMDNS() {
    String baseName = "smartlamp";
    String deviceName = baseName;
    int suffix = 0;
    
    // Try to find an available name
    while (!MDNS.begin(deviceName.c_str())) {
        suffix++;
        deviceName = baseName + String(suffix);
        Serial.printf("Trying mDNS name: %s\n", deviceName.c_str());
        
        if (suffix > 99) {  // Reasonable limit
            Serial.println("Failed to start mDNS after many attempts");
            return false;
        }
        delay(100);  // Small delay between attempts
    }

    // Successfully registered mDNS name
    Serial.printf("mDNS responder started: %s.local\n", deviceName.c_str());
    
    // Add service
    MDNS.addService("http", "tcp", 80);
    
    // Store the name for future reference
    this->deviceName = deviceName;
    return true;
}

#if DATA_LOGGING_ENABLED
void NetworkManager::enableWiFi() {
    Serial.println("Enabling WiFi for data transmission...");
    WiFi.mode(WIFI_STA);
    wifiStartTime = millis();
    
    if (wifiConfig.configured) {
        WiFi.begin(wifiConfig.ssid, wifiConfig.password);
    }
}

void NetworkManager::disableWiFi() {
    Serial.println("Disabling WiFi to save power...");
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
}

void NetworkManager::sendMonitoringData() {
    // If WiFi is not enabled yet, enable it
    if (WiFi.status() != WL_CONNECTED) {
        // Check if we've been trying too long
        if (wifiStartTime > 0 && millis() - wifiStartTime > LampConfig::WIFI_TIMEOUT_MS) {
            Serial.println("WiFi connection timed out. Disabling WiFi.");
            disableWiFi();
            return;
        }
        
        // If WiFi is off, turn it on
        if (WiFi.getMode() == WIFI_OFF) {
            enableWiFi();
        }
        
        // Not connected yet, try again later
        return;
    }
    
    // WiFi is connected, send data
    String data = lamp->getMonitoringData();
    if (sendDataToServer(data)) {
        Serial.println("Monitoring data sent successfully");
        lamp->clearDataReadyFlag();
        disableWiFi();  // Turn off WiFi after successful transmission
    } else {
        Serial.println("Failed to send monitoring data");
        // Will try again next cycle
    }
}

bool NetworkManager::sendDataToServer(const String& data) {
    HTTPClient http;
    http.begin(loggingServerUrl);
    http.addHeader("Content-Type", "application/json");
    
    int httpResponseCode = http.POST(data);
    
    if (httpResponseCode > 0) {
        Serial.printf("HTTP Response code: %d\n", httpResponseCode);
        String response = http.getString();
        Serial.println(response);
        http.end();
        return true;
    } else {
        Serial.printf("Error code: %d\n", httpResponseCode);
        http.end();
        return false;
    }
}
#endif

#if REMOTE_CONTROL_ENABLED && DATA_LOGGING_ENABLED
bool NetworkManager::isWifiIdle() {
    return (millis() - lastActivityTime > WIFI_IDLE_TIMEOUT);
}

void NetworkManager::handleWifiPowerSaving() {
    // If we've been idle and WiFi is on, turn it off to save power
    if (isWifiIdle() && WiFi.getMode() != WIFI_OFF) {
        disableWiFi();
    }
    
    // If we need to send data or handle a request, turn WiFi on
    if ((lamp->isDataReadyToSend() || server.getClientDisconnected()) && WiFi.getMode() == WIFI_OFF) {
        enableWiFi();
        lastActivityTime = millis();
    }
}
#endif