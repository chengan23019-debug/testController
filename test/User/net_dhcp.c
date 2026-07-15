#include "net_dhcp.h"
#include "lwip/opt.h"
#include "lwip/dhcp.h"
#include "lwip/netif.h"
#include "lwip/tcpip.h"
#include "ethernetif.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>

/* Safe IP parsing macros to ensure compatibility across lwIP versions */
#define IP_OCTET1(addr_ptr)   ((uint8_t)(ip4_addr_get_u32(addr_ptr) & 0xFF))
#define IP_OCTET2(addr_ptr)   ((uint8_t)((ip4_addr_get_u32(addr_ptr) >> 8) & 0xFF))
#define IP_OCTET3(addr_ptr)   ((uint8_t)((ip4_addr_get_u32(addr_ptr) >> 16) & 0xFF))
#define IP_OCTET4(addr_ptr)   ((uint8_t)((ip4_addr_get_u32(addr_ptr) >> 24) & 0xFF))

struct netif g_netif;
int errno;

static void lwip_secure_init_callback(void *arg)
{
    ip_addr_t ipaddr;
    ip_addr_t netmask;
    ip_addr_t gw;

    IP4_ADDR(&ipaddr, 0, 0, 0, 0);
    IP4_ADDR(&netmask, 0, 0, 0, 0);
    IP4_ADDR(&gw, 0, 0, 0, 0);

    printf("[NET] Adding network interface...\r\n");
    netif_add(&g_netif, &ipaddr, &netmask, &gw, NULL, &ethernetif_init, &tcpip_input);
    netif_set_default(&g_netif);
    netif_set_up(&g_netif);

    printf("[DHCP] Starting DHCP client...\r\n");
    dhcp_start(&g_netif);
}

static void app_net_thread(void *pvParameters)
{
    uint8_t dhcp_state = 0;
    uint16_t phy_bsr = 0;
    uint32_t print_count = 0;

    /* Wait a moment for hardware link to stabilize */
    vTaskDelay(pdMS_TO_TICKS(500));

    printf("[NET] Initializing TCP/IP Stack (lwIP)...\r\n");
    tcpip_init(lwip_secure_init_callback, NULL);

    while (1) {
        /* Poll and print PHY link status every 2 seconds */
        if (print_count % 4 == 0) {
            extern uint16_t bsp_enet_phy_bsr_read(void);
            extern uint16_t bsp_enet_phy_bcr_read(void);
            extern uint16_t bsp_enet_phy_reg_read(uint16_t reg_addr);
            uint16_t phy_bcr = bsp_enet_phy_bcr_read();
            uint16_t phy_id1 = bsp_enet_phy_reg_read(2);
            uint16_t phy_id2 = bsp_enet_phy_reg_read(3);
            uint16_t phy_spec = bsp_enet_phy_reg_read(0x11);
            phy_bsr = bsp_enet_phy_bsr_read();
            printf("[NET] PHY Link: %s (BSR: 0x%04X, BCR: 0x%04X) ID: 0x%04X-0x%04X SPEC: 0x%04X\r\n", 
                   (phy_bsr & 0x0004) ? "UP" : "DOWN", phy_bsr, phy_bcr, phy_id1, phy_id2, phy_spec);
        }
        print_count++;

        /* Check if DHCP has obtained an IP address */
        if (dhcp_supplied_address(&g_netif)) {
            if (dhcp_state == 0) {
                dhcp_state = 1;
                printf("\r\n========================================\r\n");
                printf("[DHCP] Network Configuration Successful!\r\n");
                printf(" -> IP Address:  %d.%d.%d.%d\r\n",
                       IP_OCTET1(netif_ip4_addr(&g_netif)),
                       IP_OCTET2(netif_ip4_addr(&g_netif)),
                       IP_OCTET3(netif_ip4_addr(&g_netif)),
                       IP_OCTET4(netif_ip4_addr(&g_netif)));
                printf(" -> Netmask:     %d.%d.%d.%d\r\n",
                       IP_OCTET1(netif_ip4_netmask(&g_netif)),
                       IP_OCTET2(netif_ip4_netmask(&g_netif)),
                       IP_OCTET3(netif_ip4_netmask(&g_netif)),
                       IP_OCTET4(netif_ip4_netmask(&g_netif)));
                printf(" -> Gateway:     %d.%d.%d.%d\r\n",
                       IP_OCTET1(netif_ip4_gw(&g_netif)),
                       IP_OCTET2(netif_ip4_gw(&g_netif)),
                       IP_OCTET3(netif_ip4_gw(&g_netif)),
                       IP_OCTET4(netif_ip4_gw(&g_netif)));
                printf("========================================\r\n\r\n");
            }
        } else {
            if (dhcp_state == 1) {
                dhcp_state = 0;
                printf("[DHCP] IP Address lost or released.\r\n");
            }
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/**
 * @brief Create the LwIP network thread/task in FreeRTOS.
 */
void app_net_task_create(void)
{
    xTaskCreate(app_net_thread, "NET_THREAD", 512, NULL, 3, NULL);
}
