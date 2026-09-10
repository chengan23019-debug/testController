# GD32F470 串口扩展与引脚规划开发指南

本文档记录了在 **GD32F470ZGT6**（立创·梁山派开发板）测控系统已有外设资源基础之上，对**扩展串口（USART/UART）及 RS485 通信引脚**的规划方案、冲突排查分析、推荐硬件接线与底层驱动代码实现。

---

## 1. 系统已用引脚与冲突排查

当前测控系统工程已集成的外设及引脚占用情况如下：

| 外设模块 | 已占用引脚 | 说明 |
| :--- | :--- | :--- |
| **LAN8720A / 8702 (以太网 PHY)** | `PA1`, `PA2`, `PA7`, `PC1`, `PC4`, `PC5`, `PB11`, `PB12`, `PB13` | RMII + SMI 接口 (AF11) |
| **CS1238 (24位双通道 ADC)** | `PD8` (SCLK), `PD9` (DOUT) | 软件 2-Wire 总线 |
| **CS1237 (24位单通道 ADC)** | `PC6` (SCLK), `PC7` (DOUT) | 软件 2-Wire 总线 |
| **HLW8112 (交流电计量芯片)** | `PB5` (CS), `PB6` (SCLK), `PB7` (SDI), `PB8` (SDO), `PB9` (EN) | 软件 SPI 接口 |
| **DAC 模拟量输出** | `PA4` (DAC0_OUT0), `PA5` (DAC0_OUT1) | 加载控制电压 (0~3.3V) |
| **调试串口 USART0** | `PA9` (TX), `PA10` (RX) | 调试控制台输出 (115200 8N1) |
| **状态指示灯 LED2** | `PD7` | FreeRTOS 心跳指示 (500ms) |

---

## 2. 串口扩展与引脚规划方案

在完全避开上述已用引脚的前提下，推荐以下两路扩展串口规划：

### 2.1 推荐串口分配表（首选方案）

| 序号 | 串口外设 | 功能定义 | TX 引脚 | RX 引脚 | 方向控制 (DE/RE) | 复用功能 (AF) | 特点说明 |
| :---: | :--- | :--- | :---: | :---: | :---: | :---: | :--- |
| **串口 0** | **USART0** | 调试控制台 (CLI/Log) | **PA9** | **PA10** | - | **AF7** | 已占用，板载默认调试口 |
| **串口 1** | **USART1** | **RS485 通道 1 (Modbus RTU 从机)** | **PD5** | **PD6** | **PD4** (可选) | **AF7** | **首选**：位于 Port D 排针，连续且与 LED(PD7) 相邻 |
| **串口 2** | **USART2** | **RS485 通道 2 (电机驱动器/备用传感器)** | **PD8** | **PD9** | **PD10** (可选) | **AF7** | **首选**：与 USART1 位于同一 Port D 排针区域，布线整洁 |

> **备选方案**：若 USART2 需布线至 Port C 区域，可选用 **`PC10 (TX)` / `PC11 (RX)`**，同样配置为 **AF7**，引脚完全空闲且无冲突。

---

### 2.2 全部可用备选串口对照表

| 串口外设 | TX 引脚 | RX 引脚 | 复用 AF 映射 | 冲突状态 | 适用场景 |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **USART1** | **PD5** | **PD6** | `GPIO_AF_7` | **无冲突 (推荐)** | RS485 通道 1 / Modbus RTU |
| **USART2** | **PD8** 或 **PC10** | **PD9** 或 **PC11** | `GPIO_AF_7` | **无冲突 (推荐)** | RS485 通道 2 / 测功机传感器通信 |
| **USART5** | **PC6** | **PC7** | `GPIO_AF_8` | **无冲突** | 紧邻以太网 PC5 引脚，扩展通信备用 |
| **UART4**  | **PC12** | **PD2** | `GPIO_AF_8` | **无冲突** | 远距离独立引脚备用 |
| **UART6**  | **PE8** | **PE7** | `GPIO_AF_8` | **无冲突** | Port E 区域设备扩展 |

---

## 3. RS485 硬件接口连接示意

若扩展串口用于 RS485 工业通信（如对接 SP3485、MAX485 或带光耦隔离的 485 模块），连接拓扑如下：

```text
GD32F470 MCU                           RS485 收发器 (如 SP3485)
┌──────────────┐                        ┌──────────────┐
│  PD5 (TX)    ├───────────────────────►│  DI (Driver)  │
│  PD6 (RX)    │◄───────────────────────┤  RO (Receiver)│          A (+)  ────────
│  PD4 (DIR/EN)├──────────┬────────────►│  DE (Tx En)  ├─────────►        双绞屏蔽线
│              │          └────────────►│ /RE (Rx En)  ├─────────► B (-)  ────────
│  3.3V / GND  ├───────────────────────►│  VCC / GND   │
└──────────────┘                        └──────────────┘
```

* **发送状态**：MCU 将 `PD4` 置高电平（`DE=1`, `/RE=1`），使能 RS485 发送驱动器。
* **接收状态**：MCU 将 `PD4` 置低电平（`DE=0`, `/RE=0`），使能 RS485 接收器。

---

## 4. GD32 底层驱动初始化代码参考

### 4.1 USART1 (PD5/PD6) 初始化函数

```c
#include "gd32f4xx.h"

#define RS485_1_DIR_RCU     RCU_GPIOD
#define RS485_1_DIR_PORT    GPIOD
#define RS485_1_DIR_PIN     GPIO_PIN_4

#define RS485_1_TX_EN()     gpio_bit_set(RS485_1_DIR_PORT, RS485_1_DIR_PIN)
#define RS485_1_RX_EN()     gpio_bit_reset(RS485_1_DIR_PORT, RS485_1_DIR_PIN)

void bsp_usart1_rs485_init(uint32_t baudrate)
{
    /* 1. 使能 GPIOD、USART1 及 SYSCFG 时钟 */
    rcu_periph_clock_enable(RCU_GPIOD);
    rcu_periph_clock_enable(RCU_USART1);

    /* 2. 配置 PD5(TX) 与 PD6(RX) 为 AF7 复用推挽 */
    gpio_af_set(GPIOD, GPIO_AF_7, GPIO_PIN_5 | GPIO_PIN_6);
    gpio_mode_set(GPIOD, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO_PIN_5 | GPIO_PIN_6);
    gpio_output_options_set(GPIOD, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_5 | GPIO_PIN_6);

    /* 3. 配置 PD4 为 RS485 发送/接收方向控制引脚 (DIR) */
    gpio_mode_set(RS485_1_DIR_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, RS485_1_DIR_PIN);
    gpio_output_options_set(RS485_1_DIR_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, RS485_1_DIR_PIN);
    RS485_1_RX_EN(); /* 初始置为接收模式 */

    /* 4. 复位并配置 USART1 串口参数 (8-N-1) */
    usart_deinit(USART1);
    usart_baudrate_set(USART1, baudrate);
    usart_word_length_set(USART1, USART_WL_8BIT);
    usart_stop_bit_set(USART1, USART_STB_1BIT);
    usart_parity_config(USART1, USART_PM_NONE);
    usart_hardware_flow_coherence_config(USART1, USART_HCM_NONE);

    /* 5. 使能收发与串口模块 */
    usart_transmit_config(USART1, USART_TRANSMIT_ENABLE);
    usart_receive_config(USART1, USART_RECEIVE_ENABLE);

    /* 6. 使能接收中断 (如需使用中断/FreeRTOS 队列接收) */
    nvic_irq_enable(USART1_IRQn, 6, 0);
    usart_interrupt_enable(USART1, USART_INT_RBNE);

    usart_enable(USART1);
}
```

### 4.2 USART2 (PD8/PD9) 初始化函数

```c
#define RS485_2_DIR_RCU     RCU_GPIOD
#define RS485_2_DIR_PORT    GPIOD
#define RS485_2_DIR_PIN     GPIO_PIN_10

#define RS485_2_TX_EN()     gpio_bit_set(RS485_2_DIR_PORT, RS485_2_DIR_PIN)
#define RS485_2_RX_EN()     gpio_bit_reset(RS485_2_DIR_PORT, RS485_2_DIR_PIN)

void bsp_usart2_rs485_init(uint32_t baudrate)
{
    /* 1. 使能 GPIOD 及 USART2 时钟 */
    rcu_periph_clock_enable(RCU_GPIOD);
    rcu_periph_clock_enable(RCU_USART2);

    /* 2. 配置 PD8(TX) 与 PD9(RX) 为 AF7 复用 */
    gpio_af_set(GPIOD, GPIO_AF_7, GPIO_PIN_8 | GPIO_PIN_9);
    gpio_mode_set(GPIOD, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO_PIN_8 | GPIO_PIN_9);
    gpio_output_options_set(GPIOD, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_8 | GPIO_PIN_9);

    /* 3. 配置 PD10 为 RS485 方向控制引脚 */
    gpio_mode_set(RS485_2_DIR_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, RS485_2_DIR_PIN);
    gpio_output_options_set(RS485_2_DIR_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, RS485_2_DIR_PIN);
    RS485_2_RX_EN();

    /* 4. 配置 USART2 参数 (8-N-1) */
    usart_deinit(USART2);
    usart_baudrate_set(USART2, baudrate);
    usart_word_length_set(USART2, USART_WL_8BIT);
    usart_stop_bit_set(USART2, USART_STB_1BIT);
    usart_parity_config(USART2, USART_PM_NONE);
    usart_hardware_flow_coherence_config(USART2, USART_HCM_NONE);

    usart_transmit_config(USART2, USART_TRANSMIT_ENABLE);
    usart_receive_config(USART2, USART_RECEIVE_ENABLE);

    nvic_irq_enable(USART2_IRQn, 6, 0);
    usart_interrupt_enable(USART2, USART_INT_RBNE);

    usart_enable(USART2);
}
```

---

## 5. 总结与集成建议

1. **统一端口集中在 Port D**：将两路扩展串口集中在 `PD4~PD6`（USART1 + DIR）和 `PD8~PD10`（USART2 + DIR），排针引脚连续，与开发板现有外设无任何冲突，硬件打板或杜邦线接线极其整齐。
2. **协议集成**：
   - **USART1 (PD5/PD6)**：可直接对接工程中的 `app_protocol_modbus.c`，作为标准 Modbus RTU 从机通道；
   - **USART2 (PD8/PD9)**：可作为 Modbus 主机或私有串口协议，用于轮询第三方转速传感器、电机控制器或环境温湿度变送器。
