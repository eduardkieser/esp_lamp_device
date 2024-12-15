#include "NetworkManager.h"

NetworkManager::NetworkManager(LampController& lampCtrl) : lamp(&lampCtrl) {}

void NetworkManager::begin() {
    EEPROM.begin(512);
    
    if (loadConfig() && tryConnect(wifiConfig.ssid, wifiConfig.password)) {
        inAPMode = false;
        setupStation();
    } else {
        inAPMode = true;
        setupAP();
    }
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
    // Setup web server for normal operation
    server.on("/", [this]() { 
        server.send(200, "text/html", getControlHtml()); 
    });
    
    // Original web interface endpoints
    server.on("/status", [this]() { handleStatus(); });
    server.on("/control", [this]() { handleControl(); });
    
    // New API Endpoints
    server.on("/api/status", HTTP_GET, [this]() {
        float normalizedValue = (lamp->getCurrentValue() / LampConfig::MAX_ANALOG) * 100.0;
        String json = "{\"brightness\":" + String(normalizedValue) + 
                     ",\"deviceName\":\"" + deviceName + "\"}";
        server.send(200, "application/json", json);
    });

    server.on("/api/power", HTTP_POST, [this]() {
        if (server.hasArg("state")) {
            bool state = server.arg("state") == "on";
            // TODO: Add setPower method to LampController
            // lamp->setPower(state);
            server.send(200, "application/json", "{\"status\":\"success\"}");
        } else {
            server.send(400, "application/json", "{\"error\":\"missing parameters\"}");
        }
    });

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

void NetworkManager::handleStatus() {
    String json = "{\"brightness\":" + String(lamp->getCurrentValue()/LampConfig::MAX_ANALOG*100) + 
                  ",\"deviceName\":\"" + deviceName + "\"}";
    server.send(200, "application/json", json);
}

void NetworkManager::handleControl() {
    if (server.hasArg("brightness")) {
        float brightness = server.arg("brightness").toFloat();
        lamp->setRemoteValue(brightness);  // Using our existing setRemoteValue method
        server.send(200, "text/plain", "OK");
    } else {
        server.send(400, "text/plain", "Missing parameters");
    }
}

String NetworkManager::getControlHtml() {
    return R"html(
        <!DOCTYPE html>
        <html>
        <head>
            <title>SmartLamp Control</title>
            <meta name="viewport" content="width=device-width, initial-scale=1">
            <style>
                body { font-family: Arial; margin: 20px; }
                .slider-container { 
                    width: 100%; 
                    margin: 20px 0;
                    position: relative;
                }
                .slider { 
                    width: 100%; 
                    margin: 10px 0;
                }
                #value {
                    text-align: center;
                    font-size: 1.2em;
                    margin: 10px 0;
                }
                .updating {
                    opacity: 0.5;
                    pointer-events: none;
                }
                #status {
                    color: #666;
                    font-size: 0.8em;
                    text-align: center;
                }
            </style>
        </head>
        <body>
            <h1>SmartLamp Control</h1>
            <div class="slider-container">
                <input type="range" min="0" max="100" class="slider" id="brightness">
                <div id="value">0%</div>
                <div id="status"></div>
            </div>
            <script>
                const slider = document.getElementById('brightness');
                const valueDisplay = document.getElementById('value');
                const status = document.getElementById('status');
                let updatePending = false;
                let lastKnownValue = 0;

                function setUpdating(isUpdating) {
                    updatePending = isUpdating;
                    slider.parentElement.classList.toggle('updating', isUpdating);
                    status.textContent = isUpdating ? 'Updating...' : '';
                }

                function updateValue(value, skipSlider = false) {
                    if (!skipSlider) slider.value = value;
                    valueDisplay.textContent = Math.round(value) + '%';
                    lastKnownValue = value;
                }

                async function sendUpdate(value) {
                    if (updatePending) return;
                    setUpdating(true);
                    
                    try {
                        const response = await fetch('/control?brightness=' + value);
                        if (!response.ok) throw new Error('Update failed');
                        updateValue(value);
                    } catch (error) {
                        console.error('Update failed:', error);
                        updateValue(lastKnownValue);
                        status.textContent = 'Update failed!';
                    } finally {
                        setUpdating(false);
                    }
                }

                let updateTimeout;
                slider.oninput = function() {
                    updateValue(this.value, true);
                    clearTimeout(updateTimeout);
                    updateTimeout = setTimeout(() => sendUpdate(this.value), 100);
                }
                
                async function pollStatus() {
                    if (updatePending) return;
                    
                    try {
                        const response = await fetch('/status');
                        if (!response.ok) throw new Error('Status fetch failed');
                        const data = await response.json();
                        if (!updatePending) updateValue(data.brightness);
                    } catch (error) {
                        console.error('Status update failed:', error);
                    }
                }
                
                // Initial status fetch
                pollStatus();
                
                // Poll less frequently
                setInterval(pollStatus, 2000);
            </script>
        </body>
        </html>
    )html";
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

void NetworkManager::handleNotFound() {
    if (inAPMode) {
        // Capture portal - redirect all requests to setup page
        server.sendHeader("Location", "http://192.168.4.1/");
        server.send(302, "text/plain", "");
    } else {
        server.send(404, "text/plain", "Not found");
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