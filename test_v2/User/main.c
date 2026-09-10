/*!
    \file    main.c
    \brief   Dyno Control System Main Entry with 50Hz Synchronous Sampling & Telemetry
    \version 2026-02-05, V3.3.3, firmware for GD32F4xx
*/

/*
    Copyright (c) 2026, GigaDevice Semiconductor Inc.

    Redistribution and use in source and binary forms, with or without modification,
    are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright notice, this
       list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright notice,
       this list of conditions and the following disclaimer in the documentation
       and/or other materials provided with the distribution.
    3. Neither the name of the copyright holder nor the names of its contributors
       may be used to endorse or promote products derived from this software without
       specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
    IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
    INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
    NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
    PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
    WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
    ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
    OF SUCH DAMAGE.
*/

#include "gd32f4xx.h"
#include "systick.h"
#include <stdio.h>
#include "main.h"
#include "gd32f4xx_rcu.h"
#include "usart.h"
#include "bsp_dac.h"
#include "bsp_lan8702.h"
#include "bsp_cs1237.h"
#include "bsp_cs1238.h"
#include "bsp_hlw8112.h"
#include "bsp_timer.h"
#include "bsp_sdram.h"
#include "lwip_demo.h"
#include "app_dyno_global.h"
#include "app_dyno_control.h"
#include "app_protocol_binary.h"
#include "app_protocol_modbus.h"

#include "FreeRTOS.h"
#include "task.h"

/**
 * @brief  50Hz (20ms) 核心同步采集、数字滤波与遥测推流主任务
 */
static void app_task_telemetry_50hz(void *pvParameters)
{
    float torque_nm = 0.0f;
    float speed_rpm = 0.0f;
    float dc_v = 0.0f;
    float dc_i = 0.0f;
    hlw8112_data_t ac_data;

    (void)pvParameters;

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(20));

        /* 1. 扭矩：提取 CS1237 640Hz 环形缓冲 20ms 均值并转换为物理扭矩 */
        torque_nm = cs1237_get_filtered_torque_nm(DYN_TORQUE_COEFF);

        /* 2. 直流电参：读取 CS1238 并执行一阶 IIR 低通滤波 (alpha = 0.35) */
        cs1238_read_dc_filtered(&dc_v, &dc_i);

        /* 3. 交流电参：50Hz 单次微秒级快速 SPI 读取 + Sanity Guard 坏帧拦截 */
        hlw8112_read_data(&ac_data);

        /* 4. 转速：读取定时器 M/T 硬件捕获结果 (零附加相位延迟) */
        speed_rpm = bsp_timer_get_speed_rpm();

        /* 5. 组装全传感器同步快照，原子更新全局数据中心 */
        app_dyno_update_telemetry(torque_nm, speed_rpm, ac_data.voltage, ac_data.current_a, ac_data.active_power_a, ac_data.power_factor);
        app_dyno_update_dc(dc_v, dc_i);
    }
}

/**
 * @brief  以太网协议栈维护与自适应调度任务 (高优先级确保网络与DHCP畅通)
 */
static void app_task_network(void *pvParameters)
{
    extern struct netif g_netif;
    uint32_t last_link_ticks = 0;
    uint32_t last_telemetry_ticks = 0;
    uint8_t link_status = 0;

    (void)pvParameters;

    for (;;) {
        /* 1. 非阻塞 LwIP 网络报文收发、内部定时器与客户端自动重连轮询 */
        lwip_demo_poll();

        /* 2. 50Hz (每 20ms) 遥测数据主动推流：在网络任务中统一调用，确保 LwIP 线程安全与零抖动 */
        if ((sys_now() - last_telemetry_ticks) >= 20U) {
            last_telemetry_ticks = sys_now();
            if (tcp_client_is_connected()) {
                tcp_client_send_telemetry();
            }
        }

        /* 3. 非阻塞网络链路状态与 TCP Client 状态定时检测 (每 3000ms) */
        if ((sys_now() - last_link_ticks) >= 3000U) {
            last_link_ticks = sys_now();
            link_status = bsp_lan8702_get_link_status();
            printf("[NetTask] Link: %s | TCP Client: %s | Local IP: %d.%d.%d.%d | Sent: %u pkts\r\n", 
                   link_status ? "UP" : "DOWN",
                   tcp_client_get_state_str(),
                   (uint8_t)(g_netif.ip_addr.addr),
                   (uint8_t)(g_netif.ip_addr.addr >> 8),
                   (uint8_t)(g_netif.ip_addr.addr >> 16),
                   (uint8_t)(g_netif.ip_addr.addr >> 24),
                   tcp_client_get_sent_count());
        }

        /* 4. 自适应 FreeRTOS 任务调度 */
        if (tcp_client_is_burst_mode()) {
            /* SDRAM 高速突发回放模式：最小延时以最大化吞吐带宽 */
            vTaskDelay(pdMS_TO_TICKS(1));
        } else {
            /* 标准运行模式：2ms 周期轮询，保证 50Hz (20ms) 遥测推流准时触发并让渡 CPU */
            vTaskDelay(pdMS_TO_TICKS(2));
        }
    }
}

/**
 * @brief  系统运行心跳指示任务
 */
static void app_task_led(void *pvParameters)
{
    (void)pvParameters;

    rcu_periph_clock_enable(RCU_GPIOD);
    gpio_mode_set(GPIOD, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_7); // PD7-LED2
    gpio_output_options_set(GPIOD, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_7);

    for (;;) {
        gpio_bit_toggle(GPIOD, GPIO_PIN_7);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/*!
    \brief    main function
    \param[in]  none
    \param[out] none
    \retval     none
*/
int main(void)
{
    systick_config();
    usart_debug_init(115200);

    printf("\r\n=== GD32F470 Dynamometer Control System Initializing ===\r\n");

    /* 1. Initialize On-Board W9825G6KH-6I SDRAM (32MB) via EXMC */
    bsp_sdram_init();
    bsp_sdram_test();

    /* 2. Initialize CS1237 24-bit ADC (Torque, 640Hz + EXTI7) */
    cs1237_init();
    printf("CS1237 Torque ADC Initialized (CLK: PF6, DOUT: PF7, SPEED: 640Hz, EXTI7 Enabled)\r\n");

    /* 3. Initialize CS1238 Dual-Channel 24-bit ADC (DC V/I, 640Hz) */
    cs1238_init();
    printf("CS1238 DC ADC Initialized (CLK: PF8, DOUT: PF9, SPEED: 640Hz, Isolated Clock)\r\n");

    /* 4. Initialize HLW8112 AC Metering Peripheral (Fast SPI 1us) */
    hlw8112_init();
    printf("HLW8112 AC Metering Initialized (CS: PB5, SCLK: PB6, SDI: PB7, SDO: PB8, EN: PB9)\r\n");

    /* 5. Initialize Speed Measurement Hardware Timer (TIMER1 / PA0) */
    bsp_timer_init();
    printf("Speed Timer Input Capture Initialized (PA0, TIMER1_CH0, HW Digital Filter Enabled)\r\n");

    /* 6. Initialize LAN8702/LAN8720 Ethernet Driver */
    if (LAN8702_OK == bsp_lan8702_init()) {
        printf("LAN8702 Ethernet Module Driver Ready!\r\n");
    } else {
        printf("LAN8702 Ethernet Module Driver Initialization Failed!\r\n");
    }

    /* 7. Initialize LwIP TCP Stack & Fast DHCP */
    lwip_demo_init();

    /* 8. Initialize Dynamometer Global Data Center */
    app_dyno_global_init();
    printf("Dynamometer Global Data Center Initialized!\r\n");

    printf("Creating FreeRTOS Application Tasks...\r\n");

    /* Create Network Task (Priority 6, Stack 1024) */
    xTaskCreate(app_task_network, "NetworkTask", 1024, NULL, 6, NULL);

    /* Create 50Hz (20ms) Unified Sampling & Telemetry Task (Priority 5, Stack 1024) */
    xTaskCreate(app_task_telemetry_50hz, "Tele50HzTask", 1024, NULL, 5, NULL);

    /* Create Dynamometer Load PID Control Task (Priority 5, Stack 1024) */
    xTaskCreate(app_task_dyno_control, "DynoCtrlTask", 1024, NULL, 5, NULL);

    /* Create LED Heartbeat Task (Priority 2, Stack 256) */
    xTaskCreate(app_task_led, "LEDTask", 256, NULL, 2, NULL);

    printf("Starting FreeRTOS Scheduler...\r\n");

    /* Start Scheduler */
    vTaskStartScheduler();

    /* Infinite loop fallback if scheduler fails */
    while(1) {
    }
}
