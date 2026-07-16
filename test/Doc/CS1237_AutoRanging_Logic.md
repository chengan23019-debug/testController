# CS1237 ADC 自动量程切换（自动换挡）逻辑说明

本文档详细记录了在 GD32F470 平台上为 CS1237 24位高精度 ADC 编写的**自动量程切换算法**的设计原理、阈值计算及防振荡机制。

---

## 1. 算法背景与数学模型

CS1237 是一款 24 位有符号差分 ADC，输出的数据范围为：
$$\text{Raw ADC Value} \in [-8388608, 8388607]$$

其差分输入电压的满量程范围（Full-Scale Range）为 $\pm V_{REF} / Gain$。
* 当信号较小（如 3mV）时，若使用 1X 增益，分辨率极低且噪声占比大；若自动切换至 128X 增益，可以将信号放大 128 倍，最大程度提升测量精度。
* 当信号较大（如 1V）时，若使用 128X 增益，信号会瞬间溢出饱和（过载），必须自动切换至 1X 或 2X 低增益挡位。

---

## 2. 阈值设计与防震荡（回差滞后）机制

为防止在挡位临界点附近，由于数据的微小波动（白噪声）导致程序在两个挡位之间**来回频繁换挡（振荡）**，算法引入了**回差滞后（Hysteresis）区间**。

### 核心不等式约束
如果从低增益挡位（如 1X）升挡到高增益挡位（如 2X）的升挡阈值为 $T_{up}$，那么在升挡发生后，由于增益翻倍，ADC 的读数值也会瞬间翻倍：
$$\text{New Code} = T_{up} \times \frac{Gain_{new}}{Gain_{old}}$$

为了确保升挡后，新读数**绝对不会**立即触发降挡阈值 $T_{down}$，系统必须满足以下不等式：
$$T_{up} \times \frac{Gain_{new}}{Gain_{old}} < T_{down}$$

---

### 3. 具体挡位参数表

我们设定系统的降挡阀值 $T_{down} = 6,800,000$（约占总满量程的 81%，预留约 19% 空间以避开 CS1237 内部 PGA 模拟运放的饱和截断区）。

根据上述约束，各挡位切换阈值配置如下：

| 当前增益 (Gain) | 读数条件 (绝对值) | 切换动作 | 切换后理论最高值 | 降挡阈值 | 安全缓冲区 (Guard-band) |
| :---: | :---: | :---: | :---: | :---: | :---: |
| **1X** | $> 6,800,000$ | 无法再降挡 | - | - | - |
| **1X** | $< 3,000,000$ | 升挡 $\rightarrow$ **2X** | $3,000,000 \times 2 = 6,000,000$ | $6,800,000$ | **`800,000 LSB`** (安全) |
| **2X** | $> 6,800,000$ | 降挡 $\rightarrow$ **1X** | $6,800,000 / 2 = 3,400,000$ | - | - |
| **2X** | $< 100,000$ | 升挡 $\rightarrow$ **64X** | $100,000 \times 32 = 3,200,000$ | $6,800,000$ | **`3,600,000 LSB`** (极安全) |
| **64X** | $> 6,800,000$ | 降挡 $\rightarrow$ **2X** | $6,800,000 / 32 = 212,500$ | - | - |
| **64X** | $< 3,000,000$ | 升挡 $\rightarrow$ **128X** | $3,000,000 \times 2 = 6,000,000$ | $6,800,000$ | **`800,000 LSB`** (安全) |
| **128X** | $> 6,800,000$ | 降挡 $\rightarrow$ **64X** | $6,800,000 / 2 = 3,400,000$ | - | - |

---

## 4. 软件实现细节 (main.c 伪代码)

```c
success = 0;
adc_val = cs1237_read_adc_signed(&success);
if (success) {
    adc_val_abs = abs(adc_val);
    pga_changed = 0;

    /* 1. 降挡判定（数据过大，溢出风险） */
    if (adc_val_abs > 6800000) {
        if (current_pga == CS1237_PGA_128X)      { current_pga = CS1237_PGA_64X; pga_changed = 1; }
        else if (current_pga == CS1237_PGA_64X)  { current_pga = CS1237_PGA_2X;  pga_changed = 1; }
        else if (current_pga == CS1237_PGA_2X)   { current_pga = CS1237_PGA_1X;  pga_changed = 1; }
    }
    /* 2. 升挡判定（数据太小，提高分辨率） */
    else {
        if (current_pga == CS1237_PGA_1X && adc_val_abs < 3000000)      { current_pga = CS1237_PGA_2X;   pga_changed = 1; }
        else if (current_pga == CS1237_PGA_2X && adc_val_abs < 100000)   { current_pga = CS1237_PGA_64X;  pga_changed = 1; }
        else if (current_pga == CS1237_PGA_64X && adc_val_abs < 3000000) { current_pga = CS1237_PGA_128X; pga_changed = 1; }
    }

    /* 3. 换挡处理：延迟避开脏数据 */
    if (pga_changed) {
        gain_multiplier = get_gain_multiplier(current_pga);
        cs1237_configure(current_pga, CS1237_SPEED_10HZ, CS1237_CH_A, CS1237_VREF_ON);
        delay_1ms(200); /* 必须延时 200ms 等待芯片模拟前端和滤波器建立稳定 */
        continue;       /* 略过本次打印，防止换挡过渡瞬间的数据突变 */
    }
    
    // ... 后续计算真实电压与卡尔曼滤波 ...
}
```

---

## 5. 换挡过渡期的“避震”处理
在调用 `cs1237_configure()` 写入新挡位后，CS1237 内部的 $\Sigma-\Delta$ 调制器和数字滤波器需要重新建立（Settling Time）。
为了避免读出换挡过渡期间不稳定的“脏数据”，算法：
1. 配置后加入了 **`200ms` 的硬件稳定延时**（因为 10Hz 下一个周期为 100ms，200ms 可以让芯片完成两次完整的转换，确保数据完全稳定）。
2. 在换挡成功后使用 `continue` 语句**跳出当前循环**，直接丢弃换挡期间的那次采样值，确保滤波输出端数据的平滑和连续。
