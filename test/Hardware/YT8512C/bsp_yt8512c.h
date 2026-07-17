#ifndef __BSP_YT8512C_H
#define __BSP_YT8512C_H

#include "gd32f4xx.h"
#include "gd32f4xx_enet.h"

/* 必须确保 gd32f4xx_enet.h 中的 PHY_TYPE 定义为 YT8512 */
/* 可以在工程全局宏定义中添加，或者在 gd32f4xx_enet.h 中修改 */

/* PHY 硬件复位引脚定义 (根据分配方案使用 PA3) */
#define YT8512C_RST_RCU     RCU_GPIOA
#define YT8512C_RST_PORT    GPIOA
#define YT8512C_RST_PIN     GPIO_PIN_3

/* 函数声明 */
void bsp_yt8512c_init(void);
ErrStatus bsp_yt8512c_link_check(void);

#endif /* __BSP_YT8512C_H */