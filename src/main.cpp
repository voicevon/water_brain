#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <ArduinoJson.h>

#include "config.h"

// ============================================================================
//  HC595 移位寄存器控制机制
// ============================================================================

// 全局 16 位移位寄存器状态缓冲（Bit 0 ~ 15）
uint16_t g_hc595State = 0;

/**
 * @brief 将 16 位状态数据通过 SPI/GPIO 串行移入两片级联的 HC595 并锁存输出
 * @param state 16 位二进制组合状态
 */
void updateHC595(uint16_t state) {
    digitalWrite(HC595_ST_CP_PIN, LOW);
    
    // 分两个字节依次移出（高位芯片与低位芯片）
    shiftOut(HC595_DS_PIN, HC595_SH_CP_PIN, MSBFIRST, (state >> 8) & 0xFF); // Bit 15~8 (LED 阶段与部分指示灯)
    shiftOut(HC595_DS_PIN, HC595_SH_CP_PIN, MSBFIRST, state & 0xFF);        // Bit 7~0  (继电器及指示灯)
    
    digitalWrite(HC595_ST_CP_PIN, HIGH);
}

// ============================================================================
//  数据结构与类定义（骨架）
// ============================================================================

/**
 * @brief 自适应滤波与基准门限处理器（骨架）
 * 独立维护各通道的滑动平均滤波和慢速环境基准线跟踪。
 */
class DataProcessor {
public:
    DataProcessor() {
        // TODO: 初始化滑动平均与基准线滑动缓冲区
    }

    /**
     * @brief 输入最新原生值，进行滑动滤波并更新基准线
     */
    void pushRaw(uint16_t rawValue) {
        // TODO: 计算 50 窗口滑动平均 -> filteredValue
        // TODO: 计算 200 窗口慢速滑动平均 -> baselineValue
    }

    uint16_t getFiltered() const { return filteredValue; }
    uint16_t getBaseline() const { return baselineValue; }
    uint16_t getThreshold() const { return baselineValue - THRESHOLD_OFFSET_VAL; }

    /**
     * @brief 判断当前滤波值是否低于阈值（水流接触判断）
     */
    bool isDetected() const {
        return filteredValue <= getThreshold();
    }

private:
    uint16_t filteredValue = 0;
    uint16_t baselineValue = 0;
    // TODO: 声明历史滑动窗口环形缓冲区
};

/**
 * @brief 10步采样与抽空业务逻辑通道管理类（骨架）
 * 独立追踪每个采样通道的状态机、定时参数、水泵继电器状态等。
 */
class SamplingChannel {
public:
    int id;
    int relayBit; // 对应 HC595 位偏移
    int ledBit;   // 对应 HC595 位偏移
    uint32_t expectedDuration;
    uint32_t pumpWorkTime;
    float safetyFactor;
    
    // 状态机运行时参数
    int currentStage = 0;  // 0: 空闲, 1: 待稳, 2: 取头样, 3: 延时1, 4: 取中样, 5: 延时2, 6: 取尾样, 7: 等信号丢, 8: 排空, 9: 结束
    bool active = false;
    bool isDetected = false;
    bool lastPumpState = false;
    uint32_t onCount = 0;
    uint32_t offCount = 0;

    SamplingChannel(int chId, int rBit, int lBit, uint32_t defaultDur) 
        : id(chId), relayBit(rBit), ledBit(lBit), expectedDuration(defaultDur), 
          pumpWorkTime(DEFAULT_PUMP_WORK_SEC), safetyFactor(DEFAULT_SAFETY_FACTOR) {}

    /**
     * @brief 更新该通道的配置参数并重算休眠周期
     */
    void updateParameters(uint32_t newDur, uint32_t newPumpTime) {
        expectedDuration = newDur;
        pumpWorkTime = newPumpTime;
        calculateStages();
    }

    /**
     * @brief 核心状态机更新函数（10步采样算法骨架）
     * @param detected 传感器滤波后自适应判定结果
     * @return bool 是否需要运行水泵
     */
    bool update(bool detected) {
        isDetected = detected;
        // TODO: 实现 10 步采样状态机跳转：
        // Stage 0 -> (detected) -> Stage 1 (稳定期等待 180s) -> Stage 2 (取头样泵开 10s) 
        // -> Stage 3 (休眠 rest_time) -> Stage 4 (取中样泵开 10s) -> Stage 5 (休眠 rest_time)
        // -> Stage 6 (取尾样泵开 10s) -> Stage 7 (等信号丢失 180s) -> Stage 8 (排空抽空 10s)
        // -> Stage 9 (结束，等待信号断开 detected==false 后复位回到 Stage 0)

        bool needsPump = false;
        
        // 状态更新守卫
        if (needsPump != lastPumpState) {
            lastPumpState = needsPump;
            if (needsPump) onCount++;
            else offCount++;
            // TODO: 发送诊断日志
        }

        return needsPump;
    }

private:
    uint32_t stageTimes[5]; // 计算出的采样工作和休眠序列时间片
    
    void calculateStages() {
        // TODO: 实现 available_time = expectedDuration * safetyFactor - STABILIZATION_TIME_SEC
        // TODO: 实现 rest_time = (available_time - pumpWorkTime * 3) / 2 并填充时间序列
    }
};

// ============================================================================
//  全局对象与变量定义
// ============================================================================

WiFiClient espClient;
PubSubClient mqttClient(espClient);
BLEScan* pBLEScan = nullptr;

// 4通道传感器输入数据处理器
DataProcessor processors[4];

// 3通道本地继电器与指示灯物理状态机
SamplingChannel channels[3] = {
    SamplingChannel(0, HC595_RELAY_CH0, HC595_LED_CH0, DEFAULT_EXPECTED_DUR_CH0),
    SamplingChannel(1, HC595_RELAY_CH1, HC595_LED_CH1, DEFAULT_EXPECTED_DUR_CH1),
    SamplingChannel(2, HC595_RELAY_CH2, HC595_LED_CH2, DEFAULT_EXPECTED_DUR_CH2)
};

uint32_t lastWiFiCheck = 0;
uint32_t lastMQTTCheck = 0;
uint32_t lastStatusPublish = 0;
uint8_t lastSeqNum = -1;

// ============================================================================
//  WiFi 与 MQTT 连接机制（骨架）
// ============================================================================

void setupWiFi() {
    Serial.println("Initializing WiFi...");
    // TODO: 实现非阻塞 WiFi STA 初始化
}

void checkWiFi() {
    if (millis() - lastWiFiCheck >= 5000) {
        lastWiFiCheck = millis();
        // TODO: 非阻塞检查网络连接状态，断开时自动重连
    }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    // TODO: 实现接收 MQTT 配置消息，包括时长(duration)及泵启动时间(pump_time)，动态调用 updateParameters()
}

void checkMQTT() {
    if (millis() - lastMQTTCheck >= MQTT_RECONNECT_INTERVAL_MS) {
        lastMQTTCheck = millis();
        // TODO: 检查 MQTT 连接并执行非阻塞重连与订阅订阅主题
    }
}

void publishStatus() {
    if (millis() - lastStatusPublish >= 30000) {
        lastStatusPublish = millis();
        // TODO: 使用 StaticJsonDocument 组装包含 uptime, channels[i] 所有状态及传感器数值的 JSON 负载
        // TODO: 发布到 water_brain/system/status
    }
}

// ============================================================================
//  BLE 广告包扫描回调（骨架）
// ============================================================================

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
        // TODO: 过滤设备名称为 TARGET_BLE_NAME
        // TODO: 提取厂商自定义数据，判断 Company ID 是否为 0xFFFF 并且序列号发生变化
        // TODO: 解析大端序的 Ch0~Ch3 原始值，将其推入数据处理器中更新滤波
    }
};

void setupBLE() {
    BLEDevice::init("");
    pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks(), true);
    pBLEScan->setActiveScan(true);
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99);
}

// ============================================================================
//  Arduino 运行入口
// ============================================================================

void setup() {
    Serial.begin(115200);
    Serial.println("Starting water_brain...");

    // 配置 HC595 引脚输出模式
    pinMode(HC595_DS_PIN, OUTPUT);
    pinMode(HC595_SH_CP_PIN, OUTPUT);
    pinMode(HC595_ST_CP_PIN, OUTPUT);
    
    // 初始化状态，全关
    g_hc595State = 0;
    updateHC595(g_hc595State);

    // 初始化各个通信组件
    setupWiFi();
    mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
    mqttClient.setCallback(mqttCallback);
    
    setupBLE();
}

void loop() {
    // 维持网络连接
    checkWiFi();
    if (WiFi.status() == WL_CONNECTED) {
        checkMQTT();
        mqttClient.loop();
    }

    // 触发非阻塞扫描
    pBLEScan->start(BLE_SCAN_DURATION_S, false);

    // 映射对应传感数据到继电器状态机，并计算结果
    bool channelPumps[3] = {false, false, false};
    
    // 通道 0：对应 SENSOR_MAP_PUMP_0 的传感器输入
    channelPumps[0] = channels[0].update(processors[SENSOR_MAP_PUMP_0].isDetected());
    // 通道 1：对应 SENSOR_MAP_PUMP_1 的传感器输入
    channelPumps[1] = channels[1].update(processors[SENSOR_MAP_PUMP_1].isDetected());
    // 通道 2：对应 SENSOR_MAP_PUMP_2 的传感器输入
    channelPumps[2] = channels[2].update(processors[SENSOR_MAP_PUMP_2].isDetected());

    // 根据逻辑状态重新组装 16 位 HC595 输出状态
    g_hc595State = 0;
    
    // 1. 设置 3 路水泵继电器状态
    if (channelPumps[0]) g_hc595State |= (1 << HC595_RELAY_CH0);
    if (channelPumps[1]) g_hc595State |= (1 << HC595_RELAY_CH1);
    if (channelPumps[2]) g_hc595State |= (1 << HC595_RELAY_CH2);

    // 2. 设置 3 路传感器指示 LED 状态（例如检测到液体或 Active 状态）
    if (channels[0].isDetected) g_hc595State |= (1 << HC595_LED_CH0);
    if (channels[1].isDetected) g_hc595State |= (1 << HC595_LED_CH1);
    if (channels[2].isDetected) g_hc595State |= (1 << HC595_LED_CH2);

    // 3. 设置 10 步状态机阶段指示 LED
    // 示例：这里我们指示当前正在运行采样的通道的 Stage（如果没有通道处于 active，则显示通道0的Stage）
    int activeStage = 0;
    if (channels[0].active) activeStage = channels[0].currentStage;
    else if (channels[1].active) activeStage = channels[1].currentStage;
    else if (channels[2].active) activeStage = channels[2].currentStage;
    else activeStage = channels[0].currentStage; // 缺省空闲阶段

    if (activeStage >= 0 && activeStage < 10) {
        g_hc595State |= (1 << (HC595_LED_STAGE_BASE + activeStage));
    }

    // 写入级联移位寄存器并更新物理状态
    updateHC595(g_hc595State);

    // 定期上报系统状态 JSON
    publishStatus();

    delay(10);
}
