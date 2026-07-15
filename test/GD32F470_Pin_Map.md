# GD32F470 梁山派开发板引脚占用说明文档

本文档详细列出了当前 Keil 工程（CS1237 与 DAC 调试系统）中已被占用的 MCU 引脚及其功能用途，以防后续外设开发时发生引脚冲突。

---

## 引脚分配总表

| 序号 | 端口引脚 | 外设模块 | 配置模式 | I/O方向 | 默认电平/状态 | 功能描述 |
| :---: | :---: | :---: | :---: | :---: | :---: | :--- |
| 1 | **PA9** | **USART0** | 复用模式 (AF7) | 输出 (Push-Pull) | - | 调试串口发送端 (TX)，用于串口输出调试数据 |
| 2 | **PA10** | **USART0** | 复用模式 (AF7) | 输入 (Pull-Up) | - | 调试串口接收端 (RX)，接收控制指令 |
| 3 | **PA4** | **DAC0_OUT0** | 模拟模式 (Analog) | 输出 | - | 模拟电压输出通道 0 (DAC0) |
| 4 | **PA5** | **DAC0_OUT1** | 模拟模式 (Analog) | 输出 | - | 模拟电压输出通道 1 (DAC1) |
| 5 | **PC3** | **CS1237 ADC** | 普通输出 (GPIO Output) | 输出 (Push-Pull) | LOW | ADC 串行时钟线 (SCLK)，输出时钟脉冲（已由 PC1 移至 PC3） |
| 6 | **PC2** | **CS1237 ADC** | 双向输入输出 (GPIO) | 输入(上拉) / 输出 | - | ADC 串行数据线 (DOUT/DRDY)，收发数据与检测就绪 |
| 7 | **PD7** | **LED2** | 普通输出 (GPIO Output) | 输出 (Push-Pull) | - | 板载 LED2 控制脚，用于指示运行状态 |
| 8 | **PA1** | **YT8512C PHY** | 复用模式 (AF11) | 输入/输出 | - | RMII 50MHz 参考时钟线 (RMII_REF_CLK / TXC) |
| 9 | **PA2** | **YT8512C PHY** | 复用模式 (AF11) | 双向 (MDIO) | - | SMI 管理数据输入输出线 (RMII_MDIO) |
| 10 | **PC1** | **YT8512C PHY** | 复用模式 (AF11) | 输出 (Push-Pull) | - | SMI 管理时钟线 (RMII_MDC) |
| 11 | **PA7** | **YT8512C PHY** | 复用模式 (AF11) | 输入 | - | RMII 载波侦听/接收数据有效线 (RMII_CRS_DV / RXDV) |
| 12 | **PC4** | **YT8512C PHY** | 复用模式 (AF11) | 输入 | - | RMII 接收数据线 0 (RMII_RXD0 / RX0) |
| 13 | **PC5** | **YT8512C PHY** | 复用模式 (AF11) | 输入 | - | RMII 接收数据线 1 (RMII_RXD1 / RX1) |
| 14 | **PB11** | **YT8512C PHY** | 复用模式 (AF11) | 输出 (Push-Pull) | - | RMII 发送使能线 (RMII_TX_EN / TXEN) |
| 15 | **PB12** | **YT8512C PHY** | 复用模式 (AF11) | 输出 (Push-Pull) | - | RMII 发送数据线 0 (RMII_TXD0 / TX0) |
| 16 | **PB13** | **YT8512C PHY** | 复用模式 (AF11) | 输出 (Push-Pull) | - | RMII 发送数据线 1 (RMII_TXD1 / TX1) |
| 17 | **PD1** | **YT8512C PHY** | 普通输出 (GPIO Output) | 输出 (Push-Pull) | HIGH | PHY 芯片硬件复位引脚 (ETH_RST / RST) |

---

## 各外设模块引脚详情说明

### 1. 调试串口 (USART0)
* **引脚**：PA9 (TX), PA10 (RX)
* **配置源文件**：`usart.c` / `usart.h`
* **说明**：配置为 AF7 复用功能，波特率通常初始化为 `115200`，供 `printf` 重定向输出。

### 2. 数模转换 (DAC)
* **引脚**：PA4 (通道0), PA5 (通道1)
* **配置源文件**：`bsp_dac.c` / `bsp_dac.h`
* **说明**：配置为模拟（Analog）通道，当前在 `main.c` 中已被注释掉，若后续需要使用板载 DAC 输出测试电压，请直接取消 `main.c` 中 `bsp_dac_init()` 等函数的注释。

### 3. 高精度模数转换 (CS1237 ADC)
* **引脚**：PC3 (SCLK), PC2 (DOUT)
* **配置源文件**：`bsp_cs1237.c` / `bsp_cs1237.h`
* **说明**：
  * **PC3 (SCLK)**：时钟输出引脚（已由 PC1 改为 PC3，以释放 PC1 供以太网 MDC 使用）。由于 CS1237 芯片空闲时钟需保持为低电平且时钟高电平 $>100\mu\text{s}$ 会使芯片进入休眠，因此该引脚默认为 **低电平**。
  * **PC2 (DOUT)**：这是一个双向引脚，数据读取阶段作为**带上拉输入**；配置寄存器阶段会临时切换为**推挽输出**。

### 4. 状态指示灯 (LED2)
* **引脚**：PD7
* **配置源文件**：`main.c`
* **说明**：用于开发板板载的蓝色 LED 灯闪烁控制。

### 5. 百兆以太网 PHY (YT8512C)
* **引脚**：PA1 (REF_CLK), PA2 (MDIO), PA7 (CRS_DV), PB11 (TX_EN), PB12 (TXD0), PB13 (TXD1), PC1 (MDC), PC4 (RXD0), PC5 (RXD1), PD1 (RST)
* **配置源文件**：`bsp_enet.c` / `bsp_enet.h`
* **说明**：配置为 RMII 百兆以太网工作模式。其中 `PD1` 为 PHY 硬件复位脚，`PC1` 为 MDC 时钟线。

---

## 避坑与扩展指南

* **引脚复用注意**：
  在添加其他外设（如 SPI、I2C 或 PWM 脉宽调制）时，应避开已被占用的引脚。当前已占用引脚包括：
  * **调试串口**：PA9, PA10
  * **数模转换 (DAC)**：PA4, PA5
  * **CS1237 ADC**：PC3 (SCLK), PC2 (DOUT)
  * **状态指示灯**：PD7
  * **以太网 PHY**：PA1, PA2, PA7, PB11, PB12, PB13, PC1, PC4, PC5, PD1
* **VREF 与模拟地 (AGND) 注意**：
  在差分测量微小电压时，应保证 AIN- 与 AIN+ 外部线路远离高频数字信号线（如 PA9, PA10 串口线、PC3 时钟线以及以太网的 50MHz 时钟和数据高频线），并确保 GND 接触良好，以防引入高频噪声干扰 ADC 采样。
