#include "QcControlClient.h"

#if QC_CONTROL_ENABLED

namespace {
const char* SERVICE_UUID = "7b6f1000-5f18-45d3-9b61-36d2d85b4c10";
const char* RX_UUID = "7b6f1001-5f18-45d3-9b61-36d2d85b4c10";
const char* TX_UUID = "7b6f1002-5f18-45d3-9b61-36d2d85b4c10";

class ProvisioningCallbacks : public BLECharacteristicCallbacks {
public:
    explicit ProvisioningCallbacks(QcControlClient* owner) : client(owner) {}

    void onWrite(BLECharacteristic* characteristic) override {
        client->handleProvisioningWrite(characteristic->getValue());
    }

private:
    QcControlClient* client;
};
}

QcControlClient::QcControlClient(LampController& lampCtrl) : lamp(&lampCtrl) {}

void QcControlClient::begin() {
    EEPROM.begin(EEPROM_SIZE);
    loadConfig();
    status = config.magic == CONFIG_MAGIC ? Status::WIFI_CONNECTING : Status::UNPROVISIONED;
    startBleProvisioning();
    if (config.magic != CONFIG_MAGIC) {
        setStatus(Status::UNPROVISIONED);
    }
    connectWifi();
}

void QcControlClient::update() {
    updateStatusLed();

    if (!config.magic) {
        return;
    }

    const wl_status_t wifiStatus = WiFi.status();
    if (wifiStatus != WL_CONNECTED) {
        if (wifiStatus == WL_NO_SSID_AVAIL ||
            wifiStatus == WL_CONNECT_FAILED ||
            millis() - lastWifiAttempt >= WIFI_CONNECT_TIMEOUT_MS) {
            setStatus(Status::WIFI_FAILED);
        }
        if (millis() - lastWifiAttempt >= WIFI_RETRY_MS) {
            connectWifi();
        }
        return;
    }

    if (!wsConnected) {
        if (millis() - lastWsAttempt >= WS_RETRY_MS) {
            connectWebSocket();
        }
        return;
    }

    processWebSocket();
    if (millis() - lastTelemetryAt >= TELEMETRY_INTERVAL_MS) {
        sendTelemetry();
    }
}

void QcControlClient::startBleProvisioning() {
    if (bleStarted) {
        return;
    }

    BLEDevice::init(("LampQC-" + deviceId().substring(12)).c_str());
    BLEServer* server = BLEDevice::createServer();
    BLEService* service = server->createService(SERVICE_UUID);

    BLECharacteristic* rx = service->createCharacteristic(
        RX_UUID,
        BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR
    );
    rx->setCallbacks(new ProvisioningCallbacks(this));

    txCharacteristic = service->createCharacteristic(
        TX_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
    );
    txCharacteristic->addDescriptor(new BLE2902());
    txCharacteristic->setValue("unprovisioned");

    service->start();
    BLEAdvertising* advertising = BLEDevice::getAdvertising();
    advertising->addServiceUUID(SERVICE_UUID);
    advertising->setScanResponse(true);
    advertising->start();
    bleStarted = true;
    publishProvisioningStatus(statusName(status));
}

void QcControlClient::handleProvisioningWrite(const std::string& value) {
    const String json(value.c_str());
    if (!parseProvisioningJson(json)) {
        setStatus(Status::INVALID_CONFIG);
        return;
    }

    saveConfig();
    setStatus(Status::WIFI_CONNECTING);
    closeWebSocket();
    WiFi.disconnect(true);
    connectWifi();
}

void QcControlClient::publishProvisioningStatus(const char* status) {
    if (txCharacteristic == nullptr) {
        return;
    }

    const String payload = "{\"status\":\"" + String(status) +
        "\",\"description\":\"" + String(statusDescription(this->status)) +
        "\",\"lampId\":\"" + lampId() +
        "\",\"deviceId\":\"" + deviceId() + "\"}";
    txCharacteristic->setValue(payload.c_str());
    txCharacteristic->notify();
}

void QcControlClient::setStatus(Status nextStatus) {
    if (status == nextStatus) {
        updateStatusLed();
        return;
    }

    status = nextStatus;
    statusLedOn = false;
    lastLedUpdate = 0;
    updateStatusLed();
    publishProvisioningStatus(statusName(status));
}

void QcControlClient::updateStatusLed() {
    const unsigned long interval = statusBlinkInterval(status);
    if (interval == 0) {
        writeStatusLed(true);
        return;
    }

    const unsigned long now = millis();
    if (lastLedUpdate == 0 || now - lastLedUpdate >= interval) {
        statusLedOn = !statusLedOn;
        lastLedUpdate = now;
        writeStatusLed(statusLedOn);
    }
}

void QcControlClient::writeStatusLed(bool on) {
    if (!on) {
        lamp->setQcStatusLed(false, false, false);
        return;
    }

    switch (status) {
        case Status::UNPROVISIONED:
            lamp->setQcStatusLed(false, false, true);
            break;
        case Status::WIFI_CONNECTING:
            lamp->setQcStatusLed(false, true, true);
            break;
        case Status::WIFI_FAILED:
            lamp->setQcStatusLed(true, false, false);
            break;
        case Status::WS_CONNECTING:
            lamp->setQcStatusLed(true, false, true);
            break;
        case Status::WS_FAILED:
            lamp->setQcStatusLed(true, false, true);
            break;
        case Status::CONNECTED:
            lamp->setQcStatusLed(false, true, false);
            break;
        case Status::INVALID_CONFIG:
            lamp->setQcStatusLed(true, false, false);
            break;
    }
}

const char* QcControlClient::statusName(Status value) const {
    switch (value) {
        case Status::UNPROVISIONED:
            return "unprovisioned";
        case Status::WIFI_CONNECTING:
            return "wifi_connecting";
        case Status::WIFI_FAILED:
            return "wifi_failed";
        case Status::WS_CONNECTING:
            return "ws_connecting";
        case Status::WS_FAILED:
            return "ws_failed";
        case Status::CONNECTED:
            return "connected";
        case Status::INVALID_CONFIG:
            return "invalid_config";
    }
    return "unknown";
}

const char* QcControlClient::statusDescription(Status value) const {
    switch (value) {
        case Status::UNPROVISIONED:
            return "Waiting for BLE provisioning";
        case Status::WIFI_CONNECTING:
            return "Provisioned; connecting to Wi-Fi";
        case Status::WIFI_FAILED:
            return "Wi-Fi connection failed; check SSID and password";
        case Status::WS_CONNECTING:
            return "Wi-Fi connected; connecting to controller";
        case Status::WS_FAILED:
            return "Controller WebSocket connection failed";
        case Status::CONNECTED:
            return "Connected to controller";
        case Status::INVALID_CONFIG:
            return "Provisioning payload was invalid";
    }
    return "Unknown status";
}

unsigned long QcControlClient::statusBlinkInterval(Status value) const {
    switch (value) {
        case Status::CONNECTED:
            return 0;
        case Status::INVALID_CONFIG:
            return 150;
        case Status::WIFI_CONNECTING:
            return 250;
        case Status::UNPROVISIONED:
            return 500;
        case Status::WS_CONNECTING:
            return 500;
        case Status::WIFI_FAILED:
            return 1000;
        case Status::WS_FAILED:
            return 1000;
    }
    return 500;
}

bool QcControlClient::loadConfig() {
    EEPROM.get(CONFIG_OFFSET, config);
    if (config.magic != CONFIG_MAGIC) {
        memset(&config, 0, sizeof(config));
        return false;
    }
    return true;
}

void QcControlClient::saveConfig() {
    config.magic = CONFIG_MAGIC;
    EEPROM.put(CONFIG_OFFSET, config);
    EEPROM.commit();
}

bool QcControlClient::parseProvisioningJson(const String& json) {
    const String ssid = readStringValue(json, "ssid");
    const String password = readStringValue(json, "password");
    const String controller = readStringValue(json, "controller");
    const String id = readStringValue(json, "lampId");

    if (ssid.length() == 0 || controller.length() == 0) {
        return false;
    }

    memset(&config, 0, sizeof(config));
    ssid.toCharArray(config.ssid, sizeof(config.ssid));
    password.toCharArray(config.password, sizeof(config.password));
    if (id.length() > 0) {
        id.toCharArray(config.lampId, sizeof(config.lampId));
    }
    if (!parseControllerUrl(controller)) {
        return false;
    }
    config.magic = CONFIG_MAGIC;
    return true;
}

void QcControlClient::connectWifi() {
    if (config.magic != CONFIG_MAGIC) {
        setStatus(Status::UNPROVISIONED);
        return;
    }

    lastWifiAttempt = millis();
    setStatus(Status::WIFI_CONNECTING);
    WiFi.mode(WIFI_STA);
    WiFi.begin(config.ssid, config.password);
}

void QcControlClient::connectWebSocket() {
    lastWsAttempt = millis();
    setStatus(Status::WS_CONNECTING);
    if (!client.connect(config.controllerHost, config.controllerPort)) {
        wsConnected = false;
        setStatus(Status::WS_FAILED);
        return;
    }

    client.printf(
        "GET %s HTTP/1.1\r\n"
        "Host: %s:%u\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: x3JJHMbDL1EzLkh9GBhXDw==\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n",
        config.controllerPath,
        config.controllerHost,
        config.controllerPort
    );

    const unsigned long startedAt = millis();
    String response;
    while (millis() - startedAt < 3000) {
        while (client.available()) {
            response += char(client.read());
            if (response.indexOf("\r\n\r\n") >= 0) {
                wsConnected = response.indexOf("101") >= 0;
                if (wsConnected) {
                    setStatus(Status::CONNECTED);
                    sendHello();
                } else {
                    client.stop();
                    setStatus(Status::WS_FAILED);
                }
                return;
            }
        }
        delay(5);
    }

    client.stop();
    wsConnected = false;
    setStatus(Status::WS_FAILED);
}

void QcControlClient::closeWebSocket() {
    if (client.connected()) {
        client.stop();
    }
    wsConnected = false;
    if (config.magic == CONFIG_MAGIC &&
        (status == Status::CONNECTED || status == Status::WS_CONNECTING)) {
        setStatus(Status::WS_FAILED);
    }
}

void QcControlClient::processWebSocket() {
    if (!client.connected()) {
        closeWebSocket();
        return;
    }

    while (client.available() >= 2) {
        const uint8_t first = client.read();
        uint8_t lengthByte = client.read();
        const uint8_t opcode = first & 0x0F;
        uint64_t length = lengthByte & 0x7F;

        if (length == 126) {
            while (client.available() < 2) delay(1);
            length = (uint16_t(client.read()) << 8) | client.read();
        } else if (length == 127) {
            closeWebSocket();
            return;
        }

        String payload;
        payload.reserve(length);
        const unsigned long startedAt = millis();
        while (payload.length() < length && millis() - startedAt < 1000) {
            if (client.available()) {
                payload += char(client.read());
            }
        }

        if (opcode == 0x1) {
            handleTextFrame(payload);
        } else if (opcode == 0x8) {
            closeWebSocket();
            return;
        } else if (opcode == 0x9) {
            sendTextFrame("{\"type\":\"pong\"}");
        }
    }
}

void QcControlClient::sendHello() {
    const String payload = "{\"type\":\"hello\",\"lampId\":\"" + lampId() +
        "\",\"deviceId\":\"" + deviceId() +
        "\",\"firmware\":\"" + String(LampConfig::FIRMWARE_LABEL) +
        "\",\"batteryVoltage\":" + String(lamp->getBatteryVoltage(), 2) + "}";
    sendTextFrame(payload);
    lastTelemetryAt = millis();
}

void QcControlClient::sendTelemetry() {
    const String payload = "{\"type\":\"telemetry\",\"lampId\":\"" + lampId() +
        "\",\"batteryVoltage\":" + String(lamp->getBatteryVoltage(), 2) +
        ",\"brightness\":" +
        String((lamp->getCurrentValue() / LampConfig::MAX_ANALOG) * 100.0f, 1) +
        "}";
    sendTextFrame(payload);
    lastTelemetryAt = millis();
}

void QcControlClient::sendTextFrame(const String& payload) {
    if (!client.connected()) {
        closeWebSocket();
        return;
    }

    const size_t length = payload.length();
    client.write(uint8_t(0x81));
    if (length < 126) {
        client.write(uint8_t(0x80 | length));
    } else {
        client.write(uint8_t(0x80 | 126));
        client.write(uint8_t((length >> 8) & 0xFF));
        client.write(uint8_t(length & 0xFF));
    }

    const uint8_t mask[4] = {0x12, 0x34, 0x56, 0x78};
    client.write(mask, 4);
    for (size_t index = 0; index < length; index++) {
        client.write(uint8_t(payload[index]) ^ mask[index % 4]);
    }
}

void QcControlClient::handleTextFrame(const String& payload) {
    if (payload.indexOf("\"type\":\"set\"") >= 0 ||
        payload.indexOf("\"type\": \"set\"") >= 0) {
        applySetCommand(payload);
    }
}

void QcControlClient::applySetCommand(const String& payload) {
    const float level = readFloatValue(payload, "level", -1.0f);
    if (level < 0.0f) {
        return;
    }
    commandSequence++;
    lamp->setRemoteValue(level);
    sendTelemetry();
}

bool QcControlClient::parseControllerUrl(const String& url) {
    String value = url;
    value.replace("ws://", "");
    const int slash = value.indexOf('/');
    const String authority = slash >= 0 ? value.substring(0, slash) : value;
    const String path = slash >= 0 ? value.substring(slash) : "/lamp-ws";
    const int colon = authority.indexOf(':');
    const String host = colon >= 0 ? authority.substring(0, colon) : authority;
    const int port = colon >= 0 ? authority.substring(colon + 1).toInt() : 8080;
    if (host.length() == 0 || port <= 0) {
        return false;
    }

    host.toCharArray(config.controllerHost, sizeof(config.controllerHost));
    path.toCharArray(config.controllerPath, sizeof(config.controllerPath));
    config.controllerPort = uint16_t(port);
    return true;
}

String QcControlClient::lampId() const {
    if (strlen(config.lampId) > 0) {
        return String(config.lampId);
    }
    return "LAMP-" + deviceId().substring(12);
}

String QcControlClient::deviceId() const {
    char buffer[17];
    snprintf(buffer, sizeof(buffer), "%016llX", lamp->getSerialNumber());
    return String(buffer);
}

String QcControlClient::readStringValue(const String& json, const char* key) const {
    const String pattern = "\"" + String(key) + "\"";
    int index = json.indexOf(pattern);
    if (index < 0) return "";
    index = json.indexOf(':', index);
    if (index < 0) return "";
    index = json.indexOf('"', index);
    if (index < 0) return "";
    const int end = json.indexOf('"', index + 1);
    if (end < 0) return "";
    return json.substring(index + 1, end);
}

float QcControlClient::readFloatValue(const String& json, const char* key, float fallback) const {
    const String pattern = "\"" + String(key) + "\"";
    int index = json.indexOf(pattern);
    if (index < 0) return fallback;
    index = json.indexOf(':', index);
    if (index < 0) return fallback;
    int start = index + 1;
    while (start < json.length() && isspace(json[start])) start++;
    int end = start;
    while (end < json.length() &&
           (isdigit(json[end]) || json[end] == '.' || json[end] == '-')) {
        end++;
    }
    if (end <= start) return fallback;
    return json.substring(start, end).toFloat();
}

#endif
