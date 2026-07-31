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
#include "bsp_hlw8112.h"
#include "lwip_demo.h"

#include "FreeRTOS.h"
#include "task.h"

static void app_task_network(void *pvParameters)
{
    extern struct netif g_netif;
    uint32_t last_link_ticks = 0;
    uint8_t link_status = 0;

    (void)pvParameters;

    for (;;) {
        /* Non-blocking LwIP network packet & timer polling */
        lwip_demo_poll();

        /* Non-blocking Ethernet Link status check every 3000ms */
        if ((sys_now() - last_link_ticks) >= 3000U) {
            last_link_ticks = sys_now();
            link_status = bsp_lan8702_get_link_status();
            printf("GD32F470 Ethernet Link: %s | Current IP: %d.%d.%d.%d | TCP Port: 8080\r\n", 
                   link_status ? "LINK UP [Connected]" : "LINK DOWN [Disconnected]",
                   (uint8_t)(g_netif.ip_addr.addr),
                   (uint8_t)(g_netif.ip_addr.addr >> 8),
                   (uint8_t)(g_netif.ip_addr.addr >> 16),
                   (uint8_t)(g_netif.ip_addr.addr >> 24));
        }

        vTaskDelay(pdMS_TO_TICKS(10));
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
        /* Sample ADC with Auto-Range gain switching */
        adc_val = cs1237_read_adc_auto_range(&success, &active_pga);
        if (success) {
            voltage_mv = cs1237_raw_to_voltage_mv(adc_val, active_pga, 3.3f);
            printf("[CS1237 Auto-Range] Gain: %3dX | ADC Raw: %8d | Voltage: %9.4f mV (%7.2f uV)\r\n",
                   cs1237_get_pga_multiplier(active_pga),
                   (int)adc_val,
                   voltage_mv,
                   voltage_mv * 1000.0f);
        } else {
            printf("[CS1237] ADC Read Timeout / DRDY Not Ready\r\n");
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void app_task_hlw8112(void *pvParameters)
{
    hlw8112_data_t hlw_data;

    (void)pvParameters;

    printf("\r\n=== HLW8112 AC Energy Metering Task Started ===\r\n");

    for (;;) {
        hlw8112_read_data(&hlw_data);
        if (hlw_data.calib_ok) {
            printf("[HLW8112 Measure] Voltage: %6.2f V | Current: %7.3f A (%6.1f mA) | ActivePower: %7.2f W | PF: %5.3f | Freq: %5.2f Hz | Angle: %5.1f Deg | Energy: %.4f kWh\r\n",
                   hlw_data.voltage,
                   hlw_data.current_a,
                   hlw_data.current_a_ma,
                   hlw_data.active_power_a,
                   hlw_data.power_factor,
                   hlw_data.frequency,
                   hlw_data.phase_angle,
                   hlw_data.active_energy_a);
        } else {
            printf("[HLW8112 Status] Calibration Checksum Failed / Check SPI Wiring! (V: %6.2f V | I: %7.3f A)\r\n",
                   hlw_data.voltage,
                   hlw_data.current_a);
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
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

    /* Initialize CS1237 24-bit ADC Peripheral */
    cs1237_init();
    printf("CS1237 24-bit ADC Peripheral Initialized (CLK: PC3, DOUT: PC2)\r\n");

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

    printf("Creating FreeRTOS Application Tasks...\r\n");

    /* Create Network Task */
    xTaskCreate(app_task_network, "NetworkTask", 1024, NULL, 5, NULL);

    /* Create LED Toggle Task */
    xTaskCreate(app_task_led, "LEDTask", 256, NULL, 2, NULL);

    /* Create CS1237 ADC Sampling Task (Priority 5, Stack 1024) */
    xTaskCreate(app_task_cs1237, "CS1237Task", 1024, NULL, 5, NULL);

    /* Create HLW8112 Metering Task (Priority 5, Stack 1024) */
    xTaskCreate(app_task_hlw8112, "HLW8112Task", 1024, NULL, 5, NULL);

    printf("Starting FreeRTOS Scheduler...\r\n");

    /* Start Scheduler */
    vTaskStartScheduler();

    /* Infinite loop fallback if scheduler fails */
    while(1) {
    }
}


