/*!
    \file    lwip_demo.c
    \brief   LwIP Initialization, Fast DHCP, Periodic Timers, and Main Loop Integration
*/

#include "lwip_demo.h"
#include "systick.h"
#include "bsp_lan8702.h"
#include <stdio.h>

struct netif g_netif;

static uint32_t sg_tcp_timer = 0;
static uint32_t sg_arp_timer = 0;
static uint32_t sg_dhcp_fine_timer = 0;
static uint32_t sg_dhcp_coarse_timer = 0;
static uint32_t sg_dhcp_start_tick = 0;
static uint8_t  sg_dhcp_status = 0;
static uint8_t  sg_last_link_state = 0;
static uint8_t  sg_static_fallback_done = 0;

/**
 * @brief  配置并应用默认静态 IP (当无 DHCP 或超时使用)
 */
void lwip_demo_apply_static_ip(void)
{
    struct ip_addr ipaddr;
    struct ip_addr netmask;
    struct ip_addr gw;

    IP4_ADDR(&ipaddr, BOARD_IP_ADDR0, BOARD_IP_ADDR1, BOARD_IP_ADDR2, BOARD_IP_ADDR3);
    IP4_ADDR(&netmask, BOARD_NETMASK0, BOARD_NETMASK1, BOARD_NETMASK2, BOARD_NETMASK3);
    IP4_ADDR(&gw, BOARD_GW_ADDR0, BOARD_GW_ADDR1, BOARD_GW_ADDR2, BOARD_GW_ADDR3);

    netif_set_addr(&g_netif, &ipaddr, &netmask, &gw);
    
    printf("\r\n==============================================");
    printf("\r\n[Static IP Active] IP Address : %d.%d.%d.%d",
           BOARD_IP_ADDR0, BOARD_IP_ADDR1, BOARD_IP_ADDR2, BOARD_IP_ADDR3);
    printf("\r\n[Static IP Active] Subnet Mask : %d.%d.%d.%d",
           BOARD_NETMASK0, BOARD_NETMASK1, BOARD_NETMASK2, BOARD_NETMASK3);
    printf("\r\n[Static IP Active] Gateway     : %d.%d.%d.%d",
           BOARD_GW_ADDR0, BOARD_GW_ADDR1, BOARD_GW_ADDR2, BOARD_GW_ADDR3);
    printf("\r\n==============================================\r\n\r\n");
}

void lwip_demo_init(void)
{
    struct ip_addr ipaddr;
    struct ip_addr netmask;
    struct ip_addr gw;

    printf("\r\n=== LwIP TCP Stack Initializing (High Speed) ===\r\n");

    /* Initialize LwIP core stack */
    lwip_init();

    /* Configure IP parameters (Initial 0.0.0.0 for fast DHCP discovery) */
    IP4_ADDR(&ipaddr, 0, 0, 0, 0);
    IP4_ADDR(&netmask, 0, 0, 0, 0);
    IP4_ADDR(&gw, 0, 0, 0, 0);

    /* Add network interface */
    netif_add(&g_netif, &ipaddr, &netmask, &gw, NULL, &ethernetif_init, &ethernet_input);

    /* Set as default network interface and bring up link */
    netif_set_default(&g_netif);
    netif_set_up(&g_netif);

    /* Start DHCP Auto IP Acquisition */
    dhcp_start(&g_netif);
    sg_dhcp_start_tick = sys_now();
    sg_dhcp_status = 0;
    sg_static_fallback_done = 0;
    sg_last_link_state = bsp_lan8702_get_link_status();

    printf("[DHCP] Fast DHCP request started (Timeout: 4000ms -> Static fallback)...\r\n");

    /* Initialize TCP Client Application */
    tcp_client_app_init();
}

void lwip_demo_poll(void)
{
    uint32_t now = sys_now();
    uint8_t current_link;

    /* 1. Poll incoming Ethernet frames until queue is empty */
    while (ethernetif_input(&g_netif) == ERR_OK) {
    }

    /* 2. Check Physical Link State changes (Auto-restart DHCP on fresh plug-in) */
    current_link = bsp_lan8702_get_link_status();
    if (current_link != sg_last_link_state) {
        sg_last_link_state = current_link;
        if (current_link) {
            printf("[NetTask] Ethernet Cable Plugged in! Restarting Fast DHCP...\r\n");
            netif_set_link_up(&g_netif);
            dhcp_start(&g_netif);
            sg_dhcp_start_tick = now;
            sg_static_fallback_done = 0;
            sg_dhcp_status = 0;
        } else {
            printf("[NetTask] Ethernet Cable Unplugged!\r\n");
            netif_set_link_down(&g_netif);
        }
    }

    /* 3. Handle TCP Fast/Slow Timers (~250ms) */
    if ((now - sg_tcp_timer) >= 250U) {
        sg_tcp_timer = now;
        tcp_tmr();
    }

    /* 4. Handle ARP Timer (~5000ms) */
    if ((now - sg_arp_timer) >= 5000U) {
        sg_arp_timer = now;
        etharp_tmr();
    }

    /* 5. Handle DHCP Fine Timer (~500ms) */
    if ((now - sg_dhcp_fine_timer) >= DHCP_FINE_TIMER_MSECS) {
        sg_dhcp_fine_timer = now;
        dhcp_fine_tmr();
    }

    /* 6. Handle DHCP Coarse Timer (~60000ms) */
    if ((now - sg_dhcp_coarse_timer) >= (DHCP_COARSE_TIMER_SECS * 1000U)) {
        sg_dhcp_coarse_timer = now;
        dhcp_coarse_tmr();
    }

    /* 7. Check DHCP bound status and print IP when acquired */
    if ((g_netif.dhcp != NULL) && (g_netif.dhcp->state == DHCP_BOUND)) {
        if (sg_dhcp_status == 0) {
            sg_dhcp_status = 1;
            sg_static_fallback_done = 1;
            printf("\r\n==============================================");
            printf("\r\n[DHCP Success] IP Address : %d.%d.%d.%d",
                   (uint8_t)(g_netif.ip_addr.addr),
                   (uint8_t)(g_netif.ip_addr.addr >> 8),
                   (uint8_t)(g_netif.ip_addr.addr >> 16),
                   (uint8_t)(g_netif.ip_addr.addr >> 24));
            printf("\r\n[DHCP Success] Subnet Mask : %d.%d.%d.%d",
                   (uint8_t)(g_netif.netmask.addr),
                   (uint8_t)(g_netif.netmask.addr >> 8),
                   (uint8_t)(g_netif.netmask.addr >> 16),
                   (uint8_t)(g_netif.netmask.addr >> 24));
            printf("\r\n[DHCP Success] Gateway     : %d.%d.%d.%d",
                   (uint8_t)(g_netif.gw.addr),
                   (uint8_t)(g_netif.gw.addr >> 8),
                   (uint8_t)(g_netif.gw.addr >> 16),
                   (uint8_t)(g_netif.gw.addr >> 24));
            printf("\r\n==============================================\r\n\r\n");
        }
    } else {
        if (sg_dhcp_status == 1) {
            sg_dhcp_status = 0;
            printf("[DHCP] IP address released or lost!\r\n");
        }

        /* 8. DHCP 超时快速回退机制：超过 4 秒无响应 (如网线直连电脑无DHCP)，自动切换为静态 IP */
        if (!sg_static_fallback_done && (now - sg_dhcp_start_tick) >= 4000U) {
            sg_static_fallback_done = 1;
            printf("[DHCP] Timeout after 4000ms. Activating Static Fallback IP...\r\n");
            lwip_demo_apply_static_ip();
        }
    }

    /* 9. Poll TCP Client Connection & Auto-Reconnect State Machine */
    tcp_client_app_poll();
}
