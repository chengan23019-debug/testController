/*!
    \file    tcp_server_demo.c
    \brief   测功机以太网 TCP 服务器实现 (支持二进制协议解析与高频数据推送)
*/

#include "tcp_server_demo.h"
#include "lwip/memp.h"
#include "app_dyno_global.h"
#include "app_protocol_binary.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static struct tcp_pcb *g_active_pcb = NULL;
static proto_bin_parser_t g_bin_parser;
static uint8_t g_telemetry_seq = 0;

/**
  * @brief  发送主动数据帧到客户端 (供 50Hz 任务调用)
  */
void tcp_server_send_telemetry(void)
{
    dyno_system_status_t status;
    uint8_t tx_buf[128];
    uint32_t tx_len;
    err_t err;

    if (g_active_pcb == NULL) {
        return;
    }

    app_dyno_get_status(&status);
    tx_len = proto_bin_pack_telemetry(g_telemetry_seq++, &status, tx_buf, sizeof(tx_buf));

    if (tx_len > 0 && g_active_pcb->state == ESTABLISHED) {
        err = tcp_write(g_active_pcb, tx_buf, (u16_t)tx_len, TCP_WRITE_FLAG_COPY);
        if (err == ERR_OK) {
            tcp_output(g_active_pcb);
        }
    }
}

static err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
    uint8_t ack_buf[64];
    uint32_t ack_len = 0;
    uint8_t *payload_ptr;
    u16_t i;

    (void)arg;

    if (p != NULL) {
        tcp_recved(tpcb, p->tot_len);

        payload_ptr = (uint8_t *)p->payload;

        /* 逐字节传入二进制协议状态机解析 */
        for (i = 0; i < p->len; i++) {
            if (proto_bin_parse_byte(&g_bin_parser, payload_ptr[i])) {
                /* 解析出一帧完整合法命令帧，进行业务处理 */
                ack_len = sizeof(ack_buf);
                proto_bin_process_frame(g_bin_parser.cmd_id, g_bin_parser.seq, 
                                        g_bin_parser.payload_buf, g_bin_parser.rx_len, 
                                        ack_buf, &ack_len);

                /* 发送 ACK 响应 */
                if (ack_len > 0) {
                    tcp_write(tpcb, ack_buf, (u16_t)ack_len, TCP_WRITE_FLAG_COPY);
                    tcp_output(tpcb);
                }
            }
        }

        pbuf_free(p);
    } else if (err == ERR_OK) {
        printf("[TCP Server] Client disconnected.\r\n");
        if (g_active_pcb == tpcb) {
            g_active_pcb = NULL;
        }
        return tcp_close(tpcb);
    }

    return ERR_OK;
}

static err_t tcp_server_accept(void *arg, struct tcp_pcb *newpcb, err_t err)
{
    (void)arg;
    (void)err;

    printf("[TCP Server] Client connected! IP: %d.%d.%d.%d, Port: %d\r\n",
           (uint8_t)(newpcb->remote_ip.addr),
           (uint8_t)(newpcb->remote_ip.addr >> 8),
           (uint8_t)(newpcb->remote_ip.addr >> 16),
           (uint8_t)(newpcb->remote_ip.addr >> 24),
           newpcb->remote_port);

    g_active_pcb = newpcb;
    proto_bin_parser_init(&g_bin_parser);

    tcp_recv(newpcb, tcp_server_recv);

    return ERR_OK;
}

void tcp_server_demo_init(void)
{
    struct tcp_pcb *pcb = NULL;
    err_t err;

    pcb = tcp_new();
    if (pcb != NULL) {
        err = tcp_bind(pcb, IP_ADDR_ANY, TCP_SERVER_PORT);
        if (err == ERR_OK) {
            pcb = tcp_listen(pcb);
            tcp_accept(pcb, tcp_server_accept);
            printf("[TCP Server] LwIP Dyno TCP Server initialized on port %d!\r\n", TCP_SERVER_PORT);
        } else {
            printf("[TCP Server] Error binding PCB to port %d (err = %d)\r\n", TCP_SERVER_PORT, err);
            memp_free(MEMP_TCP_PCB, pcb);
        }
    } else {
        printf("[TCP Server] Error creating PCB!\r\n");
    }
}
