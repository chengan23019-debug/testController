# 测功机传感器 50Hz 同步采样与滤波算法开发指南

**文档标识**：`DOC-DYNO-SENS-FILTER-001`  
**适用主控**：GD32F470ZGT6 (240MHz ARM Cortex-M4F)  
**操作系统**：FreeRTOS V10.4.x  
**外设清单**：CS1237 (扭矩 ADC)、CS1238 (直流电参 ADC)、HLW8112 (交流电计量)、TIMER 硬件输入捕获 (转速)  

---

## 一、 系统架构与 50Hz 同步时基设计

### 1.1 统一 50Hz (20ms) 采样时基的设计动因
在电机测功机测控系统中，数据采集面临强电磁干扰（变频器 PWM、IGBT 斩波、大电流突变）与动态闭环响应的严苛要求。将所有传感器统一规划为 **50Hz 采样率（即 $T = 20\text{ms}$ 采样周期）** 具备以下核心工程优势：

1. **天然工频陷波**：中国工业电网标准频率为 50Hz（周期 20ms）。以 20ms 整数倍为采样基准，在物理和数学上能够天然抵消整流电源与电网耦合的 50Hz/100Hz 交流纹波干扰。
2. **与以太网遥测无缝对齐**：系统网络层 `tcp_client_send_telemetry()` 原生按 50Hz（20ms）节拍向测功机上位机推流。统一 50Hz 实现 **“单次采样 - 立即打包 - 单包推送”**，消除跨任务数据插值与相位抖动。
3. **解耦多路 ADC 转换时间约束**：双通道复用 ADC（CS1238）在 640Hz 速率下单次完整双通道转换耗时为 $6.25\text{ms}$，在 20ms 时基内具备充足的安全时间裕量（裕量达 68.75%）。

```mermaid
flowchart TD
    subgraph Interrupt_Layer [硬件中断与底层采集层]
        CS1237_HW["CS1237 扭矩传感器\n(SPEED=640Hz, T=1.56ms)"] -->|DOUT 下降沿 EXTI 中断\n27 脉冲高速直读 (15us)| CS1237_Buf["16 点环形缓冲区\n(累加和实时更新)"]
        TIM_HW["旋转编码器 / 接近开关"] -->|硬件定时器输入捕获\n硬件数字滤波 (CH0FLT)| TIM_CNT["定时器脉冲计数 CNT\n& 1us 高精度时间戳"]
    end

    subgraph Sync_Task [50Hz / 20ms 核心同步采集与调度任务]
        CS1237_Buf -->|20ms 过采样算术均值| Snap["50Hz 全传感器同步快照"]
        CS1238_HW["CS1238 直流传感器\n(独立 SCK, 640Hz 双通道)"] -->|50Hz 周期轮询\nCH1/CH2 状态机采集| Snap
        HLW_HW["HLW8112 交流计量\n(50Hz 快速 SPI 读数)"] -->|单次 180us 突发读\n工频周期积分有效值| Snap
        TIM_CNT -->|M/T 法数学解析\n20ms 窗口转速运算| Snap
        
        Snap --> Telemetry["app_dyno_update_telemetry()\n原子刷新全局数据中心"]
    end

    subgraph App_Layer [控制与通信层]
        Telemetry -->|无锁原子读取| Control["测功机加载 PID 控制任务\n(恒转速/恒扭矩/恒功率)"]
        Telemetry -->|无锁原子读取| NetTask["以太网 LwIP 推流任务\n(50Hz 56-Byte 二进制报文)"]
    end
```

---

## 二、 硬件引脚分配与总线解耦规范

> [!IMPORTANT]
> **时钟引脚物理隔离原则**：CS1237 与 CS1238 虽然通信协议相似，但二者采用芯片内部各自独立的 RC 振荡器。**严禁共用 SCLK 引脚**！共用时钟会导致任一芯片的读取脉冲强行打乱另一芯片的转换时序。

| 传感器名称 | 测量物理量 | 信号引脚 | GD32F470 GPIO | 模式与电气特性 | 备注 |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **CS1237** | 轴扭矩 (Nm) | **SCLK** | `PF6` | GPIO 推挽输出 (50MHz) | 扭矩独立时钟线 |
| | | **DOUT** | `PF7` | GPIO 上拉输入 / EXTI7 下降沿中断 | 兼作数据/就绪信号 |
| **CS1238** | 直流母线电压/电流 | **SCLK** | `PF8` | GPIO 推挽输出 (50MHz) | **直流独立时钟线 (解除共用)** |
| | | **DOUT** | `PF9` | GPIO 上拉输入 / 输出动态切换 | 数据就绪与寄存器配置 |
| **HLW8112** | 交流电压/电流/功率 | **CS** | `PB5` | GPIO 推挽输出 (默认高) | 软件片选 |
| | | **SCLK** | `PB6` | GPIO 推挽输出 (默认低) | SPI 时钟 (优化至 1MHz 级) |
| | | **SDI** | `PB7` | GPIO 推挽输出 | MOSI 数据发送 |
| | | **SDO** | `PB8` | GPIO 上拉输入 | MISO 数据接收 |
| | | **EN** | `PB9` | GPIO 推挽输出 (高电平使能) | 芯片工作模式使能 |
| **测速码盘** | 电机实时转速 (RPM) | **PULSE** | `PA0` / `PA15` | 定时器复用输入 (`TIMERx_CHy`) | 开启硬件输入捕获滤波 |

---

## 三、 各传感器 50Hz 采样驱动方案

### 3.1 扭矩传感器 (CS1237)
* **芯片寄存器配置**：`SPEED[1:0] = 10` (640Hz)，`PGA[1:0] = 11` (128X 增益)，`CH_SEL[1:0] = 00` (通道 A)。
* **中断读取时序规范（27 脉冲法则）**：
  * 当转换就绪，DOUT 引脚产生下降沿触发单片机 `EXTI7` 中断；
  * **脉冲 1 ~ 24**：MCU 产生 24 个 SCLK 周期，在上升沿后读取 DOUT，拼装 24 位最高有效位先出（MSB First）补码；
  * **脉冲 25 ~ 27**：MCU 必须额外送出 **3 个 SCLK 脉冲**。该 3 脉冲用于使 CS1237 内部逻辑完全释放 DOUT 引脚回到高阻/高电平就绪态；
  * 中断内读取总耗时 $\approx 15\mu\text{s}$，仅将数据推入环形缓冲，严禁在中断内进行浮点电压或物理扭矩转换。

### 3.2 直流电参数传感器 (CS1238)
* **芯片寄存器配置**：`SPEED[1:0] = 10` (640Hz)，CH1 (电压通道) PGA=1X/2X，CH2 (分流器电流通道) PGA=64X/128X。
* **双通道轮询时序推算**：
  * 640Hz 下单帧周期为 $1.5625\text{ms}$；
  * 步骤 ①：读取当前通道有效帧（耗时 $1.56\text{ms}$）；
  * 步骤 ②：发送 46 脉冲重写配置寄存器切换至另一通道；
  * 步骤 ③：**丢弃切换后的第 1 帧过渡数据**（Sinc 滤波器重建时间，耗时 $1.56\text{ms}$）；
  * 步骤 ④：读取第二通道真实有效帧（耗时 $1.56\text{ms}$）；
  * 步骤 ⑤：切回第一通道并丢弃过渡帧（耗时 $1.56\text{ms}$）；
  * **全流程闭环总耗时**：$4 \times 1.5625\text{ms} = 6.25\text{ms}$。在 50Hz（20ms）任务调度下，单片机采用非阻塞状态机等待，耗时远小于 20ms，采集极其平稳。

### 3.3 交流电参数计量 (HLW8112)
* **时序与电网同步原理**：
  * HLW8112 内部通过硬件乘法器与数字积分器计算瞬时功率与有效值。50Hz 交流电单周波为 20ms，50Hz 任务每 20ms 发起一次读取，正好与电网整周期对应。
* **微秒级软件 SPI 时序压减**：
  * 原驱动采用 `hlw8112_delay_us(5)`，读一次全量寄存器耗时超 1.2ms。
  * 将软件 SPI 延迟压缩至 `hlw8112_delay_us(1)`（对应 SCLK 频率约 500kHz~1MHz）：
    * 读电压寄存器 `REG_RMSU` (3 字节) 耗时 $\approx 40\mu\text{s}$；
    * 读电流寄存器 `REG_RMSIA` (3 字节) 耗时 $\approx 40\mu\text{s}$；
    * 读有功功率 `REG_POWER_PA` (4 字节) 耗时 $\approx 50\mu\text{s}$；
    * 单次总事务耗时 $\approx 150\mu\text{s}$，CPU 占用率低于 0.8%。

### 3.4 转速传感器 (定时器 M/T 测速法)
* **20ms 窗口转速计算模型**：
  * 严禁采用纯软件轮询读取 GPIO！
  * 采用硬件定时器输入捕获，开辟 1MHz 高频基准时基计数器（分辨率 $1\mu\text{s}$）。
  * 在 50Hz 采样中断到达时刻，记录当前时刻捕获到的脉冲总增量 $\Delta M$ 以及对应的精密微秒时间戳差值 $\Delta T$：
    $$\text{RPM} = \frac{\Delta M}{\text{PPR} \times \left( \Delta T \times 10^{-6} \right)} \times 60$$
    *其中 $\text{PPR}$ 为测速盘每转脉冲数（Pulses Per Revolution）。*
  * **精度提升对比**：在 20ms 窗口下，采样脉冲数比 300Hz（3.33ms）提升了 6 倍，极低转速下的量化离散跳变基本消除。

---

## 四、 传感器针对性滤波算法与数学模型

> [!CAUTION]
> **滤波基本准则：禁止对转速控制参数施加大时间常数滤波！**  
> 在闭环控制理论中，低通滤波引入的相位滞后（Phase Lag）会直接劣化闭环相位裕度。转速反馈回路若增加数十毫秒延迟，将直接诱发测功机加载 PID 发散、飞车或低频剧烈振荡。

### 4.1 滤波方案对比与选型原则

| 传感器维度 | 物理信号特征 | 主要噪声来源 | 推荐滤波算法 | 算法时间延迟 |
| :--- | :--- | :--- | :--- | :--- |
| **CS1237 扭矩** | 毫伏微弱应变信号 | 变频器高频辐射、ADC 640Hz 白噪声 | **640Hz 过采样 + 20ms 滑动窗口均值** | 10ms (线性群时延) |
| **CS1238 直流** | 直流母线大信号 | 开关电源高频纹波、电机 PWM 尖峰 | **限幅消抖 + 轻度一阶 IIR 低通 ($\alpha=0.3$)** | $\approx 20\text{ms}$ |
| **HLW8112 交流**| 芯片内部计算结果 | SPI 通信瞬态位翻转、瞬时坏点 | **原值直出 + 坏点拦截与校验和防护** | **0ms (无附加延迟)** |
| **定时器转速** | 数字脉冲方波 | 接地干扰、火花与高频感应窄毛刺 | **定时器硬件数字滤波 (CHxFLT) + M/T 原生积分** | **0ms (硬件纳秒级)** |

---

### 4.2 扭矩传感器滤波：过采样均值模型 (Oversampling & Decimation)
* **数学原理**：
  CS1237 配置为 640Hz，在 20ms 窗口内触发 $N \approx 12 \sim 13$ 次。
  利用算术滑动均值：
  $$\bar{X} = \frac{1}{N} \sum_{i=1}^{N} X_i$$
  根据白噪声统计特性，信噪比提升倍数：
  $$\text{SNR}_{\text{gain}} = 10 \cdot \log_{10}(N) \approx 10 \cdot \log_{10}(12.8) \approx 11.07\text{ dB}$$
  相当于**直接恢复了约 $1.8$ 个 ADC 有效位数 (ENOB)**，使 640Hz 下的噪声水平回落至接近 40Hz 档位的高精度水平。

---

### 4.3 直流电参数滤波：一阶 IIR 滞后低通滤波
* **差分方程模型**：
  $$y[k] = \alpha \cdot x[k] + (1 - \alpha) \cdot y[k-1]$$
  推荐参数 $\alpha = 0.35$。
* **截止频率关系**：
  在采样频率 $f_s = 50\text{Hz}$ 时，对应截止频率：
  $$f_c \approx \frac{\alpha \cdot f_s}{2\pi} \approx \frac{0.35 \times 50}{6.283} \approx 2.78\text{ Hz}$$
  能够强力平滑母线电压因电机启停导致的微小电压波动，确保稳态效率计算的准确性。

---

### 4.4 交流电参数滤波：坏点与校验和拦截 (Sanity Guard)
HLW8112 内部已具备数字积分平滑机制，**严禁在单片机中二次均值化**。单片机仅执行有效性判定：
1. **边界检查**：若读取到的电压 $V_{\text{rms}} > 500\text{V}$ 或功率为非法 NaN，判定为异常帧；
2. **坏帧保持**：判定异常时，直接丢弃本次采样，沿用上一周期的稳定有效值，确保系统控制不会因通信瞬态单字节错误而崩溃。

---

### 4.5 转速传感器滤波：定时器硬件级消抖 (Input Capture Filter)
利用 GD32F470 定时器输入通道的数字时钟滤波器（`TIMER_CHxCTL0 -> CHxFLT`）：
* 配置采样频率分频（如 $f_{\text{DTS}} / 4$），设置采样持续确认点数（例如连续 8 次时钟高电平才视为有效跳变）；
* **硬件级直接吸收小于 $500\text{ns}$ 的尖峰脉冲**，软件层无需加任何滑动均值，保持转速闭环的零相位滞后。

---

## 五、 核心算法参考实现 (C99 / ARMCC V5)

### 5.1 扭矩 16 点环形缓冲区与过采样均值滤波模块
```c
#define TORQUE_RING_BUF_SIZE   16U

typedef struct {
    int32_t  buffer[TORQUE_RING_BUF_SIZE];
    uint8_t  head;
    uint8_t  count;
    int64_t  sum;
} torque_ring_buffer_t;

static torque_ring_buffer_t g_torque_buf = {0};

/* 在 CS1237 EXTI 下降沿中断服务函数 (640Hz) 中调用，耗时极短 */
void torque_filter_push_isr(int32_t raw_adc)
{
    if (g_torque_buf.count >= TORQUE_RING_BUF_SIZE) {
        g_torque_buf.sum -= g_torque_buf.buffer[g_torque_buf.head];
    } else {
        g_torque_buf.count++;
    }

    g_torque_buf.buffer[g_torque_buf.head] = raw_adc;
    g_torque_buf.sum += raw_adc;
    g_torque_buf.head = (g_torque_buf.head + 1U) % TORQUE_RING_BUF_SIZE;
}

/* 在 50Hz 主任务中提取 20ms 算术平均值 */
int32_t torque_filter_get_averaged(void)
{
    int32_t avg_val = 0;
    
    __disable_irq(); /* 保护多字节累加原子性 */
    if (g_torque_buf.count > 0) {
        avg_val = (int32_t)(g_torque_buf.sum / g_torque_buf.count);
    }
    __enable_irq();

    return avg_val;
}
```

### 5.2 直流电参数一阶 IIR 低通滤波实现
```c
typedef struct {
    float filtered_val;
    float alpha;
    uint8_t initialized;
} lowpass_filter_t;

static lowpass_filter_t g_dc_v_filter = {0.0f, 0.35f, 0};
static lowpass_filter_t g_dc_i_filter = {0.0f, 0.35f, 0};

float lowpass_filter_apply(lowpass_filter_t *filter, float raw_val)
{
    if (!filter->initialized) {
        filter->filtered_val = raw_val;
        filter->initialized = 1;
    } else {
        filter->filtered_val = (filter->alpha * raw_val) + ((1.0f - filter->alpha) * filter->filtered_val);
    }
    return filter->filtered_val;
}
```

### 5.3 50Hz 统一时基同步采集主任务结构
```c
void app_task_telemetry_50hz(void *pvParameters)
{
    TickType_t xLastWakeTime;
    const TickType_t xPeriod = pdMS_TO_TICKS(20); /* 20ms / 50Hz 严格对齐 */

    int32_t raw_torque_avg;
    float torque_nm;
    float speed_rpm;
    cs1238_data_t dc_data;
    hlw8112_data_t ac_data;

    xLastWakeTime = xTaskGetTickCount();

    for (;;) {
        /* 绝对延时保证 20ms 等间隔执行，无累积相位漂移 */
        vTaskDelayUntil(&xLastWakeTime, xPeriod);

        /* 1. 扭矩：提取 640Hz 环形缓冲 20ms 均值并换算 */
        raw_torque_avg = torque_filter_get_averaged();
        torque_nm = cs1237_raw_to_voltage_mv(raw_torque_avg, CS1237_PGA_128X, 3.3f) * DYN_TORQUE_COEFF;

        /* 2. 直流电参：读取并进行一阶低通滤波 */
        cs1238_read_all_channels(&dc_data, 3.3f);
        float dc_v = lowpass_filter_apply(&g_dc_v_filter, dc_data.volt_ch1_mv * DC_V_SCALE);
        float dc_i = lowpass_filter_apply(&g_dc_i_filter, dc_data.volt_ch2_mv * DC_I_SCALE);

        /* 3. 交流电参：50Hz 单次高速轮询 + 坏点校验 */
        hlw8112_read_data(&ac_data);

        /* 4. 转速：读取定时器 M/T 硬件捕获结果 (零附加延迟) */
        speed_rpm = bsp_timer_get_speed_rpm();

        /* 5. 组装同步快照并更新全局数据中心 */
        app_dyno_update_telemetry(torque_nm, speed_rpm, dc_v, dc_i, ac_data.active_power_a, ac_data.power_factor);

        /* 6. 50Hz 推送二进制以太网报文至上位机 */
        tcp_client_send_telemetry();
    }
}
```

---

## 六、 方案验证与测试验收规范

1. **CS1237 零点白噪声峰峰值验证**：
   * 在测功机空载停机状态下，通过上位机观察 50Hz 推送的扭矩数据。
   * 验收指标：扭矩底噪跳变幅度应稳定在 $\le \pm 0.05\text{ N}\cdot\text{m}$，无低频随机跳变。
2. **转速阶跃响应与相位超前验证**：
   * 电机突加负载转速下跌时，50Hz 转速输出曲线应在 $20\text{ms}$ 内迅速反映物理转速变化，无平滑拖尾，PID 加载无低频自激震荡。
3. **交流电参数工频对齐验证**：
   * 观察 HLW8112 有功功率与电压有效值，对比高精度电参数分析仪，数据刷新稳定，且 CPU 占用率稳定在 $5\%$ 以下。
