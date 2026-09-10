# 立创·梁山派 (GD32F470ZGT6) 片外 SDRAM 驱动开发指南

## 1. 硬件规格与参数

* **主控芯片**: GD32F470ZGT6 (Cortex-M4 @ 240MHz, EXMC控制器)
* **SDRAM 型号**: Winbond **W9825G6KH-6I**
* **存储容量**: 256 Mbit (32 MByte = 16M × 16-bit)
* **芯片架构**: 4 Banks × 4,194,304 words × 16 bits
* **行地址 (Row)**: A0 ~ A12 (13 位)
* **列地址 (Column)**: A0 ~ A8 (9 位)
* **数据位宽 (Data Bus)**: 16 位 (D0 ~ D15)
* **片选设备 (Chip Select)**: EXMC SDRAM Device 0 (`EXMC_SDRAM_DEVICE0`)
* **内存映射基地址**: `0xC0000000` (容量范围 `0xC0000000 ~ 0xC1FFFFFF`)

---

## 2. 引脚分配与 GPIO 复用表

> 所有 SDRAM 信号引脚在 GD32F470 上均配置为 **复用功能 12 (`GPIO_AF_12`)**、**推挽输出 (`GPIO_OTYPE_PP`)**、**最大输出速率 (`GPIO_OSPEED_MAX`)**、**内部弱上拉 (`GPIO_PUPD_PULLUP`)**。

| 信号类型 | 信号名称 | 芯片引脚 | GD32F470 引脚 | 复用功能 (AF) | 功能说明 |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **地址线 (13根)** | EXMC_A0 | Pin 23 | **PF0** | `GPIO_AF_12` | 行/列地址 A0 |
| | EXMC_A1 | Pin 24 | **PF1** | `GPIO_AF_12` | 行/列地址 A1 |
| | EXMC_A2 | Pin 25 | **PF2** | `GPIO_AF_12` | 行/列地址 A2 |
| | EXMC_A3 | Pin 26 | **PF3** | `GPIO_AF_12` | 行/列地址 A3 |
| | EXMC_A4 | Pin 29 | **PF4** | `GPIO_AF_12` | 行/列地址 A4 |
| | EXMC_A5 | Pin 30 | **PF5** | `GPIO_AF_12` | 行/列地址 A5 |
| | EXMC_A6 | Pin 31 | **PF12** | `GPIO_AF_12` | 行/列地址 A6 |
| | EXMC_A7 | Pin 32 | **PF13** | `GPIO_AF_12` | 行/列地址 A7 |
| | EXMC_A8 | Pin 33 | **PF14** | `GPIO_AF_12` | 行/列地址 A8 |
| | EXMC_A9 | Pin 34 | **PF15** | `GPIO_AF_12` | 行地址 A9 |
| | EXMC_A10 | Pin 22 | **PG0** | `GPIO_AF_12` | 行地址 A10 (AP 预充电控制) |
| | EXMC_A11 | Pin 35 | **PG1** | `GPIO_AF_12` | 行地址 A11 |
| | EXMC_A12 | Pin 36 | **PG2** | `GPIO_AF_12` | 行地址 A12 |
| **数据线 (16根)** | EXMC_D0 | Pin 2 | **PD14** | `GPIO_AF_12` | 数据总线 D0 |
| | EXMC_D1 | Pin 4 | **PD15** | `GPIO_AF_12` | 数据总线 D1 |
| | EXMC_D2 | Pin 5 | **PD0** | `GPIO_AF_12` | 数据总线 D2 |
| | EXMC_D3 | Pin 7 | **PD1** | `GPIO_AF_12` | 数据总线 D3 |
| | EXMC_D4 | Pin 8 | **PE7** | `GPIO_AF_12` | 数据总线 D4 |
| | EXMC_D5 | Pin 10 | **PE8** | `GPIO_AF_12` | 数据总线 D5 |
| | EXMC_D6 | Pin 11 | **PE9** | `GPIO_AF_12` | 数据总线 D6 |
| | EXMC_D7 | Pin 13 | **PE10** | `GPIO_AF_12` | 数据总线 D7 |
| | EXMC_D8 | Pin 42 | **PE11** | `GPIO_AF_12` | 数据总线 D8 |
| | EXMC_D9 | Pin 44 | **PE12** | `GPIO_AF_12` | 数据总线 D9 |
| | EXMC_D10 | Pin 45 | **PE13** | `GPIO_AF_12` | 数据总线 D10 |
| | EXMC_D11 | Pin 47 | **PE14** | `GPIO_AF_12` | 数据总线 D11 |
| | EXMC_D12 | Pin 48 | **PE15** | `GPIO_AF_12` | 数据总线 D12 |
| | EXMC_D13 | Pin 50 | **PD8** | `GPIO_AF_12` | 数据总线 D13 |
| | EXMC_D14 | Pin 51 | **PD9** | `GPIO_AF_12` | 数据总线 D14 |
| | EXMC_D15 | Pin 53 | **PD10** | `GPIO_AF_12` | 数据总线 D15 |
| **控制线** | SDR_NBL0 | Pin 15 | **PE0** | `GPIO_AF_12` | 低字节掩码 (LDQM) |
| | SDR_NBL1 | Pin 39 | **PE1** | `GPIO_AF_12` | 高字节掩码 (UDQM) |
| | SDR_CKE0 | Pin 37 | **PC3** | `GPIO_AF_12` | 时钟使能 (CKE0) |
| | SDR_NE0 | Pin 19 | **PC2** | `GPIO_AF_12` | 片选 CS# (Device 0) |
| | SDR_NWE | Pin 16 | **PC0** | `GPIO_AF_12` | 写使能 WE# |
| | SDR_NRAS | Pin 18 | **PF11** | `GPIO_AF_12` | 行选通 RAS# |
| | SDR_NCAS | Pin 17 | **PG15** | `GPIO_AF_12` | 列选通 CAS# |
| | SDR_BA0 | Pin 20 | **PG4** | `GPIO_AF_12` | Bank 选择 0 (BS0) |
| | SDR_BA1 | Pin 21 | **PG5** | `GPIO_AF_12` | Bank 选择 1 (BS1) |
| | SDR_CLK | Pin 38 | **PG8** | `GPIO_AF_12` | SDRAM 同步时钟 |

---

## 3. EXMC 与 SDRAM 时序参数计算

GD32F470 主频为 **240MHz**，EXMC SDCLK 设置为 `EXMC_SDCLK_PERIODS_2_HCLK` (即 $HCLK / 2 = 120\text{MHz}$)，时钟周期 $t_{CK} \approx 8.33\text{ns}$。

| 参数名称 | 符号 | W9825G6KH-6I 规格 | 时钟周期换算 ($t_{CK}=8.33\text{ns}$) | EXMC 寄存器配置值 |
| :--- | :--- | :--- | :--- | :--- |
| 行至列延时 (RAS to CAS) | $t_{RCD}$ | $\min 18\text{ns}$ | $18 / 8.33 = 2.16$ | **3 cycles** |
| 行预充电时间 (Row Precharge) | $t_{RP}$ | $\min 18\text{ns}$ | $18 / 8.33 = 2.16$ | **3 cycles** |
| 写恢复时间 (Write Recovery) | $t_{WR}$ | 2 cycles | 2 cycles | **2 cycles** |
| 自动刷新周期 (Row Cycle) | $t_{RC} / t_{RFC}$ | $\min 60\text{ns}$ | $60 / 8.33 = 7.2$ | **8 cycles** |
| 行有效时间 (Row Active Time) | $t_{RAS}$ | $\min 42\text{ns}$ | $42 / 8.33 = 5.04$ | **6 cycles** |
| 退出自刷新恢复时间 | $t_{XSR}$ | $\min 72\text{ns}$ | $72 / 8.33 = 8.64$ | **9 cycles** |
| 模式寄存器设置延时 | $t_{MRD}$ | 2 cycles | 2 cycles | **2 cycles** |
| 列选通潜伏期 (CAS Latency) | $CL$ | 2 或 3 | 高频下配置为 3 | **3 SDCLK** |

### 自动刷新间隔计数器 (Auto-Refresh Counter) 计算
* 刷新要求: 64ms 内完成 8192 次刷新，即 $\text{Refresh Period} = \frac{64\text{ms}}{8192} = 7.8125\mu\text{s}$。
* 刷新计数值公式:
  $$\text{COUNT} = (7.8125\mu\text{s} \times \text{SDCLK}) - 20$$
* 当 $\text{SDCLK} = 120\text{MHz}$ 时:
  $$\text{COUNT} = (7.8125 \times 120) - 20 = 937.5 - 20 = 917.5 \approx 917 \text{ (0x395)}$$

---

## 4. SDRAM 标准上电初始化序列

依据 SDRAM 标准与 GD32 EXMC 硬件规范，驱动在 `bsp_sdram_init()` 中依次执行：
1. **GPIO 与 EXMC 时钟使能**: 使能 GPIOC/D/E/F/G 与 EXMC 时钟。
2. **GPIO 引脚配置**: 将 39 个相关引脚配置为 `AF12` 高速推挽上拉模式。
3. **EXMC 控制器与时序参数配置**: 调用 `exmc_sdram_init()`。
4. **发送 Clock Enable 命令**: 开启 SDRAM 时钟输出。
5. **硬件延时**: 延时至少 $200\mu\text{s}$（驱动延时 $300\mu\text{s}$）等待时钟稳定。
6. **发送 Precharge All 命令**: 对所有 4 个内部 Bank 进行预充电。
7. **发送连续 8 次 Auto Refresh 命令**: 稳定 SDRAM 内部电路。
8. **发送 Load Mode Register 命令**: 写入模式寄存器（突发长度=1、连续模式、CAS=3、标准模式、单次写）。
9. **配置自动刷新计数器**: 设置硬件定时刷新（917 周期）。
10. **使能读延迟采样校准**: 启用 `exmc_sdram_readsample_enable(ENABLE)` 和 `EXMC_SDRAM_6_DELAY_CELL`，保障 120MHz 高速读取稳定性。

---

## 5. API 接口与使用示例

### 5.1 头文件包含
```c
#include "bsp_sdram.h"
```

### 5.2 初始化与自检
```c
/* 在系统初始化时调用 */
bsp_sdram_init();

/* 执行硬件完整性自检 (数据线走步测试、地址线测试、混合位宽测试、4MB块读写测试) */
if (bsp_sdram_test() == 0) {
    printf("SDRAM Init & Test PASSED!\r\n");
} else {
    printf("SDRAM Test FAILED!\r\n");
}
```

### 5.3 内存指针直接访问
由于 SDRAM 映射在内部地址空间 `0xC0000000`，支持直接用 C 语言指针访问：
```c
/* 1. 直接当做大数组使用 */
#define EXT_RAM_BASE   ((uint32_t *)0xC0000000)

EXT_RAM_BASE[0] = 0x12345678;
uint32_t val = EXT_RAM_BASE[0];

/* 2. 定义大缓冲区/显存/网络大缓存 */
uint8_t *p_framebuffer = (uint8_t *)0xC0000000;
```

### 5.4 驱动辅助读写函数
```c
uint8_t tx_buf[1024];
uint8_t rx_buf[1024];

/* 写入 1024 字节到 SDRAM 偏移 0 处 */
bsp_sdram_write_buffer(tx_buf, 0, sizeof(tx_buf));

/* 从 SDRAM 偏移 0 处读回 1024 字节 */
bsp_sdram_read_buffer(rx_buf, 0, sizeof(rx_buf));
```

---

## 6. 注意事项与引脚复用提醒

> [!WARNING]
> 1. **引脚冲突排查**:
>    在原工程中如果有使用以下引脚，请务必注意冲突并调整：
>    * `PC2` (SDR_NE0), `PC3` (SDR_CKE0) 原先若用于 CS1237 或其他传感器 GPIO，需移至其他空闲引脚。
>    * `PD0` (EXMC_D2), `PD1` (EXMC_D3) 原先若用于 CS1238 或其他传感器 GPIO，需移至其他空闲引脚。
> 2. **Keil 工程配置**:
>    * 工程已自动包含 `bsp_sdram.c` 和 `Firmware/GD32F4xx_standard_peripheral/Source/gd32f4xx_exmc.c`。
>    * `gd32f4xx_libopt.h` 中已开启 `#include "gd32f4xx_exmc.h"`。
