/*!
    \file    main.c
    \brief   LAN8702 Ethernet & System Main Entry
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
#include "bsp_sdram.h"
#include "lwip_demo.h"
#include "app_dyno_global.h"
#include "app_dyno_control.h"
#include "app_protocol_binary.h"
#include "app_protocol_modbus.h"

#include "FreeRTOS.h"
#include "task.h"

static void app_task_network(void *pvParameters)
{
    extern struct netif g_netif;
    uint32_t last_link_ticks = 0;
    uint32_t last_telemetry_ticks = 0;
    uint8_t link_status = 0;

    (void)pvParameters;

    for (;;) {
        /* 1. Non-blocking LwIP network packet, timers & client auto-reconnect polling */
        lwip_demo_poll();

        /* 2. Push 56-byte binary telemetry packet at 50Hz (every 20ms) to Center Server (Nagle disabled) */
        if ((sys_now() - last_telemetry_ticks) >= 20U) {
            last_telemetry_ticks = sys_now();
            tcp_client_send_telemetry();
        }

        /* 3. Non-blocking Ethernet Link & TCP Client status check every 3000ms */
        if ((sys_now() - last_link_ticks) >= 3000U) {
            last_link_ticks = sys_now();
            link_status = bsp_lan8702_get_link_status();
            printf("[NetTask] Link: %s | TCP Client: %s | Local IP: %d.%d.%d.%d\r\n", 
                   link_status ? "UP" : "DOWN",
                   tcp_client_get_state_str(),
                   (uint8_t)(g_netif.ip_addr.addr),
                   (uint8_t)(g_netif.ip_addr.addr >> 8),
                   (uint8_t)(g_netif.ip_addr.addr >> 16),
                   (uint8_t)(g_netif.ip_addr.addr >> 24));
        }

        /* 4. Adaptive FreeRTOS Task Scheduling (Optimization 5) */
        if (tcp_client_is_burst_mode()) {
            /* State A: SDRAM high-speed burst downloading -> minimal delay for maximum throughput */
            vTaskDelay(pdMS_TO_TICKS(1));
        } else {
            /* State B: Standard 50Hz telemetry -> 5ms delay to yield CPU for PID control & sampling */
            vTaskDelay(pdMS_TO_TICKS(5));
        }
    }
}

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

static void app_task_cs1237(void *pvParameters)
{
    uint8_t success = 0;
    int32_t adc_val = 0;
    uint8_t reg_val = 0;
    cs1237_pga_t active_pga = CS1237_PGA_128X;
    float voltage_mv = 0.0f;

    (void)pvParameters;

    /* Print CS1237 configuration register status */
    reg_val = cs1237_read_reg();
    printf("CS1237 Register Read Code: 0x%02X\r\n", reg_val);

    for (;;) {
        /* CS1237: 测量扭矩 (Load Cell) */
        adc_val = cs1237_read_adc_auto_range(&success, &active_pga);
        if (success) {
            voltage_mv = cs1237_raw_to_voltage_mv(adc_val, active_pga, 3.3f);
            
            /* 同步更新扭矩测量值到全局数据中心 */
            app_dyno_update_torque(voltage_mv);
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

static void app_task_hlw8112(void *pvParameters)
{
    hlw8112_data_t hlw_data;

    (void)pvParameters;

    printf("\r\n=== HLW8112 AC Energy Metering Task Started ===\r\n");

    for (;;) {
        /* HLW8112: 测量交流电参量 (电压、电流、功率、功率因数) */
        hlw8112_read_data(&hlw_data);
        if (hlw_data.calib_ok) {
            app_dyno_update_hlw8112(hlw_data.voltage, 
                                    hlw_data.current_a, 
                                    hlw_data.active_power_a, 
                                    hlw_data.power_factor);
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

static void app_task_cs1238(void *pvParameters)
{
    cs1238_data_t cs1238_data;
    uint8_t reg_val = 0;

    (void)pvParameters;

    /* Print CS1238 configuration register status */
    reg_val = cs1238_read_reg();
    printf("CS1238 Register Read Code: 0x%02X\r\n", reg_val);

    for (;;) {
        /* CS1238: 测量直流电参量 (CH1: 直流电压, CH2: 直流电流) */
        cs1238_read_all_channels(&cs1238_data, 3.3f);
        if (cs1238_data.success) {
            /* 同步更新直流电压 (CH1) 与直流电流 (CH2) 到全局数据中心 */
            app_dyno_update_dc(cs1238_data.volt_ch1_mv, cs1238_data.volt_ch2_mv);
        }

        vTaskDelay(pdMS_TO_TICKS(100));
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

    printf("\r\n=== GD32F470 System Initialization (FreeRTOS Architecture) ===\r\n");

    /* Initialize On-Board W9825G6KH-6I SDRAM (32MB) via EXMC */
    bsp_sdram_init();
    bsp_sdram_test();

    /* Initialize CS1237 24-bit ADC Peripheral */
    cs1237_init();
    printf("CS1237 24-bit ADC Peripheral Initialized (CLK: PF6, DOUT: PF7)\r\n");

    /* Initialize CS1238 Dual-Channel 24-bit ADC Peripheral */
    cs1238_init();
    printf("CS1238 Dual-CH 24-bit ADC Peripheral Initialized (CLK: PF8, DOUT: PF9)\r\n");

    /* Initialize HLW8112 AC Metering Peripheral */
    hlw8112_init();
    printf("HLW8112 AC Metering Peripheral Initialized (CS: PB5, SCLK: PB6, SDI: PB7, SDO: PB8, EN: PB9)\r\n");

    /* Initialize LAN8702/LAN8720 Ethernet Driver */
    if (LAN8702_OK == bsp_lan8702_init()) {
        printf("LAN8702 Ethernet Module Driver Ready!\r\n");
    } else {
        printf("LAN8702 Ethernet Module Driver Initialization Failed!\r\n");
    }

    /* Initialize LwIP TCP Stack & TCP Server Demo */
    lwip_demo_init();

    /* Initialize Dynamometer Global Data Center */
    app_dyno_global_init();
    printf("Dynamometer Global Data Center Initialized!\r\n");

    printf("Creating FreeRTOS Application Tasks...\r\n");

    /* Create Dynamometer Load Control Task (Priority 7, Stack 1024) */
    xTaskCreate(app_task_dyno_control, "DynoCtrlTask", 1024, NULL, 7, NULL);

    /* Create Network Task */
    xTaskCreate(app_task_network, "NetworkTask", 1024, NULL, 5, NULL);

    /* Create LED Toggle Task */
    xTaskCreate(app_task_led, "LEDTask", 256, NULL, 2, NULL);

    /* Create CS1237 ADC Sampling Task (Priority 5, Stack 1024) */
    xTaskCreate(app_task_cs1237, "CS1237Task", 1024, NULL, 5, NULL);

    /* Create CS1238 Dual-Channel ADC Task (Priority 5, Stack 1024) */
    xTaskCreate(app_task_cs1238, "CS1238Task", 1024, NULL, 5, NULL);

    /* Create HLW8112 Metering Task (Priority 5, Stack 1024) */
    xTaskCreate(app_task_hlw8112, "HLW8112Task", 1024, NULL, 5, NULL);

    printf("Starting FreeRTOS Scheduler...\r\n");

    /* Start Scheduler */
    vTaskStartScheduler();

    /* Infinite loop fallback if scheduler fails */
    while(1) {
    }
}


