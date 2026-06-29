# ESP32 主控接收器 (water_brain) 产品需求文档 (PRD)

## 1. 项目概述
本项目设计并开发一个基于 **ESP32** 微控制器的水控制系统主控接收节点（`water_brain`）。
该节点通过 BLE（低功耗蓝牙）扫描接收来自传感器节点（`water_sensor_touchpin`，设备名称为 `FengBLE`）广播的 4 通道电容式触摸传感器原始数值。主控在本地对这些数值进行自适应滤波和门限计算以判定是否检测到液体，并运行 **10 步采样控制算法**。
主控节点通过 **HC595 串口级联扩展芯片** 控制 **16** 路开关，其中包括 **3 路水泵继电器** 以及 **13 路状态 LED 指示灯**，以驱动水泵和显示系统运行状态。同时，系统支持 WiFi 接入及 MQTT 通信，可远程监控运行状态并动态调整配置参数。

---

## 2. 硬件架构与引脚配置

### 2.1 16路原生 GPIO 直接输出引脚分配表
本项目取消使用 HC595 移位寄存器芯片，改用 ESP32 的 16 个原生 GPIO 引脚作为数字输出直接控制水泵和指示灯：

| 功能定义 | GPIO 引脚 | 输出对象 | 安全说明与设计注意事项 |
| :--- | :---: | :--- | :--- |
| **Relay 0** | **GPIO 25** | 水泵继电器 0 | A 类引脚，无任何引导或复位启动限制 |
| **Relay 1** | **GPIO 26** | 水泵继电器 1 | A 类引脚，无任何引导或复位启动限制 |
| **Relay 2** | **GPIO 27** | 水泵继电器 2 | A 类引脚，无任何引导或复位启动限制 |
| **LED Ch0** | **GPIO 13** | 传感器通道 0 状态指示灯 | A 类引脚，无任何引导或复位启动限制 |
| **LED Ch1** | **GPIO 14** | 传感器通道 1 状态指示灯 | A 类引脚，无任何引导或复位启动限制 |
| **LED Ch2** | **GPIO 4**  | 传感器通道 2 状态指示灯 | A 类引脚，无任何引导或复位启动限制 |
| **LED Stage 0** | **GPIO 16** | 阶段 0 (空闲) 指示灯 | A 类引脚，无任何引导或复位启动限制 |
| **LED Stage 1** | **GPIO 17** | 阶段 1 (待稳) 指示灯 | A 类引脚，无任何引导或复位启动限制 |
| **LED Stage 2** | **GPIO 18** | 阶段 2 (取头样) 指示灯 | A 类引脚，无任何引导或复位启动限制 |
| **LED Stage 3** | **GPIO 19** | 阶段 3 (延时 1) 指示灯 | A 类引脚，无任何引导或复位启动限制 |
| **LED Stage 4** | **GPIO 21** | 阶段 4 (取中样) 指示灯 | A 类引脚，无任何引导或复位启动限制 |
| **LED Stage 5** | **GPIO 22** | 阶段 5 (延时 2) 指示灯 | A 类引脚，无任何引导或复位启动限制 |
| **LED Stage 6** | **GPIO 23** | 阶段 6 (取尾样) 指示灯 | A 类引脚，无任何引导或复位启动限制 |
| **LED Stage 7** | **GPIO 32** | 阶段 7 (等信号丢) 指示灯 | A 类引脚，无任何引导或复位启动限制 |
| **LED Stage 8** | **GPIO 33** | 阶段 8 (排空) 指示灯 | A 类引脚，无任何引导或复位启动限制 |
| **LED Stage 9** | **GPIO 5**  | 阶段 9 (结束) 指示灯 | B 类引脚，上电默认为 HIGH，建议接 active-high 负载 |

*注：以上引脚映射可在 `config.h` 中灵活调整。GPIO 12、GPIO 0、GPIO 2 等存在引导拉低/拉高限制的引脚在方案中已完全留空以确保 100% 稳定的烧录和引导。*

---

## 3. BLE 数据解析与通道对齐关系

传感器节点 `FengBLE` 广播 4 个通道的数据。主控只需要控制 3 个物理水泵。因此在 `config.h` 中必须定义一个对齐映射表，指定哪三个传感器通道映射到这三个水泵通道。

### 3.1 对齐宏定义
```cpp
// 传感器通道 (0~3) 到 继电器/泵通道 (0~2) 的映射
#define SENSOR_MAP_PUMP_0   3   // UI Ch4 (物理 Ch3) 映射到水泵继电器 0
#define SENSOR_MAP_PUMP_1   2   // UI Ch3 (物理 Ch2) 映射到水泵继电器 1
#define SENSOR_MAP_PUMP_2   1   // UI Ch2 (物理 Ch1) 映射到水泵继电器 2
```

### 3.2 厂商自定义数据载荷结构 (共 11 字节)
| 字节偏移 | 字段名称 | 数据类型 | 字节序 | 描述 |
| :---: | :--- | :---: | :---: | :--- |
| **0** | Company ID LSB | uint8_t | 小端序 | `0xFF` (与 MSB 组成 0xFFFF) |
| **1** | Company ID MSB | uint8_t | 小端序 | `0xFF` |
| **2~3** | Ch0 Raw Value | uint16_t | 大端序 | 传感器通道 0 原始触摸电容 |
| **4~5** | Ch1 Raw Value | uint16_t | 大端序 | 传感器通道 1 原始触摸电容 |
| **6~7** | Ch2 Raw Value | uint16_t | 大端序 | 传感器通道 2 原始触摸电容 |
| **8~9** | Ch3 Raw Value | uint16_t | 大端序 | 传感器通道 3 原始触摸电容 |
| **10** | Sequence Number | uint8_t | - | 递增序列号（用于包去重判定） |

---

## 4. 信号滤波与自适应基准判定算法

为应对硬件漂移、环境干扰及探针接触面变化，主控本地独立运行数字滤波器和自适应阈值追踪算法。

### 4.1 滤波与基准跟踪定义
针对每个通道的电容值：
1. **滤波后数值 (filteredValue)**：维护长度为 `50` 的滑动均值窗口：
   $$\text{filteredValue} = \frac{1}{50} \sum_{i=0}^{49} \text{rawHistory}[i]$$
2. **基准追踪线 (baseline)**：维护长度为 `200` 的慢速滑动均值窗口，对滤波后的值进行平滑：
   $$\text{baseline} = \frac{1}{200} \sum_{j=0}^{199} \text{filteredHistory}[j]$$
3. **自适应触发阈值 (threshold)**：
   $$\text{threshold} = \text{baseline} - \text{THRESHOLD\_OFFSET}$$
   默认配置 `THRESHOLD_OFFSET` 为 `5000`。

### 4.2 触发状态检测 (is_detected)
- **液体接触判断**：当滑动滤波值低于触发阈值时，判定为接触液体：
  $$\text{is\_detected} = (\text{filteredValue} \le \text{threshold})$$

---

## 5. 10 步采样状态机与继电器控制逻辑

每个通道独立运行一个状态机，对应 **10 个状态阶段 (Stage 0 ~ 9)**，完全适配 Android 客户端界面：

| 阶段索引 (Stage) | 界面显示名称 | 继电器状态 (泵) | 时间跨度及触发逻辑 |
| :---: | :--- | :---: | :--- |
| **0** | 空闲 (Idle) | 关 (OFF) | 初始状态，等待检测到液体 (`is_detected = true`) |
| **1** | 待稳 (Stabilizing) | 关 (OFF) | 信号稳期计时（180 秒）。中断则回退至 Stage 0 |
| **2** | 取头样 (Sample 1) | 开 (ON) | 第一次采样泵运行时间（默认 10 秒） |
| **3** | 延时 1 (Delay 1) | 关 (OFF) | 第一次休眠周期（`rest_time`） |
| **4** | 取中样 (Sample 2) | 开 (ON) | 第二次采样泵运行时间（默认 10 秒） |
| **5** | 延时 2 (Delay 2) | 关 (OFF) | 第二次休眠周期（`rest_time`） |
| **6** | 取尾样 (Sample 3) | 开 (ON) | 第三次采样泵运行时间（默认 10 秒） |
| **7** | 等信号丢 (Delay 3) | 关 (OFF) | 采样序列完成，等待信号丢失。若信号丢失达 180s 触发排空 |
| **8** | 排空 (Evacuation) | 开 (ON) | 信号消失后的第 4 次抽空泵运行时间（10 秒） |
| **9** | 结束 (Finished) | 关 (OFF) | 状态终结。待信号完全断开 (`is_detected = false`) 时复位回到 Stage 0 |

### 5.1 采样间隔时间动态计算
在转入 `Stage 2` 时，采样休眠的 `rest_time` 基于可用安全时间动态计算：
- **总可用时间 (available_time)**:
  $$\text{available\_time} = \text{expected\_duration} \times \text{safety\_factor} - \text{stabilization\_time}$$
  *其中 `expected_duration` 默认 1.5 ~ 2 小时，`safety_factor` 默认 0.75，`stabilization_time` 默认 180 秒。*
- **休息间隔时间 (rest_time)**:
  $$\text{rest\_time} = \frac{\text{available\_time} - \text{pump\_work\_time} \times 3}{2}$$
  如果计算得到的 `rest_time` 小于 0，则强制设为 0。

---

## 6. WiFi 与 MQTT 协议设计

### 6.1 网络连接
WiFi 连接和 MQTT 重连由单独的周期性非阻塞定时器维护，确保重连过程不阻塞 BLE 数据接收。

### 6.2 状态发布主题: `water_brain/system/status` (1/30Hz)
每隔 30 秒以 JSON 格式上报系统的整体状态（供 Android APP 监控状态条起伏绘制）：
```json
{
  "uptime_seconds": 12345,
  "ble_connected": true,
  "channels": [
    {
      "id": 1,
      "active": true,
      "stage": 2,
      "pump_on": true,
      "on_count": 1,
      "detected": true,
      "raw": 15400,
      "filtered": 15380,
      "baseline": 20450
    },
    ...
  ]
}
```

### 6.3 诊断日志发布主题: `water_brain/log/<channel_id>`
当通道状态切换、泵启停或接收到配置更改时，向相应的通道日志主题发送格式化的文本日志（兼容 3 行 x 7 汉字的终端显示）：
```
06月16日 15:47
通道1:
检测触发:待稳
```

### 6.5 配置订阅主题
- **采样预期时长**：`water_brain/config/duration/<channel_id>` (负载单位：分钟，范围 10.0 ~ 480.0)
- **水泵开启时间**：`water_brain/config/pump_time/<channel_id>` (负载单位：秒，范围 5.0 ~ 120.0)
