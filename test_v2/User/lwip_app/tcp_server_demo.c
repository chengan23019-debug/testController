/*!
    \file    tcp_server_demo.c
    \brief   LwIP TCP Echo Server Implementation
*/

#include "tcp_server_demo.h"
#include "lwip/memp.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
    if (p != NULL) {
        /* Inform LwIP that data has been processed */
        tcp_recved(tpcb, p->tot_len);

        printf("[TCP Received Message] (%d bytes): %.*s\r\n", p->len, p->len, (char *)p->payload);

        /* Echo received data back to client */
        tcp_write(tpcb, p->payload, p->len, 1);
        tcp_output(tpcb);

        /* Free pbuf */
        pbuf_free(p);
    } else if (err == ERR_OK) {
        /* Client disconnected */
        printf("[TCP Server] Client disconnected.\r\n");
        return tcp_close(tpcb);
    }

    return ERR_OK;
}

static err_t tcp_server_accept(void *arg, struct tcp_pcb *newpcb, err_t err)
{
    printf("[TCP Server] Client connected! IP: %d.%d.%d.%d, Port: %d\r\n",
           (uint8_t)(newpcb->remote_ip.addr),
           (uint8_t)(newpcb->remote_ip.addr >> 8),
           (uint8_t)(newpcb->remote_ip.addr >> 16),
           (uint8_t)(newpcb->remote_ip.addr >> 24),
           newpcb->remote_port);

    /* Setup receive callback */
    tcp_recv(newpcb, tcp_server_recv);

    return ERR_OK;
}

void tcp_server_demo_init(void)
{
    struct tcp_pcb *pcb = NULL;

    pcb = tcp_new();
    if (pcb != NULL) {
        err_t err;

        err = tcp_bind(pcb, IP_ADDR_ANY, TCP_SERVER_PORT);
        if (err == ERR_OK) {
            pcb = tcp_listen(pcb);
            tcp_accept(pcb, tcp_server_accept);
            printf("[TCP Server] LwIP TCP Server initialized successfully on port %d!\r\n", TCP_SERVER_PORT);
        } else {
            printf("[TCP Server] Error binding PCB to port %d (err = %d)\r\n", TCP_SERVER_PORT, err);
            memp_free(MEMP_TCP_PCB, pcb);
        }
    } else {
        printf("[TCP Server] Error creating PCB!\r\n");
    }
}
