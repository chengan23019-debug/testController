# JSY-MK-1031 交直流电能计量模块驱动开发与接线指南

本文档记录了在 **GD32F470ZGT6（立创·梁山派）** 测功机控制系统中集成 **JSY-MK-1031** 交直流通用电能计量模块的引脚分配、通信协议、软件架构与 API 说明。

---

## 1. 硬件引脚分配与物理连线

### 1.1 模块电气特性
* **通信接口**：TTL 电平（兼容 3.3V / 5V）
* **供电电压**：DC 3.3V ~ 5V（推荐直接接梁山派 3.3V 供电引脚，电平完美契合）
* **通信协议**：Modbus-RTU 从机协议，默认波特率 **9600 bps**，数据位 **8**，无校验 **N**，停止位 **1**（8-N-1）
* **默认从机地址**：`0x01`

### 1.2 MCU 引脚映射 (USART1)
在避开板载 32MB SDRAM、LAN8720 以太网、CS1237/CS1238 ADC 及调试串口 PA9/PA10 的前提下，选定 **USART1 (Port D)**：

| 信号名称 | GD32F470 引脚 | 引脚复用 (AF) | JSY-MK-1031 引脚 | 连线说明 |
| :--- | :---: | :---: | :---: | :--- |
| **TX** | **PD5** | `GPIO_AF_7` | **RXD** (或对调接TXD) | MCU 发送端 |
| **RX** | **PD6** | `GPIO_AF_7` | **TXD** (或对调接RXD) | MCU 接收端 |
| **VCC** | **5V** | - | **VCC** | 模块供电（必须接 5V） |
| **GND** | **GND** | - | **GND** | 共地参考 |
| **PF** | *(悬空)* | - | **PF** | 脉冲校验输出端，日常采集无需接线 |

> **硬件设计提示**：
> 1. **PC10/PC11 与板载 TF 卡冲突**：立创·梁山派板载 MicroSD (TF卡) 插槽默认使用 `PC10 (SDIO_D2)` 和 `PC11 (SDIO_D3)`。因此优先使用 **USART1 (PD5/PD6)** 可以保留板载 TF 卡功能。
> 2. **线序对应**：由于部分模块丝印习惯不同，若首次连接超时，只需将 MCU 侧的两根通信线对调即可。

### 1.3 梁山派板载外设资源冲突核验报告 (PD5 & PD6)

针对**立创·梁山派 (GD32F470ZGT6)** 官方硬件原理图的逐一排查核验结论：

| 板载硬件外设模块 | 使用引脚列表 | 是否占用 PD5 / PD6 | 冲突状态分析 |
| :--- | :--- | :---: | :--- |
| **板载 SDRAM (32MB)** | `PD0, PD1, PD8, PD9, PD10, PD14, PD15` 及 Port C/E/F/G | ❌ 否 | **零冲突**。SDRAM 控制线完美避开 `PD2~PD7`。 |
| **板载 TF 卡 (SDIO)** | `PC8 (D0), PC9 (D1), PC10 (D2), PC11 (D3), PC12 (CLK), PD2 (CMD)` | ❌ 否 | **零冲突**。使用 PD5/PD6 可完整保留 SDIO TF 卡扩展能力。 |
| **板载 用户 LED** | `PD7 (LED1-蓝), PE3 (LED2), PE4 (LED3), PE5 (LED4)` | ❌ 否 | **零冲突**。LED1 位于相邻的 `PD7`，布线靠近且完全独立。 |
| **板载 物理按键** | `PA0 (K1/Wakeup), PC13 (K2), PE0 (K3), PE1 (K4)` | ❌ 否 | **零冲突**。所有按键均不在 Port D5/D6。 |
| **板载 以太网 (LAN8720)** | `PA1, PA2, PA7, PB11, PB12, PB13, PC1, PC4, PC5` | ❌ 否 | **零冲突**。以太网 RMII 引脚完全无交集。 |
| **板载 调试串口 (CH340)** | `PA9 (USART0_TX), PA10 (USART0_RX)` | ❌ 否 | **零冲突**。控制台日志走 USART0，独立于测电模块 USART1。 |
| **板载 SPI Flash** | `PB2 (CLK), PC0 (CS), PF6~PF9 (IO/ADC共用区)` | ❌ 否 | **零冲突**。 |

> **核心结论**：
> **PD5 和 PD6 在梁山派开发板上未挂载任何板载元器件（无电阻、无电容、无芯片并联），是直通 2.54mm 双排引出排母的纯净 GPIO**。因此选用 **USART1 (PD5/PD6)** 是目前梁山派上接入 JSY-MK-1031 电力计量模块的**最优解**。

---

## 2. 软件设计与工程集成

### 2.1 源码文件结构
* [bsp_jsy_mk1031.h](file:///d:/code/testController/testController/test_v2/User/bsp_jsy_mk1031.h)：引脚宏、Modbus 寄存器定义、全电量数据结构体 `jsy_mk1031_data_t` 及 API 声明。
* [bsp_jsy_mk1031.c](file:///d:/code/testController/testController/test_v2/User/bsp_jsy_mk1031.c)：USART1 硬件初始化、中断接收、CRC-16 计算、Modbus 编解码与 FreeRTOS 后台采集任务。
* [gd32f4xx_it.c](file:///d:/code/testController/testController/test_v2/User/gd32f4xx_it.c)：实现 `USART1_IRQHandler()` 串口中断回调。
* [main.c](file:///d:/code/testController/testController/test_v2/User/main.c)：系统启动初始化、创建 10Hz 采集任务，并在 50Hz 核心同步推流中原子读取快照。

### 2.2 核心采集逻辑与 FreeRTOS 调度
由于 9600 bps 下一帧 35 字节的 Modbus 查询与应答耗时约 $40 \sim 50\text{ ms}$，系统采用了**异步分层机制**：
1. **后台独立任务 (`app_task_jsy_mk1031`)**：以 100ms (10Hz) 周期运行，负责下发查询帧 `01 03 00 48 00 0F 85 D8` 并通过空闲中断触发帧接收，解析后存入内部缓存。
2. **核心推流任务 (`app_task_telemetry_50hz`)**：每 20ms 执行一次，通过调用 `jsy_mk1031_get_data(&jsy_data)` 瞬间获取最新电力快照，**零阻塞、零延迟**；若模块离线则自动平滑回退至板载 HLW8112。

---

## 3. 测量参数与物理量转换关系

| 测量量 | 寄存器地址 | 寄存器数 | 模块上报原始格式 | 换算系数 | 物理单位 |
| :--- | :---: | :---: | :--- | :--- | :--- |
| **电压** | `0x0048` | 1 | 16位无符号整数 | $\div 100$ | $\text{V}$ |
| **电流** | `0x0049~0x004A` | 2 | 32位无符号整数 | $\div 10000$ | $\text{A}$ |
| **有功功率** | `0x004B~0x004C` | 2 | 32位无符号整数 | $\div 10000 (\text{kW}) \times 1000$ | $\text{W}$ |
| **有功总电能** | `0x004D~0x004E` | 2 | 32位无符号整数 | $\div 100$ | $\text{kWh}$ |
| **功率因数** | `0x004F` | 1 | 16位有符号整数 | $\div 1000$ | $-1.000 \sim +1.000$ |
| **频率** | `0x0050` | 1 | 16位无符号整数 | $\div 100$ | $\text{Hz}$ |
| **视在功率** | `0x0057~0x0058` | 2 | 32位无符号整数 | $\div 10$ | $\text{VA}$ |
| **无功功率** | `0x0059~0x005A` | 2 | 32位无符号整数 | $\div 10$ | $\text{var}$ |
| **相位角** | `0x005B` | 1 | 16位有符号整数 | $\div 100$ | 度 (°) |

---

## 4. 驱动对外 API 快速参考

```c
/* 1. 初始化串口 USART1 (PD5/PD6, 9600 8N1) */
void bsp_jsy_mk1031_init(uint32_t baudrate);

/* 2. 获取最新电能快照 (线程安全，支持高频 50Hz 随取随用) */
void jsy_mk1031_get_data(jsy_mk1031_data_t *out_data);

/* 3. 电能累计清零 (写 0x004D 为 0) */
bool jsy_mk1031_reset_energy(uint8_t slave_addr, uint32_t timeout_ms);

/* 4. 创建 FreeRTOS 后台异步采样任务 */
void jsy_mk1031_create_task(uint32_t priority);
```
