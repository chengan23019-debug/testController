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
#include "lwip_demo.h"

/*!
    \brief    main function
    \param[in]  none
    \param[out] none
    \retval     none
*/
int main(void)
{
    extern struct netif g_netif;
    uint32_t last_led_ticks = 0;
    uint32_t last_link_ticks = 0;
    uint8_t link_status = 0;

    systick_config();
    usart_debug_init(115200);

    printf("\r\n=== GD32F470 System Initialization ===\r\n");

    /* Initialize LAN8702/LAN8720 Ethernet Driver */
    if (LAN8702_OK == bsp_lan8702_init()) {
        printf("LAN8702 Ethernet Module Driver Ready!\r\n");
    } else {
        printf("LAN8702 Ethernet Module Driver Initialization Failed!\r\n");
    }

    /* Initialize LwIP TCP Stack & TCP Server Demo */
    lwip_demo_init();

    rcu_periph_clock_enable(RCU_GPIOD);
    gpio_mode_set(GPIOD, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_7); // PD7-LED2
    gpio_output_options_set(GPIOD, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_7);

    while(1) {
        /* Non-blocking LwIP network packet & timer polling */
        lwip_demo_poll();

        /* Non-blocking LED toggle every 500ms */
        if ((sys_now() - last_led_ticks) >= 500U) {
            last_led_ticks = sys_now();
            gpio_bit_toggle(GPIOD, GPIO_PIN_7);
        }

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
    }
}

