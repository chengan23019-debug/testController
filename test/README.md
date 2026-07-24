# GD32F4xx 控制器嵌入式项目说明

本项目基于 **GD32F4xx**（GD32F450/GD32F470 系列 Cortex-M4）微控制器开发，结合了 **FreeRTOS** 实时操作系统和 **LwIP** 轻量级网络协议栈，实现了包含高精度 ADC 采样、电能/电力计量、DAC 控制及以太网通信等功能的嵌入式系统。

---

## 📁 目录结构及内容说明

整个项目按照模块化结构组织，主要文件夹及内容如下：

| 目录名称 | 模块说明 | 主要功能与内容 |
| :--- | :--- | :--- |
| 📂 [`App`](App/) | **应用业务逻辑层** | 封装具体的业务应用算法与任务逻辑。包含 CS1237 高精度 ADC 数据的采样处理与自动量程切换逻辑（`app_cs1237`），以及 HLW8112 电能计量芯片的数据处理应用（`app_hlw8112`）。 |
| 📂 [`Hardware`](Hardware/) | **硬件外设驱动层 (BSP)** | 板级硬件驱动程序。封装芯片级引脚通信与控制逻辑，包含：<br>• `cs1237/`: 24位高精度 ADC 底层通信驱动<br>• `hlw8112/`: 电力计量芯片通信驱动<br>• `dac/`: 模拟量输出 DAC 底层驱动<br>• `YT8512C/`: 裕太微以太网 PHY 芯片驱动 |
| 📂 [`User`](User/) | **用户主程序及系统配置** | 项目入口与各组件全局配置。包含系统主函数（`main.c`）、系统滴答时钟与调试串口（`systick.c` / `usart.c`）、中断服务程序（`gd32f4xx_it.c`），以及 FreeRTOS（`FreeRTOSConfig.h`）和 LwIP（`lwipopts.h`）的配置文件。 |
| 📂 [`Firmware`](Firmware/) | **GD32 官方固件库 SDK** | GigaDevice 官方提供的 SDK 库文件。包含：<br>• `CMSIS/`: ARM Cortex-M4 核心及系统初始化代码<br>• `GD32F4xx_standard_peripheral/`: GD32F4xx 外设标准库驱动<br>• `GD32F4xx_usb_library/`: USB 驱动库 |
| 📂 [`FreeRTOS`](FreeRTOS/) | **FreeRTOS 操作系统内核** | FreeRTOS V11.3.0 实时操作系统内核源码、头文件以及特定芯片架构的移植代码（`portable/`）。 |
| 📂 [`LwIP`](LwIP/) | **LwIP 网络协议栈** | LwIP 轻量级 TCP/IP 协议栈源码（`src/`）及与 GD32F4xx 网卡接口的移植适配层（`port/`）。 |
| 📂 [`Project`](Project/) | **工程编译与构建文件** | 开发环境工程文件。包含 Keil uVision5 工程（`GD32f450.uvprojx`）、VS Code EIDE 工程配置（`.eide`）以及编译产生的中间文件目录（`Objects/`, `Listings/`）。 |
| 📂 [`Doc`](Doc/) | **项目文档与参考资料** | 包含项目相关的设计文档、硬件引脚映射说明（`GD32F470_Pin_Map.md`）、芯片 Datasheet（如 `YT8512C`）、算法逻辑说明（`CS1237_AutoRanging_Logic.md`）及参考驱动源码。 |

---

## 🛠️ 主要硬件与协议支持

- **主控 MCU**: GD32F4xx (ARM Cortex-M4)
- **操作系统**: FreeRTOS V11.3.0
- **网络协议栈**: LwIP (支持 PHY 芯片 YT8512C/YT8512H)
- **高精度 ADC**: CS1237 (24-bit Sigma-Delta ADC)
- **电能计量**: HLW8112
- **模拟输出**: DAC (数模转换)

---

## 💻 开发环境

- **IDE 选项 1**: Keil uVision 5 (`Project/GD32f450.uvprojx`)
- **IDE 选项 2**: Embedded IDE (EIDE) in VS Code (`Project/GD32f450.code-workspace`)
