#include <Arduino.h>
#include <ArduinoJson.h>
#include <SmartGateway.h>

#include "config.h"

// ============================================================================
//  GPIO 直接控制引脚定义
// ============================================================================

// 10步阶段指示灯引脚映射数组，方便管理与循环控制
const int STAGE_LED_PINS[10] = {
    LED_PIN_STAGE_0, LED_PIN_STAGE_1, LED_PIN_STAGE_2, LED_PIN_STAGE_3, LED_PIN_STAGE_4,
    LED_PIN_STAGE_5, LED_PIN_STAGE_6, LED_PIN_STAGE_7, LED_PIN_STAGE_8, LED_PIN_STAGE_9
};

// ============================================================================
//  数据结构与类定义
// ============================================================================

/**
 * @brief 自适应滤波与基准门限处理器
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
 * @brief 10步采样与抽空业务逻辑通道管理类
 * 独立追踪每个采样通道的状态机、定时参数、水泵继电器状态等。
 */
class SamplingChannel {
public:
    int id;
    int relayPin; // 物理 GPIO 引脚
    int ledPin;   // 物理 GPIO 引脚
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

    SamplingChannel(int chId, int rPin, int lPin, uint32_t defaultDur) 
        : id(chId), relayPin(rPin), ledPin(lPin), expectedDuration(defaultDur), 
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
            // 状态变更日志由 loop() 定时对比 currentStage 自动发出，此处不再耦合
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

// 4通道传感器输入数据处理器
DataProcessor processors[4];

// 3通道本地继电器与指示灯物理状态机
SamplingChannel channels[3] = {
    SamplingChannel(0, RELAY_PIN_CH0, LED_PIN_CH0, DEFAULT_EXPECTED_DUR_CH0),
    SamplingChannel(1, RELAY_PIN_CH1, LED_PIN_CH1, DEFAULT_EXPECTED_DUR_CH1),
    SamplingChannel(2, RELAY_PIN_CH2, LED_PIN_CH2, DEFAULT_EXPECTED_DUR_CH2)
};

// 智能网关实例
SmartGateway gateway(SensorSource::BLE);

uint32_t lastStatusPublish = 0;
int lastStages[3] = {-1, -1, -1};

// ============================================================================
//  网关事件回调处理
// ============================================================================

// 1. 接收到 BLE 传感器广播触摸原生值
void handleSensorData(uint16_t ch0, uint16_t ch1, uint16_t ch2, uint16_t ch3) {
    Serial.printf("[APP BLE] handleSensorData: ch0=%u, ch1=%u, ch2=%u, ch3=%u\n", ch0, ch1, ch2, ch3);
    processors[0].pushRaw(ch0);
    processors[1].pushRaw(ch1);
    processors[2].pushRaw(ch2);
    processors[3].pushRaw(ch3);
}

// 2. 接收到 MQTT 配置更改预期总时长
void handleConfigDuration(int channelId, float durationMinutes) {
    for (int i = 0; i < 3; i++) {
        if (channels[i].id == channelId) {
            uint32_t newDur = (uint32_t)(durationMinutes * 60.0f);
            channels[i].updateParameters(newDur, channels[i].pumpWorkTime);
            Serial.printf("[CONFIG] Channel %d duration updated to %d sec via MQTT.\n", channelId, newDur);
            break;
        }
    }
}

// 3. 接收到 MQTT 配置更改单次泵开启时长
void handleConfigPumpTime(int channelId, float pumpTimeSeconds) {
    for (int i = 0; i < 3; i++) {
        if (channels[i].id == channelId) {
            uint32_t newPump = (uint32_t)pumpTimeSeconds;
            channels[i].updateParameters(channels[i].expectedDuration, newPump);
            Serial.printf("[CONFIG] Channel %d pump work time updated to %d sec via MQTT.\n", channelId, newPump);
            break;
        }
    }
}

// 4. 定时发布状态数据 (JSON 格式)
void publishStatus() {
    if (millis() - lastStatusPublish >= 30000) {
        lastStatusPublish = millis();
        
        StaticJsonDocument<512> doc;
        doc["uptime_seconds"] = millis() / 1000;
        doc["ble_connected"] = gateway.isBleConnected();
        
        JsonArray channelsArr = doc.createNestedArray("channels");
        for (int i = 0; i < 3; i++) {
            JsonObject chObj = channelsArr.createNestedObject();
            chObj["id"] = channels[i].id;
            chObj["active"] = channels[i].active;
            chObj["stage"] = channels[i].currentStage;
            chObj["pump_on"] = channels[i].lastPumpState;
            chObj["on_count"] = channels[i].onCount;
            chObj["detected"] = channels[i].isDetected;
            
            // 获取对应的传感器物理通道索引并填充值
            int sensorCh = (i == 0) ? SENSOR_MAP_PUMP_0 : ((i == 1) ? SENSOR_MAP_PUMP_1 : SENSOR_MAP_PUMP_2);
            chObj["raw"] = processors[sensorCh].getFiltered(); // 默认填充滤波值或原生值
            chObj["filtered"] = processors[sensorCh].getFiltered();
            chObj["baseline"] = processors[sensorCh].getBaseline();
        }
        
        String jsonPayload;
        serializeJson(doc, jsonPayload);
        gateway.publishStatus(jsonPayload.c_str());
    }
}

// ============================================================================
//  Arduino 运行入口
// ============================================================================

void setup() {
    Serial.begin(115200);
    Serial.println("Starting water_brain...");

    // 初始化 3 路水泵继电器引脚与 3 路传感器状态指示灯引脚
    for (int i = 0; i < 3; i++) {
        pinMode(channels[i].relayPin, OUTPUT);
        digitalWrite(channels[i].relayPin, LOW); // 默认关闭水泵
        pinMode(channels[i].ledPin, OUTPUT);
        digitalWrite(channels[i].ledPin, LOW);   // 默认关闭通道指示灯
    }

    // 初始化 10 步状态机阶段指示灯引脚
    for (int i = 0; i < 10; i++) {
        pinMode(STAGE_LED_PINS[i], OUTPUT);
        digitalWrite(STAGE_LED_PINS[i], LOW);   // 默认关闭阶段指示灯
    }

    // 注册网关事件回调函数
    gateway.onSensorData(handleSensorData);
    gateway.onConfigDuration(handleConfigDuration);
    gateway.onConfigPumpTime(handleConfigPumpTime);

    // 启动网关网络通信
    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, LOW); // 默认关闭状态指示灯
    gateway.begin();
}

void loop() {
    // 维持网关心跳（自动处理 WiFi/MQTT 连接及非阻塞 BLE 扫描）
    gateway.loop();

    // 维持状态指示灯 LED 闪烁逻辑 (板载 LED, GPIO 2)
    unsigned long now = millis();
    NetworkState netState = gateway.getNetworkState();
    unsigned long period = 0;

    if (netState == STATE_WIFI_CONNECTED || netState == STATE_MQTT_CONNECTING) {
        period = 2000; // WiFi 已连接，MQTT 未连接：2 秒周期
    } else if (netState == STATE_MQTT_CONNECTED) {
        period = 1000; // WiFi 与 MQTT 均连接：1 秒周期
    } else {
        period = 0;    // 未连接 WiFi，常闭
    }

    if (period == 0) {
        digitalWrite(STATUS_LED_PIN, LOW);
    } else {
        if ((now % period) < (period / 2)) {
            digitalWrite(STATUS_LED_PIN, HIGH);
        } else {
            digitalWrite(STATUS_LED_PIN, LOW);
        }
    }

    // 映射对应传感数据到继电器状态机，并计算结果
    bool channelPumps[3] = {false, false, false};
    
    // 通道 0：对应 SENSOR_MAP_PUMP_0 的传感器输入
    channelPumps[0] = channels[0].update(processors[SENSOR_MAP_PUMP_0].isDetected());
    // 通道 1：对应 SENSOR_MAP_PUMP_1 的传感器输入
    channelPumps[1] = channels[1].update(processors[SENSOR_MAP_PUMP_1].isDetected());
    // 通道 2：对应 SENSOR_MAP_PUMP_2 的传感器输入
    channelPumps[2] = channels[2].update(processors[SENSOR_MAP_PUMP_2].isDetected());

    // 1. 直接控制 3 路水泵继电器引脚与 3 路传感器指示 LED
    for (int i = 0; i < 3; i++) {
        digitalWrite(channels[i].relayPin, channelPumps[i] ? HIGH : LOW);
        digitalWrite(channels[i].ledPin, channels[i].isDetected ? HIGH : LOW);
    }

    // 2. 确定当前正在运行采样的通道的 Stage
    int activeStage = 0;
    if (channels[0].active) activeStage = channels[0].currentStage;
    else if (channels[1].active) activeStage = channels[1].currentStage;
    else if (channels[2].active) activeStage = channels[2].currentStage;
    else activeStage = channels[0].currentStage; // 缺省空闲阶段

    // 3. 更新 10 步状态机指示灯，仅点亮当前 Stage 对应的指示灯，熄灭其他 9 个
    for (int stage = 0; stage < 10; stage++) {
        digitalWrite(STAGE_LED_PINS[stage], (stage == activeStage) ? HIGH : LOW);
    }

    // 4. 检测状态跳转并发送 MQTT 诊断日志
    for (int i = 0; i < 3; i++) {
        if (channels[i].currentStage != lastStages[i]) {
            lastStages[i] = channels[i].currentStage;
            char logMsg[64];
            snprintf(logMsg, sizeof(logMsg), "通道%d 状态跳转 -> Stage %d", i, channels[i].currentStage);
            gateway.publishLog(channels[i].id, logMsg);
        }
    }

    // 5. 定期上报系统状态 JSON (30 秒)
    publishStatus();

    delay(10);
}
