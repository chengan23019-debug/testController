# GD32F470 FreeRTOS 实时操作系统开发与移植总结

本文档总结了在 **GD32F470 (ARM Cortex-M4F)** 主控芯片上，使用 **Keil MDK (ARMCC V5)** 工具链将裸机工程升级重构为 **FreeRTOS V10 实时操作系统架构** 的全过程、关键技术要点、踩坑经验及后续外设扩展指导。

---

## 一、 工程背景与架构演进

### 1.1 硬件与开发环境
* **主控 MCU**：GD32F470 (240MHz/168MHz 主频，基于 ARM Cortex-M4F 内核，集成 FPU 硬件浮点单元)
* **编译器/IDE**：Keil uVision V5.36 (ARMCC V5.06 Compiler)
* **原有架构**：裸机模式（Main Loop `while(1)` + Systick 软件定时器轮询）
* **目标外设集成**：CS1237 24 位 Sigma-Delta 高精度 ADC 模数转换芯片

### 1.2 架构升级对比

```mermaid
graph TD
    subgraph 裸机架构 (Bare-Metal)
        A1[main 入口] --> B1[硬件初始化]
        B1 --> C1[while 1 主循环]
        C1 --> D1[LwIP 网络轮询]
        C1 --> E1[LED 延时翻转]
        C1 --> F1[网络 Link 状态检测]
        note1[缺点: 容易相互阻塞, 实时采样难以保障]
    end

    subgraph FreeRTOS 实时操作系统架构
        A2[main 入口] --> B2[硬件初始化]
        B2 --> C2[创建 App Tasks]
        C2 --> D2[vTaskStartScheduler]
        D2 --> E2[Network Task - 优先级 5]
        D2 --> F2[LED Task - 优先级 2]
        D2 --> G2[预留 CS1237 Task - 优先级 6]
        note2[优点: 抢占式调度, 高实时性, 任务隔离]
    end
```

---

## 二、 FreeRTOS 内核移植核心步骤

### 2.1 文件目录规划
在工程根目录下建立了独立的 `FreeRTOS` 源码框架：
* `FreeRTOS/include/`: [FreeRTOS.h](file:///d:/code/testController/testController/test_v2/FreeRTOS/include/FreeRTOS.h), [task.h](file:///d:/code/testController/testController/test_v2/FreeRTOS/include/task.h), [queue.h](file:///d:/code/testController/testController/test_v2/FreeRTOS/include/queue.h), [semphr.h](file:///d:/code/testController/testController/test_v2/FreeRTOS/include/semphr.h), [timers.h](file:///d:/code/testController/testController/test_v2/FreeRTOS/include/timers.h), [event_groups.h](file:///d:/code/testController/testController/test_v2/FreeRTOS/include/event_groups.h), [stream_buffer.h](file:///d:/code/testController/testController/test_v2/FreeRTOS/include/stream_buffer.h)
* `FreeRTOS/source/`: [tasks.c](file:///d:/code/testController/testController/test_v2/FreeRTOS/source/tasks.c), [queue.c](file:///d:/code/testController/testController/test_v2/FreeRTOS/source/queue.c), [list.c](file:///d:/code/testController/testController/test_v2/FreeRTOS/source/list.c), [timers.c](file:///d:/code/testController/testController/test_v2/FreeRTOS/source/timers.c), [event_groups.c](file:///d:/code/testController/testController/test_v2/FreeRTOS/source/event_groups.c), [stream_buffer.c](file:///d:/code/testController/testController/test_v2/FreeRTOS/source/stream_buffer.c)
* `FreeRTOS/portable/MemMang/`: [heap_4.c](file:///d:/code/testController/testController/test_v2/FreeRTOS/portable/MemMang/heap_4.c) (内存堆管理)
* `FreeRTOS/portable/RVDS/ARM_CM4F/`: [port.c](file:///d:/code/testController/testController/test_v2/FreeRTOS/portable/RVDS/ARM_CM4F/port.c) 和 [portmacro.h](file:///d:/code/testController/testController/test_v2/FreeRTOS/portable/RVDS/ARM_CM4F/portmacro.h) (Cortex-M4F 移植层)

### 2.2 核心配置参数 ([FreeRTOSConfig.h](file:///d:/code/testController/testController/test_v2/User/FreeRTOSConfig.h))
```c
#define configUSE_PREEMPTION                    1                    // 开启抢占式调度
#define configCPU_CLOCK_HZ                      ( SystemCoreClock )  // 系统主频
#define configTICK_RATE_HZ                      ( ( TickType_t ) 1000 ) // OS 心跳 1ms
#define configMAX_PRIORITIES                    ( 32 )               // 优先级数量
#define configTOTAL_HEAP_SIZE                   ( ( size_t ) ( 32 * 1024 ) ) // 动态堆 32KB

/* 中断优先级配置 (GD32F470 为 4 位 NVIC 优先级) */
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY			15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY	5
```

### 2.3 中断与心跳适配 ([gd32f4xx_it.c](file:///d:/code/testController/testController/test_v2/User/gd32f4xx_it.c))
1. **中断映射**：`vPortSVCHandler` 映射为 `SVC_Handler`，`xPortPendSVHandler` 映射为 `PendSV_Handler`。在 [gd32f4xx_it.c](file:///d:/code/testController/testController/test_v2/User/gd32f4xx_it.c) 中移除原有的空函数定义。
2. **SysTick 心跳**：在 `SysTick_Handler` 中加入调度逻辑：
   ```c
   void SysTick_Handler(void) {
       delay_decrement(); // 维持裸机毫秒计数兼容
       if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
           vTaskIncrementTick(); // 触发 FreeRTOS 时间片推进
       }
   }
   ```

---

## 三、 开发过程踩坑与解决方案 (Troubleshooting)

| 序号 | 错误现象 / 提示 | 根因分析 (Root Cause) | 解决方案 (Solution) |
| :--- | :--- | :--- | :--- |
| **1** | `portable.h(37): error: #130: expected a "{"` 导致的连锁 50+ 编译报错 | 编译器解析 `portable.h` 时，`PRIVILEGED_FUNCTION` 宏定义尚未展开，被 Armcc 误诊为函数体开头。 | 在 [mpu_wrappers.h](file:///d:/code/testController/testController/test_v2/FreeRTOS/include/mpu_wrappers.h) 补充防护并在 [portable.h](file:///d:/code/testController/testController/test_v2/FreeRTOS/include/portable.h) 头部明确 `#include "mpu_wrappers.h"`。 |
| **2** | `port.c: error: A1541E: mc is not a valid condition code` | Cortex-M4F `PendSV` 汇编中 FPU 浮点寄存器入栈/出栈条件指令 `it mc` 语法拼写错误。 | 将 `it mc` 修正为 Armcc 认可的标准条件码 `it eq`。 |
| **3** | `heap_4.c: error: #20: identifier "portBYTE_ALIGNMENT_MASK" is undefined` | 8字节堆内存对齐掩码在移植头文件中缺失。 | 在 [portmacro.h](file:///d:/code/testController/testController/test_v2/FreeRTOS/portable/RVDS/ARM_CM4F/portmacro.h) 中补充定义 `#define portBYTE_ALIGNMENT_MASK (0x0007)`。 |
| **4** | `tasks.c: error: undefined symbol xNextTaskUnblockTime` | 手写简化版 `tasks.c` 缺失部分全局控制变量。 | 补充定义 `xNextTaskUnblockTime` 及 `uxSchedulerSuspended` 静态全局控制变量。 |

---

## 四、 应用任务设计实战 ([main.c](file:///d:/code/testController/testController/test_v2/User/main.c))

在 [main.c](file:///d:/code/testController/testController/test_v2/User/main.c) 中完成了业务逻辑的任务化重构：

```c
/* 网络轮询与 link 检测任务 */
static void app_task_network(void *pvParameters) {
    for (;;) {
        lwip_demo_poll(); // 轮询 LwIP 报文
        // 周期性 link 状态打印...
        vTaskDelay(pdMS_TO_TICKS(10)); // 释放 CPU
    }
}

/* 心跳 LED 闪烁任务 */
static void app_task_led(void *pvParameters) {
    for (;;) {
        gpio_bit_toggle(GPIOD, GPIO_PIN_7);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

int main(void) {
    systick_config();
    usart_debug_init(115200);
    bsp_lan8702_init();
    lwip_demo_init();

    /* 创建任务 */
    xTaskCreate(app_task_network, "NetworkTask", 1024, NULL, 5, NULL);
    xTaskCreate(app_task_led,     "LEDTask",      256,  NULL, 2, NULL);

    /* 启动调度器 */
    vTaskStartScheduler();
    while(1);
}
```

---

## 五、 后续集成 CS1237 外设开发架构路线图

在已搭建好的 FreeRTOS 框架下，后续集成 CS1237 24-bit ADC 的标准实践方案如下：

1. **硬件引脚配置**：
   * `CS1237_SCLK`：GPIO 输出
   * `CS1237_DOUT`：GPIO 双向 (带 EXTI 下降沿中断功能，代表 DRDY 采样数据准备就绪)
2. **专属采样任务设计 (`app_task_cs1237`)**：
   * 设置为高优先级（如优先级 6），保证模数转换读取的时效性。
   * DOUT 下降沿中断中调用 `vTaskNotifyGiveFromISR()` 唤醒 `app_task_cs1237` 任务。
   * 任务接收到通知后，调用 24 脉冲时序读取数据，并进行数字滤波（如滑动平均/中值滤波）。
3. **跨任务数据通信**：
   * 创建 FreeRTOS 消息队列 `xQueueCS1237Data`。
   * `app_task_cs1237` 将过滤后的物理量写入队列，`app_task_network` 从队列读取并打包通过 TCP Server 发送给上位机。

---

## 六、 总结与项目收益

1. **高实时性**：CS1237 等高精度 ADC 外设可以通过高优先级任务及中断通知第一时间响应，不再受网络包阻塞影响。
2. **高稳定性**：LwIP 网络栈与业务逻辑相互隔离，单个任务的非阻塞挂起（`vTaskDelay`）大幅降低系统功耗与 CPU 占用率。
3. **高扩展性**：后续新增传感器、Modbus 协议或控制算法仅需增加新的 Task 模块，具备出色的工程可扩展性与可维护性。
