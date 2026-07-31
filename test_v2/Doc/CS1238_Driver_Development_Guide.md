# CS1238 24位双通道 ADC 外设驱动开发与集成指南

## 1. 概述 (Overview)

CS1238 是由芯海科技（CHIPSEA）推出的一款高精度、低功耗 24 位 Sigma-Delta 模数转换芯片（ADC）。
本驱动针对 **GD32F470** 主控芯片（主频 240MHz），基于 FreeRTOS 架构开发，实现了对 CS1238 的 GPIO 软件 2 线串行通信、配置寄存器读写、双通道（CH1 / CH2 差分输入）切换、内部温度传感器读取、Auto-Range 动态增益切换与毫伏（mV）/ 微伏（uV）电压转换功能。

---

## 2. 硬件引脚映射 (Hardware Pin Mapping)

由于 `PC4` 和 `PC5` 已被以太网 RMII 接口（`RMII_RXD0` / `RMII_RXD1`）占用，CS1238 驱动默认配置在 **GPIOD** 上：

| CS1238 引脚名称 | 信号功能 | GD32F470 GPIO 引脚 | 模式配置 | 备注 |
| :--- | :--- | :--- | :--- | :--- |
| **SCLK** | 串行时钟输入 | `PD0` | 推挽输出 (`GPIO_MODE_OUTPUT`) | 默认初始低电平 |
| **DOUT / DRDY** | 数据输出 / 准备就绪 | `PD1` | 浮空/上拉输入与推挽输出动态切换 | 数据准备好时降为低电平 |
| **VCC / GND** | 供电与地 | `3.3V` / `GND` | - | 支持 2.7V - 5.5V 供电 |

> **提示**：引脚定义位于 [bsp_cs1238.h](file:///d:/code/testController/testController/test_v2/User/bsp_cs1238.h) 中，可通过修改 `CS1238_CLK_PORT/PIN` 和 `CS1238_DOUT_PORT/PIN` 轻松重映射至任意 GPIO 引脚。

---

## 3. 通信时序与寄存器定义 (Communication & Register Mapping)

### 3.1 串行读写时序逻辑
- **ADC 数据读取**（脉冲 1 - 24）：当 DOUT 降为低电平时，产生 24 个 SCLK 脉冲移出 24 位 2 的补码 ADC 数据（高位 MSB 先出）。
- **转换/补充脉冲**（脉冲 25 - 27）：补充 3 个 SCLK 脉冲。
- **命令移出**（脉冲 28 - 36）：切换 DOUT 为输出模式，移出 7 位命令字：
  - `0x56`：读配置寄存器 (READ CMD)
  - `0x65`：写配置寄存器 (WRITE CMD)
- **数据传输与结束**（脉冲 37 - 46）：移入或移出 8 位 Config 寄存器配置字节。

### 3.2 Config 寄存器位结构 (8-bit)
| Bit 7 | Bit 6 | Bit 5 - 4 | Bit 3 - 2 | Bit 1 - 0 |
| :---: | :---: | :---: | :---: | :---: |
| Reserved (0) | REFO_OFF | SPEED[1:0] | PGA[1:0] | CH_SEL[1:0] |

- **REFO_OFF**：内部基准电压开关（0：开启 REFO，1：关闭 REFO）
- **SPEED**：输出速率（`00`: 10Hz, `01`: 40Hz, `10`: 640Hz, `11`: 1280Hz）
- **PGA**：增益倍数（`00`: 1X, `01`: 2X, `10`: 64X, `11`: 128X）
- **CH_SEL**：通道选择（`00`: CH1 差分, `01`: CH2 差分, `10`: 内部 Temp 传感器, `11`: 内部短路 Calibration）

---

## 4. 软件 API 说明 (Software API Specification)

### 4.1 核心驱动接口
- `void cs1238_init(void)`：初始化 GPIO 引脚并执行复位唤醒序列。
- `int32_t cs1238_read_adc_signed(uint8_t *success)`：读取带符号扩位的 32 位 ADC 原始测量值。
- `uint8_t cs1238_read_reg(void)`：读取 8 位 Config 寄存器。
- `void cs1238_write_reg(uint8_t reg_val)`：写入 8 位 Config 寄存器。
- `void cs1238_configure(cs1238_pga_t gain, cs1238_speed_t speed, cs1238_ch_t ch, cs1238_vref_t vref)`：高层配置函数。
- `void cs1238_select_channel(cs1238_ch_t ch)`：切换通道并自动丢弃 1 帧过渡数据。

### 4.2 双通道智能自适应增益调控算法 (Smart Adaptive Gain Algorithm)
驱动内置了针对双通道独立工作的 **智能自适应增益调控算法**（`cs1238_evaluate_smart_gain` & `cs1238_read_channel_smart_auto_range`）：

1. **通道独立状态记忆**：`CH1` 和 `CH2` 分别保存各自的 PGA 增益状态（`pga_ch1` 与 `pga_ch2`），互不干扰。
2. **防过载与溢出保护**：
   - 当单通道模数转换绝对值 `|ADC| > 7,500,000`（约满量程 89.4%）时，立即自动降级增益（如 `128X -> 64X -> 2X -> 1X`），防止信号饱和削顶。
3. **分辨率最大化与滞后防抖 (Hysteresis Guardband)**：
   - `1X -> 2X`：当 `|ADC| < 3,700,000` 时升级。
   - `2X -> 64X`：当 `|ADC| < 200,000` 时升级。
   - `64X -> 128X`：当 `|ADC| < 3,700,000` 时升级。
   - 双门限迟滞机制彻底避免了临界点处的“增益震荡/频繁切换”。
4. **数字滤波过渡帧自动刷新**：增益切换后自动重新配置寄存器，并自动丢弃 1 帧 Sigma-Delta 数字滤波未稳定的过渡数据，确保输出精准结果。

- `float cs1238_raw_to_voltage_mv(int32_t raw_val, cs1238_pga_t pga, float vref_volts)`：
  将 24 位补码原始值转换为物理电压值（毫伏 mV）。
- `void cs1238_read_all_channels(cs1238_data_t *data, float vref_volts)`：
  自动轮询通道 1 (CH1)、通道 2 (CH2) 及内部温度 (TEMP)，并将独立增益与转换结果填充至 `cs1238_data_t`。

---

## 5. FreeRTOS 任务集成示例 (Task Integration Example)

在 [main.c](file:///d:/code/testController/testController/test_v2/User/main.c) 中创建独立的 `app_task_cs1238` 任务：

```c
static void app_task_cs1238(void *pvParameters)
{
    cs1238_data_t cs1238_data;
    uint8_t reg_val = 0;

    (void)pvParameters;

    /* 打印 CS1238 寄存器初始值 */
    reg_val = cs1238_read_reg();
    printf("CS1238 Register Read Code: 0x%02X\r\n", reg_val);

    for (;;) {
        /* 读取双通道及温度数据 */
        cs1238_read_all_channels(&cs1238_data, 3.3f);
        if (cs1238_data.success) {
            printf("[CS1238 Dual-CH] Gain: %3dX | CH1: %8d (%8.4f mV) | CH2: %8d (%8.4f mV) | Temp: %8d\r\n",
                   cs1238_get_pga_multiplier(cs1238_data.active_pga),
                   (int)cs1238_data.raw_ch1,
                   cs1238_data.volt_ch1_mv,
                   (int)cs1238_data.raw_ch2,
                   cs1238_data.volt_ch2_mv,
                   (int)cs1238_data.raw_temp);
        } else {
            printf("[CS1238] Multi-Channel Read Timeout / DRDY Not Ready\r\n");
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

---

## 6. 工程源码文件清单 (Source Files List)

1. [User/bsp_cs1238.h](file:///d:/code/testController/testController/test_v2/User/bsp_cs1238.h)：驱动头文件与配置宏定义
2. [User/bsp_cs1238.c](file:///d:/code/testController/testController/test_v2/User/bsp_cs1238.c)：驱动源文件
3. [User/main.c](file:///d:/code/testController/testController/test_v2/User/main.c)：系统主入口与 FreeRTOS CS1238 采样任务
4. [Project/GD32f450.uvprojx](file:///d:/code/testController/testController/test_v2/Project/GD32f450.uvprojx)：Keil MDK 工程配置文件
