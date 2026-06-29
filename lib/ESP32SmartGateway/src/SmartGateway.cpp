#include "SmartGateway.h"
#include "../../../src/config.h"

// 初始化单例指针
SmartGateway* SmartGateway::_instance = nullptr;

SmartGateway::SmartGateway() : _netManager(_espClient) {
    _instance = this;
    _lastSeqNum = -1;
    _bleConnected = false;
    _lastBlePacketTime = 0;
}

void SmartGateway::begin() {
    NetworkConfig netConfig;
    netConfig.wifiSsid = WIFI_SSID;
    netConfig.wifiPassword = WIFI_PASSWORD;
    netConfig.mqttBroker = MQTT_BROKER;
    netConfig.mqttPort = MQTT_PORT;
    netConfig.mqttUsername = MQTT_USERNAME;
    netConfig.mqttPassword = MQTT_PASSWORD;
    netConfig.clientIdPrefix = "water_brain_client";
    netConfig.wifiReconnectIntervalMs = 20000;
    netConfig.mqttReconnectIntervalMs = MQTT_RECONNECT_INTERVAL_MS;

    _netManager.begin(netConfig);

    // 订阅主题注册回调
    _netManager.onStateChange([](NetworkState state) {
        if (state == STATE_MQTT_CONNECTED) {
            if (_instance) {
                _instance->_netManager.subscribe(MQTT_DURATION_SUB);
                _instance->_netManager.subscribe(MQTT_PUMP_TIME_SUB);
            }
        }
    });

    _netManager.setMqttCallback(mqttCallback);

    setupBLE();
}

void SmartGateway::loop() {
    _netManager.loop();

    // 触发定时扫描
    if (_pBLEScan) {
        _pBLEScan->start(BLE_SCAN_DURATION_S, false);
    }

    // 检测 BLE 连接心跳超时（超过 15 秒未收到包则判定为断开）
    if (_bleConnected && (millis() - _lastBlePacketTime > 15000)) {
        _bleConnected = false;
        Serial.println("[BLE DEBUG] BLE connection timeout, set status to disconnected.");
    }
}

void SmartGateway::onSensorData(SensorDataCallback cb) {
    _sensorCb = cb;
}

void SmartGateway::onConfigDuration(ConfigDurationCallback cb) {
    _durationCb = cb;
}

void SmartGateway::onConfigPumpTime(ConfigPumpTimeCallback cb) {
    _pumpTimeCb = cb;
}

void SmartGateway::publishStatus(const char* jsonPayload) {
    _netManager.publish(MQTT_STATUS_TOPIC, jsonPayload);
}

void SmartGateway::publishLog(int channelId, const char* logMessage) {
    String topic = String(MQTT_LOG_PREFIX) + "/" + String(channelId);
    _netManager.publish(topic.c_str(), logMessage);
}

bool SmartGateway::isBleConnected() const {
    return _bleConnected;
}

void SmartGateway::mqttCallback(char* topic, byte* payload, unsigned int length) {
    if (_instance) {
        _instance->handleMqttMessage(topic, payload, length);
    }
}

void SmartGateway::handleMqttMessage(char* topic, byte* payload, unsigned int length) {
    String topicStr(topic);
    String valStr;
    for (unsigned int i = 0; i < length; i++) {
        valStr += (char)payload[i];
    }
    float value = valStr.toFloat();

    if (topicStr.startsWith("water_brain/config/duration/")) {
        int channelId = topicStr.substring(String("water_brain/config/duration/").length()).toInt();
        if (_durationCb) {
            _durationCb(channelId, value);
        }
    } else if (topicStr.startsWith("water_brain/config/pump_time/")) {
        int channelId = topicStr.substring(String("water_brain/config/pump_time/").length()).toInt();
        if (_pumpTimeCb) {
            _pumpTimeCb(channelId, value);
        }
    }
}

void SmartGateway::setupBLE() {
    BLEDevice::init("");
    _pBLEScan = BLEDevice::getScan();
    _pBLEScan->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks(), true);
    _pBLEScan->setActiveScan(true);
    _pBLEScan->setInterval(100);
    _pBLEScan->setWindow(99);
}

void SmartGateway::AdvertisedDeviceCallbacks::onResult(BLEAdvertisedDevice advertisedDevice) {
    if (advertisedDevice.getName() == TARGET_BLE_NAME) {
        if (advertisedDevice.haveManufacturerData()) {
            std::string data = advertisedDevice.getManufacturerData();
            if (data.length() == 11) {
                uint8_t cIdLsb = (uint8_t)data[0];
                uint8_t cIdMsb = (uint8_t)data[1];
                uint16_t cId = (cIdMsb << 8) | cIdLsb;
                if (cId == BLE_COMPANY_ID_VAL) {
                    uint8_t seqNum = (uint8_t)data[10];
                    if (seqNum != _instance->_lastSeqNum) {
                        _instance->_lastSeqNum = seqNum;
                        _instance->_bleConnected = true;
                        _instance->_lastBlePacketTime = millis();

                        Serial.printf("[GATEWAY BLE] 接收到唯一广播包, seqNum=%u\n", seqNum);

                        uint16_t ch0 = ((uint8_t)data[2] << 8) | (uint8_t)data[3];
                        uint16_t ch1 = ((uint8_t)data[4] << 8) | (uint8_t)data[5];
                        uint16_t ch2 = ((uint8_t)data[6] << 8) | (uint8_t)data[7];
                        uint16_t ch3 = ((uint8_t)data[8] << 8) | (uint8_t)data[9];

                        if (_instance->_sensorCb) {
                            _instance->_sensorCb(ch0, ch1, ch2, ch3);
                        }
                    }
                }
            }
        }
    }
}
