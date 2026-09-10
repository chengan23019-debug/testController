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
#include "tcp_client_app.h"

/* 默认静态回退 IP 地址配置 (在直连电脑无 DHCP 或 DHCP 超时时立即启用) */
#define BOARD_IP_ADDR0   192
#define BOARD_IP_ADDR1   168
#define BOARD_IP_ADDR2   31
#define BOARD_IP_ADDR3   200

#define BOARD_NETMASK0   255
#define BOARD_NETMASK1   255
#define BOARD_NETMASK2   255
#define BOARD_NETMASK3   0

#define BOARD_GW_ADDR0   192
#define BOARD_GW_ADDR1   168
#define BOARD_GW_ADDR2   31
#define BOARD_GW_ADDR3   1

void lwip_demo_init(void);
void lwip_demo_poll(void);
void lwip_demo_apply_static_ip(void);

#endif /* __LWIP_DEMO_H */
