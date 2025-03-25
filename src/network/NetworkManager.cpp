#include "NetworkManager.h"

NetworkManager::NetworkManager(LampController& lampCtrl) : lamp(&lampCtrl) {}

void NetworkManager::begin() {
    EEPROM.begin(512);
    
    #if DEV_MODE
    // In development mode, use hardcoded credentials
    Serial.println("DEVELOPMENT MODE: Using hardcoded WiFi credentials");
    Serial.printf("DEV SSID: %s\n", LampConfig::DEV_WIFI_SSID);
    // Don't print the full password for security, just the first few chars
    Serial.printf("DEV PASSWORD: %.*s***\n", 3, LampConfig::DEV_WIFI_PASSWORD);
    strncpy(wifiConfig.ssid, LampConfig::DEV_WIFI_SSID, sizeof(wifiConfig.ssid));
    strncpy(wifiConfig.password, LampConfig::DEV_WIFI_PASSWORD, sizeof(wifiConfig.password));
    wifiConfig.configured = true;
    #else
    // Normal operation - load from EEPROM
    Serial.println("NORMAL MODE: Loading WiFi config from EEPROM");
    loadConfig();
    #endif
    
    #if REMOTE_CONTROL_ENABLED && DATA_LOGGING_ENABLED
    // Both features enabled - prioritize remote control
    if (wifiConfig.configured && tryConnect(wifiConfig.ssid, wifiConfig.password)) {
        inAPMode = false;
        setupStation();
    } else {
        inAPMode = true;
        setupAP();
    }
    #elif REMOTE_CONTROL_ENABLED
    // Only remote control enabled - normal operation
    if (wifiConfig.configured && tryConnect(wifiConfig.ssid, wifiConfig.password)) {
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
String NetworkManager::getLoggingServerUrl() const {
    return "http://" + String(LampConfig::DEFAULT_LOGGING_SERVER_IP) + 
           ":" + String(LampConfig::DEFAULT_LOGGING_SERVER_PORT) + "/api/log";
}

bool NetworkManager::sendDataToServer(const String& data) {
    HTTPClient http;
    String url = getLoggingServerUrl();
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    
    Serial.print("Sending data to: ");
    Serial.println(url);
    
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

void NetworkManager::enableWiFi() {
    Serial.println("Enabling WiFi for data transmission...");
    WiFi.mode(WIFI_STA);
    wifiStartTime = millis();
    
    #if DEV_MODE
    // In development mode, always use the hardcoded credentials
    Serial.println("DEV MODE: Using hardcoded WiFi credentials");
    Serial.printf("Connecting to %s...\n", LampConfig::DEV_WIFI_SSID);
    WiFi.begin(LampConfig::DEV_WIFI_SSID, LampConfig::DEV_WIFI_PASSWORD);
    #else
    // Normal mode - use stored credentials
    if (wifiConfig.configured) {
        Serial.printf("Connecting to %s...\n", wifiConfig.ssid);
        WiFi.begin(wifiConfig.ssid, wifiConfig.password);
    }
    #endif
}

void NetworkManager::disableWiFi() {
    Serial.println("Disabling WiFi to save power...");
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
}

void NetworkManager::sendMonitoringData() {
    unsigned long currentTime = millis();
    
    // If we've had connection failures, implement a backoff strategy
    if (connectionFailures > 0 && 
        currentTime - lastConnectionAttempt < (CONNECTION_RETRY_INTERVAL * connectionFailures)) {
        // Still in backoff period, don't try again yet
        return;
    }
    
    // If WiFi is not connected
    if (WiFi.status() != WL_CONNECTED) {
        // If WiFi is off, turn it on and attempt to connect
        if (WiFi.getMode() == WIFI_OFF) {
            Serial.println("Enabling WiFi for data transmission...");
            WiFi.mode(WIFI_STA);
            
            #if DEV_MODE
            // In development mode, always use the hardcoded credentials
            Serial.println("DEV MODE: Using hardcoded WiFi credentials");
            Serial.printf("Connecting to %s...\n", LampConfig::DEV_WIFI_SSID);
            WiFi.begin(LampConfig::DEV_WIFI_SSID, LampConfig::DEV_WIFI_PASSWORD);
            lastConnectionAttempt = currentTime;
            #else
            // Normal mode - use stored credentials
            if (wifiConfig.configured) {
                Serial.printf("Connecting to %s...\n", wifiConfig.ssid);
                WiFi.begin(wifiConfig.ssid, wifiConfig.password);
                lastConnectionAttempt = currentTime;
            } else {
                Serial.println("WiFi not configured, cannot connect");
                connectionFailures++;
                return;
            }
            #endif
        }
        
        // Check if we've been trying too long for this attempt
        if (currentTime - lastConnectionAttempt > LampConfig::WIFI_TIMEOUT_MS) {
            Serial.println("WiFi connection attempt timed out");
            disableWiFi();
            connectionFailures++; // Increment failure counter for backoff
            Serial.printf("Connection failures: %d, will retry in %d seconds\n", 
                         connectionFailures, 
                         (CONNECTION_RETRY_INTERVAL * connectionFailures) / 1000);
            return;
        }
        
        // Still trying to connect
        return;
    }
    
    // WiFi is connected, reset failure counter
    connectionFailures = 0;
    
    // Send the data
    String data = lamp->getMonitoringData();
    if (sendDataToServer(data)) {
        Serial.println("Monitoring data sent successfully");
        lamp->clearDataReadyFlag();
        disableWiFi();  // Turn off WiFi after successful transmission
    } else {
        Serial.println("Failed to send monitoring data");
        disableWiFi();  // Turn off WiFi even after failure to prevent battery drain
        // Will try again after backoff
        connectionFailures++;
    }
}

bool NetworkManager::isWifiIdle() {
    return (millis() - lastActivityTime > WIFI_IDLE_TIMEOUT);
}

void NetworkManager::handleWifiPowerSaving() {
    // If we've been idle and WiFi is on, turn it off to save power
    if (isWifiIdle() && WiFi.getMode() != WIFI_OFF) {
        disableWiFi();
    }
    
    // If we need to send data, turn WiFi on
    if (lamp->isDataReadyToSend() && WiFi.getMode() == WIFI_OFF) {
        enableWiFi();
        lastActivityTime = millis();
    }
}
#endif