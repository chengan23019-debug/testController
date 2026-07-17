# GD32F470 梁山派 YT8512C 以太网驱动开发指南

本文档详细记录了在 **GD32F470** 梁山派开发板上进行 **YT8512C** 以太网 PHY 芯片驱动开发的引脚映射、核心配置、避坑指南及初始化时序，以便后续维护与扩展。

---

## 1. 硬件引脚分配 (Pin Mapping)

YT8512C 采用 **RMII (Reduced Media Independent Interface)** 接口与 GD32F470 建立通信。考虑到开发板上其他外设的复用情况，引脚做出了避让调整：

| 信号名称 | 功能描述 | MCU 引脚 | 复用功能选择 (AF) | 备注说明 |
| :--- | :--- | :--- | :--- | :--- |
| **TXC / REF_CLK** | 50MHz 参考时钟输入 | **PA1** | `GPIO_AF_11` | 由 PHY 芯片输出给 MCU PA1 |
| **MDIO** | SMI 数据线 (双向) | **PA2** | `GPIO_AF_11` | **需配置开启 MCU 内部上拉** |
| **RXDV / CRS_DV** | 接收数据有效信号 | **PA7** | `GPIO_AF_11` | RMII_CRS_DV |
| **MDC** | SMI 时钟线 | **PC1** | `GPIO_AF_11` | RMII_MDC |
| **RX0 / RXD0** | 接收数据线 0 | **PC4** | `GPIO_AF_11` | RMII_RXD0 |
| **RX1 / RXD1** | 接收数据线 1 | **PC5** | `GPIO_AF_11` | RMII_RXD1 |
| **TXEN / TX_EN** | 发送使能信号 | **PG11** | `GPIO_AF_11` | 避开已被占用的 PB11 |
| **TX0 / TXD0** | 发送数据线 0 | **PG13** | `GPIO_AF_11` | 避开已被占用的 PB12 |
| **TX1 / TXD1** | 发送数据线 1 | **PG14** | `GPIO_AF_11` | 避开已被占用的 PB13 |
| **RST / RESET** | 硬件复位引脚 (低电平有效) | **PA3** | `GPIO_MODE_OUTPUT` | 普通 GPIO，低电平复位，高电平运行 |

---

## 2. 驱动开发要点与核心配置

### 2.1 PHY 物理地址 (PHY Address)
*   **硬件设定**：YT8512C 的 PHY 地址在芯片复位释放时根据 `LED0/PHYAD[0]` 和 `LED1/PHYAD[1]` 引脚的电平状态决定。
*   **当前配置**：在梁山派板载模块中，默认地址为 **`0`**（即 `LED0` 和 `LED1` 均无外部强上拉，内部弱下拉起作用）。
*   **代码修改**：在 [gd32f4xx_enet.h](file:///d:/code/testController/testController/test/Firmware/GD32F4xx_standard_peripheral/Include/gd32f4xx_enet.h) 中必须将 PHY 地址宏修改为 `0U`：
    ```c
    #define PHY_ADDRESS                      ((uint16_t)0U)
    ```

### 2.2 寄存器特征与“0x0000 陷阱”
*   **现象**：在进行以太网 PHY 扫描时，如果读取 `PHY_REG_PID1` (寄存器 0x02)，读取出的值恒为 `0x0000`。
*   **原因**：Motorcomm 公司的 OUI 标识特征决定了 **YT8512C 的 ID 1 寄存器 (0x02) 的值确实为 `0x0000`**，而其唯一的特征码保存在 `PHY_REG_PID2` (寄存器 0x03) 中，值为 **`0x0128`**。
*   **诊断注意**：不能再使用 `phy_id != 0x0000` 来过滤有效设备，应通过读取 **`0x03`** 寄存器识别该芯片。

### 2.3 硬件复位时序 (Hardware Reset Timing)
*   YT8512C 官方规格书要求：`RESET_N` 低电平复位脉冲需保持 **至少 10ms**。
*   释放复位信号后，需等待 **至少 50ms~100ms**，让 PHY 内部 PLL 锁相环起振并输出稳定的 50MHz 时钟，之后才能进行 MDC/MDIO 总线访问和以太网 DMA 软件复位。
*   **实现代码**（位于 [bsp_yt8512c.c](file:///d:/code/testController/testController/test/Hardware/YT8512C/bsp_yt8512c.c)）：
    ```c
    static void yt8512c_hw_reset(void)
    {
        rcu_periph_clock_enable(YT8512C_RST_RCU);
        gpio_mode_set(YT8512C_RST_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, YT8512C_RST_PIN);
        gpio_output_options_set(YT8512C_RST_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, YT8512C_RST_PIN);

        /* 拉低复位引脚保持 20ms */
        gpio_bit_reset(YT8512C_RST_PORT, YT8512C_RST_PIN);
        delay_1ms(20);
        
        /* 释放复位，拉高，等待 100ms 使时钟和内部 PLL 建立稳定 */
        gpio_bit_set(YT8512C_RST_PORT, YT8512C_RST_PIN);
        delay_1ms(100); 
    }
    ```

### 2.4 DMA 软件复位依赖 (Clock Dependency)
*   GD32F470 的以太网 MAC DMA 在执行软件复位（`enet_software_reset()`）时，**硬件上强依赖于 RMII PA1 引脚上的 50MHz 输入参考时钟**。
*   如果在软复位时时钟未就绪、不稳定或配置有误，`enet_software_reset()` 将超时返回 `ERROR` 导致初始化失败。因此在调用时应增加返回值校验进行总线自检。

### 2.5 链路状态读取与自协商配置 (Link Status & Autonegotiation)
*   YT8512C 的链路速度与双工配置状态保存在 **第 17号寄存器（0x11）** 即 `PHY Specific Status Register`。
*   在 [gd32f4xx_enet.h](file:///d:/code/testController/testController/test/Firmware/GD32F4xx_standard_peripheral/Include/gd32f4xx_enet.h) 中做如下适配：
    ```c
    #elif(PHY_TYPE == YT8512)
    #define PHY_SR                           17U        /* PHY状态寄存器地址 */
    #define PHY_SPEED_STATUS                 ((uint16_t)0x4000) /* Bit 14 表示 100Mbps 速度 */
    #define PHY_DUPLEX_STATUS                ((uint16_t)0x2000) /* Bit 13 表示 全双工模式 */
    #endif
    ```

---

## 3. 常见故障排查表

| 故障现象 | 可能的原因 | 排查与解决方法 |
| :--- | :--- | :--- |
| **SMI 读写超时 / 软复位超时** | 1. 50MHz 参考时钟缺失；<br>2. PHY 芯片未脱离复位状态。 | 1. 用示波器测量 PA1 是否有 50MHz 时钟输入；<br>2. 用示波器测量 PA3 (RST) 是否在初始化时正确被拉高；<br>3. 检查 `yt8512c_hw_reset()` 之后的等待延时是否足够。 |
| **读取所有 PHY 地址返回值全为 `0xFFFF`** | 1. MDIO 线悬空浮空；<br>2. MDC 或 MDIO 引脚配置/接线错误。 | 1. 确认 PA2 引脚已配置为 `GPIO_PUPD_PULLUP`（内部上拉）；<br>2. 检查开发板上的 MDC/MDIO 线是否虚焊或与 PHY 对应引脚接反。 |
| **读特定地址返回 `0x0000` 且初始化报错** | 1. 物理地址找对了但程序里硬编码错误。 | 1. 根据读出 `0x0000` 的地址（通常是 0），修改 `PHY_ADDRESS` 为对应数字；<br>2. 检查 `PHY_REG_PID2` (0x03) 能否正确读出芯片 ID `0x0128` |
