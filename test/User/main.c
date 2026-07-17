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

/*!
    \brief    main function
    \param[in]  none
    \param[out] none
    \retval     none
*/
int main(void)
{
    /* 【修复项】：所有变量必须在函数最开头声明 (C89标准) */
    int addr;
    uint16_t phy_id = 0;

    systick_config();
    usart_debug_init(115200);
    
    printf("\r\n[System] GD32F470 Hardware Init Start...\r\n");

    /* Initialize LED Pin (PD7) */
    rcu_periph_clock_enable(RCU_GPIOD);
    gpio_mode_set(GPIOD, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_7);
    gpio_output_options_set(GPIOD, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_7);

    printf("[ETH] YT8512C PHY Init...\r\n");
    bsp_yt8512c_init(); 

#if 1
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
    app_cs1237_task_create();

    /* Create the HLW8112 Task */
    app_hlw8112_task_create();

    /* Start FreeRTOS scheduler */
    printf("[System] Starting FreeRTOS Scheduler...\r\n");
    vTaskStartScheduler();

    while(1) {
        /* Should not be reached */
    }
}

/* <-- 请确保你的光标能停在这单独的空行上 */

