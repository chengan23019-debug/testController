/*!
    \file    ethernetif.c
    \brief   LwIP Network Interface Port Implementation for GD32F470 & LAN8720/LAN8702
*/

#include "lwip/opt.h"
#include "lwip/def.h"
#include "lwip/mem.h"
#include "lwip/pbuf.h"
#include "lwip/sys.h"
#include <lwip/stats.h>
#include <lwip/snmp.h>
#include "netif/etharp.h"
#include "lwip/err.h"
#include "ethernetif.h"
#include "bsp_lan8720.h"
#include <string.h>

#define IFNAME0 'e'
#define IFNAME1 '0'

#define MAX_ETH_MSG_SIZE 1536

static uint8_t sg_rx_buffer[MAX_ETH_MSG_SIZE];
static uint8_t sg_tx_buffer[MAX_ETH_MSG_SIZE];

/**
 * In this function, the hardware should be initialized.
 * Called from ethernetif_init().
 */
static void low_level_init(struct netif *netif)
{
    /* set MAC hardware address length */
    netif->hwaddr_len = ETHARP_HWADDR_LEN;

    /* set MAC hardware address from bsp_lan8720 driver */
    bsp_lan8720_get_mac(netif->hwaddr);

    /* maximum transfer unit */
    netif->mtu = 1500;

    /* device capabilities */
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP;
}

/**
 * Transmission of packet.
 */
static err_t low_level_output(struct netif *netif, struct pbuf *p)
{
    struct pbuf *q;
    uint16_t framelen = 0;

    for (q = p; q != NULL; q = q->next) {
        if ((framelen + q->len) > MAX_ETH_MSG_SIZE) {
            return ERR_MEM;
        }
        memcpy(&sg_tx_buffer[framelen], q->payload, q->len);
        framelen += q->len;
    }

    if (LAN8720_OK == bsp_lan8720_send_packet(sg_tx_buffer, framelen)) {
        return ERR_OK;
    } else {
        return ERR_IF;
    }
}

/**
 * Reception of packet into pbuf chain.
 */
static struct pbuf *low_level_input(struct netif *netif)
{
    struct pbuf *p = NULL;
    struct pbuf *q;
    uint16_t len = 0;
    uint16_t l = 0;

    len = bsp_lan8720_receive_packet(sg_rx_buffer, MAX_ETH_MSG_SIZE);
    if (len == 0) {
        return NULL;
    }

    /* Allocate pbuf chain from pool */
    p = pbuf_alloc(PBUF_RAW, len, PBUF_POOL);
    if (p != NULL) {
        for (q = p; q != NULL; q = q->next) {
            memcpy(q->payload, &sg_rx_buffer[l], q->len);
            l += q->len;
        }
    }

    return p;
}

/**
 * Should be called when a packet is ready to be read from interface.
 */
err_t ethernetif_input(struct netif *netif)
{
    err_t err;
    struct pbuf *p;

    p = low_level_input(netif);
    if (p == NULL) {
        return ERR_MEM;
    }

    err = netif->input(p, netif);
    if (err != ERR_OK) {
        LWIP_DEBUGF(NETIF_DEBUG, ("ethernetif_input: IP input error\n"));
        pbuf_free(p);
        p = NULL;
    }

    return err;
}

/**
 * Set up network interface.
 */
err_t ethernetif_init(struct netif *netif)
{
    LWIP_ASSERT("netif != NULL", (netif != NULL));

#if LWIP_NETIF_HOSTNAME
    netif->hostname = "gd32f470_lwip";
#endif /* LWIP_NETIF_HOSTNAME */

    NETIF_INIT_SNMP(netif, snmp_ifType_ethernet_csmacd, 100000000);

    netif->name[0] = IFNAME0;
    netif->name[1] = IFNAME1;
    netif->output = etharp_output;
    netif->linkoutput = low_level_output;

    low_level_init(netif);

    return ERR_OK;
}
