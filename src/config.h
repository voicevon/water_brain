#ifndef CONFIG_H
#define CONFIG_H

// ============================================================
//  全局配置文件 — 所有硬件参数、协议常量、网络配置的唯一来源
// ============================================================

// -------- 级联 HC595 移位寄存器总线引脚配置 --------
#define HC595_DS_PIN     14  // 串行数据输入 (SER)
#define HC595_SH_CP_PIN  12  // 移位寄存器时钟 (SRCLK)
#define HC595_ST_CP_PIN  13  // 存储寄存器/锁存时钟 (RCLK)

// 继电器/泵和指示灯在 HC595 16-bit 寄存器中的位偏移定义
#define HC595_RELAY_CH0   0  // 继电器 0
#define HC595_RELAY_CH1   1  // 继电器 1
#define HC595_RELAY_CH2   2  // 继电器 2

#define HC595_LED_CH0     3  // 通道 0 状态指示 LED
#define HC595_LED_CH1     4  // 通道 1 状态指示 LED
#define HC595_LED_CH2     5  // 通道 2 状态指示 LED

#define HC595_LED_STAGE_BASE 6 // 10 个阶段指示灯起始位 (Bit 6~15)

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

// MQTT 非阻塞重连最小间隔（毫秒）
#define MQTT_RECONNECT_INTERVAL_MS  5000UL

// -------- 系统运行诊断配置 --------
#define CORE_DEBUG_LEVEL 3

#endif // CONFIG_H
