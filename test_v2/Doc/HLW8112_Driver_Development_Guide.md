# GD32F470 HLW8112 交流电计量外设驱动开发与测试指南

本文档记录了在 **GD32F470ZGT6** 主控芯片及 **FreeRTOS** 实时操作系统环境下，集成 **HLW8112 高精度单相交流电计量芯片** 的完整驱动架构、硬件引脚接线、通信协议、内部出厂标定与 Checksum 自动校验机制、物理量换算公式及测试方法。

---

## 1. 硬件架构与引脚映射 (适配立创梁山派)

HLW8112 支持 SPI 模式通信。为了最大程度提高可移植性与可靠性，驱动采用了软件 GPIO SPI 模式。

### 1.1 引脚对照表

| 信号名称 | HLW8112 引脚 | GD32F470 GPIO (梁山派) | 引脚模式 / 默认状态 | 说明 |
| :--- | :--- | :--- | :--- | :--- |
| **CS** | CS | **PB5** | 输出 (推挽 50MHz) / 高电平 | 软件片选，低电平有效 |
| **SCLK** | SCLK | **PB6** | 输出 (推挽 50MHz) / 低电平 | SPI 通信时钟信号 |
| **SDI (MOSI)** | SDI | **PB7** | 输出 (推挽 50MHz) / 低电平 | 主机发送数据至 HLW8112 |
| **SDO (MISO)** | SDO | **PB8** | 输入 (上拉) | HLW8112 返回数据至主机 |
| **EN (SEL)** | EN / SEL | **PB9** | 输出 (推挽 50MHz) / 高电平 | 芯片使能/复位线：高电平使能 SPI 模式 |

> **提示**：如果插在开发板 PB11~PB15 排针上，只需修改 `User/bsp_hlw8112.h` 顶部的 GPIO 宏定义即可。

---

## 2. 软件 SPI 协议与寄存器访问

### 2.1 数据帧格式

HLW8112 SPI 数据按 **MSB (高位在前)** 顺序传输。

- **读寄存器帧格式**：
  1. 片选 `CS` 拉低；
  2. 发送 1 字节地址字节 `RegAddr & 0x7F` (Bit 7 = 0)；
  3. 连续按 MSB 顺序读取 2、3 或 4 字节数据；
  4. 片选 `CS` 拉高。

- **写寄存器帧格式**：
  1. 写解锁：向 SPI 写入命令 `0xEA` 跟着写使能命令 `0xE5`；
  2. 写入目标寄存器：发送地址字节 `RegAddr | 0x80` (Bit 7 = 1)，紧跟高字节与低字节数据；
  3. 写锁定：向 SPI 写入命令 `0xEA` 跟着写锁定命令 `0xDC`。

---

## 3. 核心寄存器与物理量换算公式

### 3.1 核心寄存器映射表

| 寄存器名称 | 地址 | 长度 | 读写权限 | 说明 |
| :--- | :--- | :--- | :--- | :--- |
| `REG_SYSCON` | `0x00` | 16-bit | 读写 | 系统控制寄存器 (控制 PGA 增益) |
| `REG_EMUCON` | `0x01` | 16-bit | 读写 | 电力计量控制寄存器 (开启能量累加) |
| `REG_EMUCON2` | `0x13` | 16-bit | 读写 | 计量控制寄存器2 (过零检测与波形采样) |
| `REG_ANGLE` | `0x22` | 16-bit | 只读 | 相角寄存器 |
| `REG_UFREQ` | `0x23` | 16-bit | 只读 | 电压线频率寄存器 |
| `REG_RMSIA` | `0x24` | 24-bit | 只读 | 通道 A 电流有效值 raw |
| `REG_RMSIB` | `0x25` | 24-bit | 只读 | 通道 B 电流有效值 raw |
| `REG_RMSU` | `0x26` | 24-bit | 只读 | 电压有效值 raw |
| `REG_PF` | `0x27` | 24-bit | 只读 | 功率因数 raw |
| `REG_ENERGY_PA` | `0x28` | 24-bit | 只读 | 通道 A 有功电能 raw |
| `REG_POWER_PA` | `0x2C` | 32-bit | 只读 | 通道 A 有功功率 raw (有符号) |
| `REG_IE` | `0x40` | 16-bit | 读写 | 中断使能寄存器 |
| `REG_CHECKSUM` | `0x6F` | 16-bit | 只读 | 芯片出厂校准参数校验和寄存器 |
| `REG_RMS_IAC` ~ `ENERGY_BC`| `0x70`~`0x77` | 16-bit | 只读 | 芯片内部出厂标定参数 |

### 3.2 芯片出厂标定与 Checksum 自动校验

HLW8112 芯片内部在出厂时固化了当前硬件环境下的转换系数（`0x70` ~ `0x77`）。
驱动在 `hlw8112_init()` 时会自动读取这些系数，并校验 Checksum：

$$\text{Checksum}_{\text{calc}} = \sim \left( 0xFFFF + \text{RMS}_{\text{IAC}} + \text{RMS}_{\text{IBC}} + \text{RMS}_{\text{UC}} + \text{POWER}_{\text{PAC}} + \text{POWER}_{\text{PBC}} + \text{POWER}_{\text{SC}} + \text{ENERGY}_{\text{AC}} + \text{ENERGY}_{\text{BC}} \right) \& 0xFFFF$$

当 `Checksum_calc == Checksum_reg` 时，`calib_ok` 置为 `true`。

### 3.3 物理量计算公式

1. **电网频率 ($F$, Hz)**：
   $$F = \frac{3579545.0}{8.0 \times \text{Raw\_UFREQ}}$$

2. **交流电压有效值 ($V$, V)**：
   $$V = \frac{\text{Raw\_RMSU} \times \text{RMS\_UC}}{2^{22} \times 100}$$

3. **通道 A 电流有效值 ($I_A$, A)**：
   $$I_A = \frac{\text{Raw\_RMSIA} \times \text{RMS\_IAC}}{2^{23} \times 1000}$$

4. **通道 A 有功功率 ($P_A$, W)**：
   $$P_A = \frac{\text{Raw\_POWER\_PA} \times \text{POWER\_PAC}}{2^{31}}$$

5. **通道 A 有功电能 ($E_A$, kWh)**：
   $$E_A = \frac{\text{Raw\_ENERGY\_PA} \times \text{ENERGY\_AC}}{2^{29}}$$

6. **功率因数 ($PF$)**：
   $$PF = \frac{\text{Raw\_PF}}{8388607.0}$$

---

## 4. 驱动文件架构

- **`User/bsp_hlw8112.h`**：定义 GPIO 引脚映射、HLW8112 内部寄存器映射表、数据结构体 `hlw8112_data_t` 及 API 接口。
- **`User/bsp_hlw8112.c`**：实现软件 SPI 传输、写保护解锁/锁定、出厂标定参数读取与 Checksum 校验、全量参数测量及单位转换。
- **`User/main.c`**：初始化外设并在 FreeRTOS 中建立 `HLW8112Task` 采样任务。
- **`Project/GD32f450.uvprojx`**：Keil 项目工程配置。

---

## 5. 测试与数据验证步骤

### 步骤 1：上电通信测试
烧录程序后打开串口调试助手 (115200 8N1)，观察串口初始化输出：
```text
HLW8112 AC Metering Peripheral Initialized (CS: PB5, SCLK: PB6, SDI: PB7, SDO: PB8, EN: PB9)
```

### 步骤 2：读数校验
观察 `[HLW8112 Measure]` 定期打出的日志：
```text
[HLW8112 Measure] Voltage: 220.45 V | Current:   0.455 A ( 455.0 mA) | ActivePower:  100.25 W | PF: 0.999 | Freq: 50.01 Hz | Angle:   0.0 Deg | Energy: 0.0012 kWh
```
如果输出：
```text
[HLW8112 Status] Calibration Checksum Failed / Check SPI Wiring!
```
则表示 SPI 接线不良或芯片未正常供电/使能。
