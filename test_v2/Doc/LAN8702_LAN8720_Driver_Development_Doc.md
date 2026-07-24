# GD32F470 LAN8702 / LAN8720 以太网驱动开发文档

## 1. 项目与硬件概述

本工程基于 **GD32F470** 主控芯片，成功实现了 **LAN8702 / LAN8720A** 10/100M 以太网 PHY 模块（RMII 接口）的底层驱动开发与集成。

- **主控芯片**：GD32F470ZGT6 (ARM Cortex-M4, 主频 240MHz)
- **网络 PHY 芯片**：LAN8702 / LAN8720A (SMSC/Microchip)
- **通信接口**：RMII (Reduced Media Independent Interface) + SMI (MDC/MDIO 管理接口)
- **开发环境**：Keil uVision5 (ARMCC V5.06 编译器)
- **工程文件**：`Project/GD32f450.uvprojx`

---

## 2. 硬件引脚映射 (RMII Interface)

LAN8702 / LAN8720A 模块与 GD32F470 开发板（如梁山派）的硬件管脚连接如下表所示：

| LAN8702 模块管脚 | GD32F470 引脚 | RMII 信号名称 | 功能说明 |
| :--- | :--- | :--- | :--- |
| **VCC** | **3.3V** | VCC | 模块 3.3V 电源供电 |
| **GND** | **GND** | GND | 电源地 |
| **RETCLK / nINT** | **PA1** | `ETH_RMII_REF_CLK` | 50MHz 参考时钟输入 (模块板载 50M 晶振) |
| **MDIO** | **PA2** | `ETH_MDIO` | SMI 串行管理数据总线 |
| **MDC** | **PC1** | `ETH_MDC` | SMI 串行管理时钟总线 |
| **CRS / CRS_DV** | **PA7** | `ETH_RMII_CRS_DV` | 载波监听 / 接收数据有效 |
| **RX0 / RXD0** | **PC4** | `ETH_RMII_RXD0` | RMII 接收数据线 0 |
| **RX1 / RXD1** | **PC5** | `ETH_RMII_RXD1` | RMII 接收数据线 1 |
| **TX-EN** | **PB11** | `ETH_RMII_TX_EN` | RMII 发送使能 |
| **TX0 / TXD0** | **PB12** | `ETH_RMII_TXD0` | RMII 发送数据线 0 |
| **TX1 / TXD1** | **PB13** | `ETH_RMII_TXD1` | RMII 发送数据线 1 |

---

## 3. 代码架构与文件分布

软件驱动代码分布于工程 `User/` 目录下，模块化划分明确：

```text
Project/
├── User/
│   ├── bsp_lan8720.h      # LAN8720/LAN8702 寄存器定义与驱动 API 头文件
│   ├── bsp_lan8720.c      # 网卡硬件初始化、SMI 总线扫描、物理链路判定与 DMA 帧收发
│   ├── bsp_lan8702.h      # LAN8702 API 兼容封装头文件
│   ├── bsp_lan8702.c      # LAN8702 兼容层源文件
│   ├── main.c             # 系统入口与以太网链路轮询测试
│   └── gd32f4xx_libopt.h  # 固件库头文件包含配置 (自动包含 gd32f4xx_enet.h)
└── Firmware/
    └── GD32F4xx_standard_peripheral/Source/
        ├── gd32f4xx_enet.c    # GD32 官方 ENET 外设固件库
        └── gd32f4xx_syscfg.c  # GD32 官方 SYSCFG 外设固件库 (用于 RMII 模式选择)
```

---

## 4. 关键技术难点与解决记录

### 4.1 RMII 模式配置宏与调用时机
GD32F470 与 STM32F4 的外设配置宏存在差异：
- 必须开启 `RCU_SYSCFG` 时钟并调用 `syscfg_enet_phy_interface_config(SYSCFG_ENET_PHY_RMII)` 选择 RMII 模式。
- GPIO 引脚须配置为复用推挽模式，并指定复用功能为 `GPIO_AF_11`。

### 4.2 保持 PHY 芯片硬件自动协商 (`BCR = 0x1000`)
- **踩坑点**：上电时若未插网线，自动协商超时后如果将 `media_mode` 误设为 `ENET_100M_FULLDUPLEX`，调用 `enet_init()` 会将 `0x2100` 写入 PHY 的 `BCR` 寄存器，从而**关闭芯片硬件自动协商功能**（BCR Bit 12=0）。此后插拔网线时，LAN8702 无法与网线对端交换机握手，导致 `BSR` 寄存器锁死在 `0x7809`（Link Down）。
- **解决方法**：在 `bsp_lan8720_init()` 中始终维持 `enet_init(ENET_AUTO_NEGOTIATION, ...)`，并在初始化后显式将 `PHY_BCR` 写为 `PHY_BCR_AUTONEG_ENABLE` (`0x1000`)，保持网卡的动态协商响应。

### 4.3 链路状态精准判定
- **标准**：严格依据 IEEE 802.3 规范，读取 PHY 基本状态寄存器 `PHY_BSR` (0x01) 的 **Bit 2 (`0x0004`)**。由于 Bit 2 具备锁存特性（Latch-low），每次读取需连续读取两次以获取最新实时物理链路状态。
- `BSR = 0x7809` (Bit 2=0) -> **LINK DOWN** (未连接网线)
- `BSR = 0x780D` (Bit 2=1) -> **LINK UP** (网线已连接并握手成功)

---

## 5. 主要 API 说明与使用示例

### 5.1 驱动初始化 `bsp_lan8702_init`
```c
int bsp_lan8702_init(void);
```
- **功能**：使能 ENET/GPIO 时钟，配置 RMII 引脚，扫描 SMI 总线自动识别 PHY 地址 (0x01)，执行 PHY 软复位并使能自动协商，初始化 ENET MAC 地址及 DMA 描述符链。
- **返回值**：`LAN8702_OK` (0) 表示成功，`LAN8702_ERROR` (-1) 表示失败。

### 5.2 链路状态查询 `bsp_lan8702_get_link_status`
```c
uint8_t bsp_lan8702_get_link_status(void);
```
- **返回值**：`1` 表示物理链路通畅 (Link UP)，`0` 表示断开 (Link DOWN)。

### 5.3 数据包发送 `bsp_lan8702_send_packet`
```c
int bsp_lan8702_send_packet(uint8_t *p_buf, uint16_t len);
```
- **功能**：通过 GD32 MAC DMA 发送原始以太网帧。

### 5.4 数据包接收 `bsp_lan8702_receive_packet`
```c
uint16_t bsp_lan8702_receive_packet(uint8_t *p_buf, uint16_t max_len);
```
- **功能**：从 MAC DMA 接收描述符轮询数据帧，返回接收到的字节长度。

### 5.5 寄存器调试诊断 `bsp_lan8702_print_phy_regs`
```c
void bsp_lan8702_print_phy_regs(void);
```
- **功能**：串口输出当前 PHY 的 BCR、BSR、ANAR、ANLPAR 及 SCSR 寄存器十六进制原始值。

---

## 6. 应用层例程 (`main.c`)

```c
#include "gd32f4xx.h"
#include "systick.h"
#include "usart.h"
#include "bsp_lan8702.h"
#include <stdio.h>

int main(void)
{
    uint8_t link_status = 0;

    systick_config();
    usart_debug_init(115200);
    printf("\r\n=== GD32F470 LAN8702 Ethernet Driver Test ===\r\n");

    /* 初始化 LAN8702 网络模块 */
    if (LAN8702_OK == bsp_lan8702_init()) {
        printf("LAN8702 Ethernet Driver Initialized Successfully!\r\n");
    } else {
        printf("LAN8702 Driver Initialization Failed!\r\n");
    }

    while (1) {
        delay_1ms(1000);
        /* 查询网络链路状态 */
        link_status = bsp_lan8702_get_link_status();
        printf("Ethernet Link Status: %s\r\n", 
               link_status ? "LINK UP [Connected]" : "LINK DOWN [Disconnected]");
        
        /* 打印 PHY 底层寄存器 Hex 值 */
        bsp_lan8702_print_phy_regs();
    }
}
```
