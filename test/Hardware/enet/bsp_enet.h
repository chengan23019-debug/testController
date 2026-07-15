#ifndef BSP_ENET_H
#define BSP_ENET_H

#include "gd32f4xx.h"

/* Reset pin definition for YT8512C: PD1 */
#define ENET_RST_RCU      RCU_GPIOD
#define ENET_RST_PORT     GPIOD
#define ENET_RST_PIN      GPIO_PIN_1

/* Function Declarations */
void bsp_enet_init(void);
uint16_t bsp_enet_phy_bsr_read(void);
uint16_t bsp_enet_phy_bcr_read(void);
uint16_t bsp_enet_phy_reg_read(uint16_t reg_addr);

#endif /* BSP_ENET_H */
