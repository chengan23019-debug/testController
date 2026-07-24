/*!
    \file    lwip_demo.c
    \brief   LwIP Initialization, DHCP, Periodic Timers, and Main Loop Integration
*/

#include "lwip_demo.h"
#include "systick.h"
#include <stdio.h>

struct netif g_netif;

static uint32_t sg_tcp_timer = 0;
static uint32_t sg_arp_timer = 0;
static uint32_t sg_dhcp_fine_timer = 0;
static uint32_t sg_dhcp_coarse_timer = 0;
static uint8_t  sg_dhcp_status = 0;

void lwip_demo_init(void)
{
    struct ip_addr ipaddr;
    struct ip_addr netmask;
    struct ip_addr gw;

    printf("\r\n=== LwIP TCP Stack Initializing ===\r\n");

    /* Initialize LwIP core stack */
    lwip_init();

    /* Configure IP parameters (0.0.0.0 for DHCP) */
    IP4_ADDR(&ipaddr, BOARD_IP_ADDR0, BOARD_IP_ADDR1, BOARD_IP_ADDR2, BOARD_IP_ADDR3);
    IP4_ADDR(&netmask, BOARD_NETMASK0, BOARD_NETMASK1, BOARD_NETMASK2, BOARD_NETMASK3);
    IP4_ADDR(&gw, BOARD_GW_ADDR0, BOARD_GW_ADDR1, BOARD_GW_ADDR2, BOARD_GW_ADDR3);

    /* Add network interface */
    netif_add(&g_netif, &ipaddr, &netmask, &gw, NULL, &ethernetif_init, &ethernet_input);

    /* Set as default network interface and bring up link */
    netif_set_default(&g_netif);
    netif_set_up(&g_netif);

    /* Start DHCP Auto IP Acquisition */
    dhcp_start(&g_netif);
    printf("[DHCP] Requesting IP address from DHCP server...\r\n");

    /* Initialize TCP Server Demo on port 8080 */
    tcp_server_demo_init();
}

void lwip_demo_poll(void)
{
    uint32_t now = sys_now();

    /* 1. Poll incoming Ethernet frames until queue is empty */
    while (ethernetif_input(&g_netif) == ERR_OK) {
    }

    /* 2. Handle TCP Fast/Slow Timers (~250ms) */
    if ((now - sg_tcp_timer) >= 250U) {
        sg_tcp_timer = now;
        tcp_tmr();
    }

    /* 3. Handle ARP Timer (~5000ms) */
    if ((now - sg_arp_timer) >= 5000U) {
        sg_arp_timer = now;
        etharp_tmr();
    }

    /* 4. Handle DHCP Fine Timer (~500ms) */
    if ((now - sg_dhcp_fine_timer) >= DHCP_FINE_TIMER_MSECS) {
        sg_dhcp_fine_timer = now;
        dhcp_fine_tmr();
    }

    /* 5. Handle DHCP Coarse Timer (~60000ms) */
    if ((now - sg_dhcp_coarse_timer) >= (DHCP_COARSE_TIMER_SECS * 1000U)) {
        sg_dhcp_coarse_timer = now;
        dhcp_coarse_tmr();
    }

    /* 6. Check DHCP bound status and print IP when acquired */
    if ((g_netif.dhcp != NULL) && (g_netif.dhcp->state == DHCP_BOUND)) {
        if (sg_dhcp_status == 0) {
            sg_dhcp_status = 1;
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
    }
}
