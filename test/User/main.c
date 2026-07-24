// /*!
//     \file    main.c
//     \brief   led spark with systick

//     \version 2026-02-05, V3.3.3, firmware for GD32F4xx
// */

// /*
//     Copyright (c) 2026, GigaDevice Semiconductor Inc.

//     Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:

//     1. Redistributions of source code must retain the above copyright notice, this
//        list of conditions and the following disclaimer.
//     2. Redistributions in binary form must reproduce the above copyright notice,
//        this list of conditions and the following disclaimer in the documentation
//        and/or other materials provided with the distribution.
//     3. Neither the name of the copyright holder nor the names of its contributors
//        may be used to endorse or promote products derived from this software without
//        specific prior written permission.

//     THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
// IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
// INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
// NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
// OF SUCH DAMAGE.
// */

// #include "gd32f4xx.h"
// #include "systick.h"
// #include <stdio.h>
// #include "main.h"
// #include "gd32f4xx_rcu.h"
// #include "usart.h"
// #include "FreeRTOS.h"
// #include "task.h"
// #include "app_cs1237.h"
// #include "app_hlw8112.h"

// /*!
//     \brief    main function
//     \param[in]  none
//     \param[out] none
//     \retval     none
// */
// int main(void)
// {
//     systick_config();
//     usart_debug_init(115200);
    
//     /* Initialize LED Pin (PD7) */
//     rcu_periph_clock_enable(RCU_GPIOD);
//     gpio_mode_set(GPIOD, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_7);
//     gpio_output_options_set(GPIOD, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_7);

//     /* Create the CS1237 Task */
//     app_cs1237_task_create();

//     /* Create the HLW8112 Task */
//     app_hlw8112_task_create();

//     /* Start FreeRTOS scheduler */
//     printf("Starting FreeRTOS Scheduler...\r\n");
//     vTaskStartScheduler();

//     while(1) {
//         /* Should not be reached */
//     }
// }


#include "gd32f4xx.h"
#include "systick.h"
#include <stdio.h>
#include "main.h"
#include "gd32f4xx_rcu.h"
#include "usart.h"
#include "FreeRTOS.h"
#include "task.h"
#include "app_cs1237.h"
#include "app_hlw8112.h"
#include "bsp_yt8512c.h" 

/* LwIP headers */
#include "lwip/opt.h"
#include "lwip/tcpip.h"
#include "lwip/netif.h"
#include "lwip/dhcp.h"
#include "ethernetif.h"

struct netif gnetif;

/* Under Keil MicroLib, errno must be defined by the user */
int errno;

/* ENET interrupt counter */
extern volatile uint32_t enet_rx_cnt;

#define LwIP_INIT_TASK_STACK_SIZE          (512)
#define LwIP_INIT_TASK_PRIO                (tskIDLE_PRIORITY + 3)

static void lwip_init_completion_callback(void *arg)
{
    ip4_addr_t ipaddr;
    ip4_addr_t netmask;
    ip4_addr_t gw;

    /* Initialize IP address, netmask, and gateway to 0.0.0.0 for DHCP */
    IP4_ADDR(&ipaddr, 0, 0, 0, 0);
    IP4_ADDR(&netmask, 0, 0, 0, 0);
    IP4_ADDR(&gw, 0, 0, 0, 0);

    /* Add netif interface */
    netif_add(&gnetif, &ipaddr, &netmask, &gw, NULL, &ethernetif_init, &tcpip_input);

    /* Set as default interface and bring it up */
    netif_set_default(&gnetif);
    netif_set_up(&gnetif);

    /* Print MAC address */
    printf("[LwIP] MAC Address: %02X:%02X:%02X:%02X:%02X:%02X\r\n",
           gnetif.hwaddr[0], gnetif.hwaddr[1], gnetif.hwaddr[2],
           gnetif.hwaddr[3], gnetif.hwaddr[4], gnetif.hwaddr[5]);

    /* Start DHCP client */
    printf("[LwIP] Starting DHCP client...\r\n");
    dhcp_start(&gnetif);
}
void lwip_init_task(void *pvParameters)
{
    printf("[LwIP] Initializing TCPIP Stack...\r\n");
    
    /* Initialize LwIP TCP/IP stack thread with callback */
    tcpip_init(lwip_init_completion_callback, NULL);

    while (1) {
        printf("[LwIP] Running... IP: %d.%d.%d.%d, ENET Rx IRQ Count: %d\r\n",
               ((gnetif.ip_addr.addr) & 0xFF),
               (((gnetif.ip_addr.addr) >> 8) & 0xFF),
               (((gnetif.ip_addr.addr) >> 16) & 0xFF),
               (((gnetif.ip_addr.addr) >> 24) & 0xFF),
               (int)enet_rx_cnt);
        
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
    /* 【修复项】：所有变量必须在函数最开头声明 (C89标准) */
#if 0
    int addr;
    uint16_t phy_id = 0;
#endif

    /* Configure NVIC priority grouping to 4 bits for preempt priority, 0 bits for sub-priority */
    nvic_priority_group_set(NVIC_PRIGROUP_PRE4_SUB0);

    systick_config();
    usart_debug_init(115200);
    
    printf("\r\n[System] GD32F470 Hardware Init Start...\r\n");

    /* Initialize LED Pin (PD7) */
    rcu_periph_clock_enable(RCU_GPIOD);
    gpio_mode_set(GPIOD, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_7);
    gpio_output_options_set(GPIOD, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_7);

    printf("[ETH] YT8512C PHY Init...\r\n");
    bsp_yt8512c_init(); 

#if 0
    printf("[ETH] Scanning PHY Addresses...\r\n");
    for (addr = 0; addr < 32; addr++) {
        ErrStatus status;
        phy_id = 0;
        /* 读取 PHY ID 2 寄存器 (寄存器地址 0x03)，因为 YT8512C 的 PHY ID 1 寄存器值为 0x0000 */
        status = enet_phy_write_read(ENET_PHY_READ, addr, 0x03, &phy_id); 
        
        if (SUCCESS == status) {
            if (phy_id != 0xFFFF && phy_id != 0x0000) {
                printf("-> Success! Found PHY at Address: %d, ID2: 0x%04X\r\n", addr, phy_id);
            }
        } else {
            printf("-> Addr %2d: SMI Read Timeout/Error!\r\n", addr);
        }
    }
    printf("Scan complete. System halted.\r\n");
		while(1);
#endif
    
    /* Create the CS1237 Task */
    // app_cs1237_task_create();

    /* Create the HLW8112 Task */
    // app_hlw8112_task_create();

    /* Create LwIP Initialization Task */
    xTaskCreate(lwip_init_task, "LwIP_Init", LwIP_INIT_TASK_STACK_SIZE, NULL, LwIP_INIT_TASK_PRIO, NULL);

    /* Start FreeRTOS scheduler */
    printf("[System] Starting FreeRTOS Scheduler...\r\n");
    vTaskStartScheduler();

    while(1) {
        /* Should not be reached */
    }
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    printf("\r\n!!! [StackOverflow] Task: %s has overflowed its stack !!!\r\n", pcTaskName);
    while(1);
}

void vApplicationMallocFailedHook(void)
{
    printf("\r\n!!! [MallocFailed] Out of FreeRTOS heap memory !!!\r\n");
    while(1);
}


/* <-- 请确保你的光标能停在这单独的空行上 */

