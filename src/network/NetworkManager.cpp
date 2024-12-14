#include "NetworkManager.h"

NetworkManager::NetworkManager() {}

void NetworkManager::begin() {
    WiFi.mode(WIFI_AP);
    WiFi.softAP("SmartLamp-Setup");
    
    // Setup DNS server (for captive portal)
    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
    
    setupWebServer();
}

void NetworkManager::setupWebServer() {
    server.on("/", [this]() { handleRoot(); });
    server.on("/save", [this]() { handleSave(); });
    server.onNotFound([this]() { handleNotFound(); });
    server.begin();
}

void NetworkManager::handleRoot() {
    server.send(200, "text/html", getHtmlContent());
}

void NetworkManager::handleSave() {
    if (server.hasArg("ssid") && server.hasArg("pass")) {
        // TODO: Save WiFi credentials
        server.send(200, "text/html", 
            "Settings saved!<br>"
            "SSID: " + server.arg("ssid") + "<br>"
            "Device will restart in 3 seconds...");
        delay(3000);
        ESP.restart();
    } else {
        server.send(400, "text/plain", "Missing parameters");
    }
}

void NetworkManager::handleNotFound() {
    // Redirect all unknown requests to the configuration page
    server.sendHeader("Location", "http://" + WiFi.softAPIP().toString(), true);
    server.send(302, "text/plain", "");
}

String NetworkManager::getHtmlContent() {
    String html = R"html(
        <!DOCTYPE html>
        <html>
        <head>
            <title>SmartLamp Setup</title>
            <meta name="viewport" content="width=device-width, initial-scale=1">
            <style>
                body { font-family: Arial; margin: 20px; }
                input { display: block; margin: 10px 0; padding: 5px; width: 95%; }
                button { padding: 10px; width: 100%; background: #0078D7; color: white; border: none; }
            </style>
        </head>
        <body>
            <h1>SmartLamp Setup</h1>
            <form action="/save" method="get">
                <input type="text" name="ssid" placeholder="WiFi Name (SSID)">
                <input type="password" name="pass" placeholder="WiFi Password">
                <button type="submit">Save Configuration</button>
            </form>
        </body>
        </html>
    )html";
    return html;
}

void NetworkManager::update() {
    dnsServer.processNextRequest();
    server.handleClient();
} 