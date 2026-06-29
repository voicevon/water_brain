#ifndef SMART_GATEWAY_H
#define SMART_GATEWAY_H

#include <Arduino.h>
#include <ESP32WifiMqttManager.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

class SmartGateway {
public:
    // 事件回调函数指针定义
    typedef void (*SensorDataCallback)(uint16_t ch0, uint16_t ch1, uint16_t ch2, uint16_t ch3);
    typedef void (*ConfigDurationCallback)(int channelId, float durationMinutes);
    typedef void (*ConfigPumpTimeCallback)(int channelId, float pumpTimeSeconds);

    SmartGateway();

    // 初始化网关（WiFi, MQTT, BLE）
    void begin();
    
    // 心跳维护函数，需在主循环中非阻塞调用
    void loop();

    // 注册事件回调
    void onSensorData(SensorDataCallback cb);
    void onConfigDuration(ConfigDurationCallback cb);
    void onConfigPumpTime(ConfigPumpTimeCallback cb);

    // 上报数据与诊断日志接口
    void publishStatus(const char* jsonPayload);
    void publishLog(int channelId, const char* logMessage);
    
    // 查询蓝牙连接状态
    bool isBleConnected() const;

    // 查询网络状态
    NetworkState getNetworkState();

    // 更新扫描状态
    void setScanning(bool val) { _isScanning = val; }

private:
    static SmartGateway* _instance;
    
    SensorDataCallback _sensorCb = nullptr;
    ConfigDurationCallback _durationCb = nullptr;
    ConfigPumpTimeCallback _pumpTimeCb = nullptr;

    WiFiClient _espClient;
    ESP32WifiMqttManager _netManager;
    BLEScan* _pBLEScan = nullptr;

    uint8_t _lastSeqNum = -1;
    bool _bleConnected = false;
    bool _isScanning = false;
    uint32_t _lastBlePacketTime = 0;

    void setupBLE();
    
    static void mqttCallback(char* topic, byte* payload, unsigned int length);
    static void scanCompleteCB(BLEScanResults results);
    void handleMqttMessage(char* topic, byte* payload, unsigned int length);

    class AdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
        void onResult(BLEAdvertisedDevice advertisedDevice) override;
    };
};

#endif // SMART_GATEWAY_H
