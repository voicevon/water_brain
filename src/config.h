#ifndef CONFIG_H
#define CONFIG_H

// ============================================================
//  全局配置文件 — 所有硬件参数、协议常量、网络配置的唯一来源
// ============================================================

// -------- 直接控制 GPIO 引脚配置 --------
// 3路继电器/水泵引脚
#define RELAY_PIN_CH0   25
#define RELAY_PIN_CH1   26
#define RELAY_PIN_CH2   27

// 3路传感器通道状态指示灯引脚
#define LED_PIN_CH0     13
#define LED_PIN_CH1     14
#define LED_PIN_CH2     4

// 10步状态机阶段指示灯引脚 (对应 Stage 0 ~ 9)
#define LED_PIN_STAGE_0  16
#define LED_PIN_STAGE_1  17
#define LED_PIN_STAGE_2  18
#define LED_PIN_STAGE_3  19
#define LED_PIN_STAGE_4  21
#define LED_PIN_STAGE_5  22
#define LED_PIN_STAGE_6  23
#define LED_PIN_STAGE_7  32
#define LED_PIN_STAGE_8  33
#define LED_PIN_STAGE_9  5

// 系统运行状态指示灯 (板载 LED)
#define STATUS_LED_PIN   2

// -------- 传感器通道对齐映射表 --------
// 传感器节点 FengBLE 广播 4 个通道 (Ch0~Ch3)
// 这里映射哪三个传感器通道到我们的 3 路水泵控制 (0~2)
#define SENSOR_MAP_PUMP_0   3   // UI Ch4 (物理 Ch3) -> 水泵继电器 0
#define SENSOR_MAP_PUMP_1   2   // UI Ch3 (物理 Ch2) -> 水泵继电器 1
#define SENSOR_MAP_PUMP_2   1   // UI Ch2 (物理 Ch1) -> 水泵继电器 2

// -------- 滤波与基准门限配置 (自适应电容触发算法) --------
#define MA_WINDOW_SIZE       50     // 滑动平滑窗口大小
#define BASELINE_WINDOW_SIZE 200    // 基准追踪慢窗口大小
#define THRESHOLD_OFFSET_VAL 5000   // 触发阈值偏差 (baseline - offset)

// -------- 10阶段采样与抽空时间常量配置 --------
#define STABILIZATION_TIME_SEC   180    // 稳定触发等待时间（秒，默认3分钟，对应 Stage 1）
#define SIGNAL_LOST_TIME_SEC     180    // 信号丢失等待触发抽空时间（秒，默认3分钟，对应 Stage 7）
#define DEFAULT_PUMP_WORK_SEC    10     // 默认单次泵工作时长（秒）
#define DEFAULT_SAFETY_FACTOR    0.75f  // 默认采样安全系数

// 默认各通道预期总时长（秒）：通道0(1.5h), 通道1(2.0h), 通道2(40m)
#define DEFAULT_EXPECTED_DUR_CH0 5400
#define DEFAULT_EXPECTED_DUR_CH1 7200
#define DEFAULT_EXPECTED_DUR_CH2 2400

// -------- BLE 扫描参数 --------
#define TARGET_BLE_NAME      "FengBLE"
#define BLE_COMPANY_ID_VAL   0xFFFF
#define BLE_SCAN_DURATION_S  5      // 单次扫描时长（秒）

// -------- WiFi 网络配置 --------
#define WIFI_SSID       "Perfect"
#define WIFI_PASSWORD   "12344321"

// -------- MQTT Broker 配置 --------
#define MQTT_BROKER     "voicevon.vicp.io"
#define MQTT_PORT       1883
#define MQTT_USERNAME   "von"
#define MQTT_PASSWORD   "" // 视环境决定

// -------- MQTT 主题定义 --------
#define MQTT_STATUS_TOPIC    "water_brain/system/status"
#define MQTT_INFO_TOPIC      "water_brain/system/info"
#define MQTT_LOG_PREFIX      "water_brain/log"

// 订阅远程配置的主题前缀
#define MQTT_DURATION_SUB    "water_brain/config/duration/+"
#define MQTT_PUMP_TIME_SUB   "water_brain/config/pump_time/+"

// 站点名称定义（作为过滤依据）
#define STATION_NAME         "dongzhan"
// 订阅传感器数据的主题（以 water_Android 为准）
#define MQTT_SENSOR_DATA_SUB "water/sensor/status"

// MQTT 非阻塞重连最小间隔（毫秒）
#define MQTT_RECONNECT_INTERVAL_MS  5000UL

// -------- 系统运行诊断配置 --------
#define CORE_DEBUG_LEVEL 1

#endif // CONFIG_H
