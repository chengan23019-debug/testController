/*!
    \file    lwip_demo.h
    \brief   LwIP Stack Initialization and Main Loop Driver Header
*/

#ifndef __LWIP_DEMO_H
#define __LWIP_DEMO_H

#include "lwip/init.h"
#include "lwip/netif.h"
#include "lwip/tcp.h"
#include "lwip/dhcp.h"
#include "netif/etharp.h"
#include "ethernetif.h"
#include "tcp_server_demo.h"

/* Initial Fallback Static IP Configuration (if DHCP fails or before DHCP bound) */
#define BOARD_IP_ADDR0   0
#define BOARD_IP_ADDR1   0
#define BOARD_IP_ADDR2   0
#define BOARD_IP_ADDR3   0

#define BOARD_NETMASK0   0
#define BOARD_NETMASK1   0
#define BOARD_NETMASK2   0
#define BOARD_NETMASK3   0

#define BOARD_GW_ADDR0   0
#define BOARD_GW_ADDR1   0
#define BOARD_GW_ADDR2   0
#define BOARD_GW_ADDR3   0

void lwip_demo_init(void);
void lwip_demo_poll(void);

#endif /* __LWIP_DEMO_H */
