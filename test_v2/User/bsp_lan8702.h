/*!
    \file    bsp_lan8702.h
    \brief   LAN8702 Ethernet PHY Driver Header Wrapper for GD32F470
*/

#ifndef __BSP_LAN8702_H
#define __BSP_LAN8702_H

#include "bsp_lan8720.h"

#ifdef __cplusplus
extern "C" {
#endif

/* LAN8702 Alias Macro & Function Prototypes */
#define LAN8702_OK                      LAN8720_OK
#define LAN8702_ERROR                   LAN8720_ERROR

#define bsp_lan8702_init                bsp_lan8720_init
#define bsp_lan8702_get_link_status     bsp_lan8720_get_link_status
#define bsp_lan8702_print_phy_regs      bsp_lan8720_print_phy_regs
#define bsp_lan8702_send_packet         bsp_lan8720_send_packet
#define bsp_lan8702_receive_packet      bsp_lan8720_receive_packet
#define bsp_lan8702_get_mac             bsp_lan8720_get_mac

#ifdef __cplusplus
}
#endif

#endif /* __BSP_LAN8702_H */
