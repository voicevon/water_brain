#include "SmartGateway.h"
#include "../../../src/config.h"
#include <ArduinoJson.h>

// 初始化单例指针
SmartGateway* SmartGateway::_instance = nullptr;

// BLE 异步扫描结束后的回调
void SmartGateway::scanCompleteCB(BLEScanResults results) {
    BLEDevice::getScan()->clearResults(); // 清理扫描结果，释放内存
    if (_instance) {
        _instance->_isScanning = false;
    }
}

SmartGateway::SmartGateway(SensorSource source) : _netManager(_espClient), _sensorSource(source) {
    _instance = this;
    _lastSeqNum = -1;
    _bleConnected = false;
    _isScanning = false;
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
                if (_instance->_sensorSource == SensorSource::MQTT) {
                    _instance->_netManager.subscribe(MQTT_SENSOR_DATA_SUB);
                }
            }
        }
    });

    _netManager.setMqttCallback(mqttCallback);

    if (_sensorSource == SensorSource::BLE) {
        setupBLE();
    }
}

void SmartGateway::loop() {
    _netManager.loop();

    if (_sensorSource == SensorSource::BLE) {
        // 触发定时扫描 (非阻塞异步方式)
        if (_pBLEScan && !_isScanning) {
            _isScanning = true;
            _pBLEScan->start(BLE_SCAN_DURATION_S, scanCompleteCB, false);
        }

        // 检测 BLE 连接心跳超时（超过 15 秒未收到包则判定为断开）
        if (_bleConnected && (millis() - _lastBlePacketTime > 15000)) {
            _bleConnected = false;
            Serial.println("[BLE DEBUG] BLE connection timeout, set status to disconnected.");
        }
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

NetworkState SmartGateway::getNetworkState() {
    return _netManager.getState();
}

void SmartGateway::mqttCallback(char* topic, byte* payload, unsigned int length) {
    if (_instance) {
        _instance->handleMqttMessage(topic, payload, length);
    }
}

void SmartGateway::handleMqttMessage(char* topic, byte* payload, unsigned int length) {
    String topicStr(topic);

    if (_sensorSource == SensorSource::MQTT && topicStr == MQTT_SENSOR_DATA_SUB) {
        StaticJsonDocument<256> doc;
        DeserializationError error = deserializeJson(doc, payload, length);
        if (!error) {
            const char* name = doc["name"] | "";
            if (strcmp(name, STATION_NAME) == 0) {
                uint16_t ch0 = doc["ch1"] | 0;
                uint16_t ch1 = doc["ch2"] | 0;
                uint16_t ch2 = doc["ch3"] | 0;
                uint16_t ch3 = doc["ch4"] | 0;
                if (_sensorCb) {
                    _sensorCb(ch0, ch1, ch2, ch3);
                }
            }
        } else {
            Serial.print(F("[GATEWAY MQTT] Failed to parse sensor JSON: "));
            Serial.println(error.f_str());
        }
        return;
    }

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
