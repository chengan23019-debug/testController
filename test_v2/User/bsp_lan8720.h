/*!
    \file    bsp_lan8720.h
    \brief   Header file for LAN8720A / LAN8702 Ethernet PHY Driver on GD32F470
*/

#ifndef __BSP_LAN8720_H
#define __BSP_LAN8720_H

#include "gd32f4xx.h"
#include "gd32f4xx_enet.h"
#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* LAN8720 / LAN8702 PHY Register Addresses */
#define PHY_BCR                         0x00U   /*!< Basic Control Register */
#define PHY_BSR                         0x01U   /*!< Basic Status Register */
#define PHY_ID1                         0x02U   /*!< PHY Identifier 1 */
#define PHY_ID2                         0x03U   /*!< PHY Identifier 2 */
#define PHY_ANAR                        0x04U   /*!< Auto-Negotiation Advertisement Register */
#define PHY_ANLPAR                      0x05U   /*!< Auto-Negotiation Link Partner Ability Register */
#define PHY_SCSR                        0x1FU   /*!< Special Control/Status Register (LAN8720/LAN8702) */

/* Basic Control Register (BCR) Bits */
#define PHY_BCR_RESET                   ((uint16_t)0x8000U)
#define PHY_BCR_LOOPBACK                ((uint16_t)0x4000U)
#define PHY_BCR_SPEED_SELECT            ((uint16_t)0x2000U)
#define PHY_BCR_AUTONEG_ENABLE          ((uint16_t)0x1000U)
#define PHY_BCR_POWER_DOWN              ((uint16_t)0x0800U)
#define PHY_BCR_ISOLATE                 ((uint16_t)0x0400U)
#define PHY_BCR_RESTART_AUTONEG         ((uint16_t)0x0200U)
#define PHY_BCR_DUPLEX_MODE             ((uint16_t)0x0100U)

/* Basic Status Register (BSR) Bits */
#define PHY_BSR_100BASE_T4              ((uint16_t)0x8000U)
#define PHY_BSR_100BASE_TX_FD           ((uint16_t)0x4000U)
#define PHY_BSR_100BASE_TX_HD           ((uint16_t)0x2000U)
#define PHY_BSR_10BASE_T_FD             ((uint16_t)0x1000U)
#define PHY_BSR_10BASE_T_HD             ((uint16_t)0x0800U)
#define PHY_BSR_AUTONEG_COMPLETE        ((uint16_t)0x0020U)
#define PHY_BSR_LINK_STATUS             ((uint16_t)0x0004U)

/* LAN8720/LAN8702 Special Control/Status Register (SCSR - 0x1F) Mask & Speed Definitions */
#define PHY_SCSR_SPEED_MASK             ((uint16_t)0x001CU)
#define PHY_SCSR_10HD                   ((uint16_t)0x0004U)  /* 10BASE-T Half Duplex */
#define PHY_SCSR_10FD                   ((uint16_t)0x0014U)  /* 10BASE-T Full Duplex */
#define PHY_SCSR_100HD                  ((uint16_t)0x0008U)  /* 100BASE-TX Half Duplex */
#define PHY_SCSR_100FD                  ((uint16_t)0x0018U)  /* 100BASE-TX Full Duplex */

/* Default LAN8720/LAN8702 PHY Hardware Address */
#define LAN8720_DEFAULT_PHY_ADDR        0x00U

/* Driver Return Status Code */
#define LAN8720_OK                      0
#define LAN8720_ERROR                   (-1)

/* Function Prototypes */
int bsp_lan8720_init(void);
uint8_t bsp_lan8720_get_link_status(void);
void bsp_lan8720_print_phy_regs(void);
int bsp_lan8720_send_packet(uint8_t *p_buf, uint16_t len);
uint16_t bsp_lan8720_receive_packet(uint8_t *p_buf, uint16_t max_len);
void bsp_lan8720_get_mac(uint8_t mac[6]);

uint16_t bsp_lan8720_read_phy_reg(uint16_t phy_addr, uint16_t reg_addr);
void bsp_lan8720_write_phy_reg(uint16_t phy_addr, uint16_t reg_addr, uint16_t value);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_LAN8720_H */
