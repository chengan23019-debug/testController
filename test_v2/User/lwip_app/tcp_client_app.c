/*!
    \file    tcp_client_app.c
    \brief   测功机以太网高可靠 TCP 客户端实现 (支持自动重连、禁用Nagle、零拷贝DMA、突发大块回传)
*/

#include "tcp_client_app.h"
#include "lwip/memp.h"
#include "systick.h"
#include "app_dyno_global.h"
#include "app_protocol_binary.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Global TCP Client State */
static struct tcp_pcb *g_client_pcb = NULL;
static tcp_client_state_t g_client_state = TCP_CLIENT_DISCONNECTED;
static uint32_t g_last_reconnect_tick = 0;

static struct ip_addr g_server_ip;
static uint16_t g_server_port = TCP_CLIENT_DEFAULT_SERVER_PORT;

static proto_bin_parser_t g_bin_parser;
static uint8_t g_telemetry_seq = 0;
static volatile bool g_is_burst_transmitting = false;

/* Forward declarations */
static err_t app_tcp_client_connected(void *arg, struct tcp_pcb *tpcb, err_t err);
static err_t app_tcp_client_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);
static void  app_tcp_client_error(void *arg, err_t err);
static err_t app_tcp_client_sent(void *arg, struct tcp_pcb *tpcb, u16_t len);

/*!
    \brief      Initialize TCP Client Configuration
*/
void tcp_client_app_init(void)
{
    IP4_ADDR(&g_server_ip, 
             TCP_CLIENT_DEFAULT_SERVER_IP0, 
             TCP_CLIENT_DEFAULT_SERVER_IP1, 
             TCP_CLIENT_DEFAULT_SERVER_IP2, 
             TCP_CLIENT_DEFAULT_SERVER_IP3);
    g_server_port = TCP_CLIENT_DEFAULT_SERVER_PORT;

    g_client_state = TCP_CLIENT_DISCONNECTED;
    g_client_pcb = NULL;
    g_last_reconnect_tick = 0;
    g_is_burst_transmitting = false;

    proto_bin_parser_init(&g_bin_parser);

    printf("[TCP Client] Initialized. Target Center Server: %d.%d.%d.%d:%d\r\n",
           TCP_CLIENT_DEFAULT_SERVER_IP0,
           TCP_CLIENT_DEFAULT_SERVER_IP1,
           TCP_CLIENT_DEFAULT_SERVER_IP2,
           TCP_CLIENT_DEFAULT_SERVER_IP3,
           g_server_port);
}

/*!
    \brief      Set Target Server IP and Port dynamically
*/
void tcp_client_set_server(uint8_t ip0, uint8_t ip1, uint8_t ip2, uint8_t ip3, uint16_t port)
{
    IP4_ADDR(&g_server_ip, ip0, ip1, ip2, ip3);
    g_server_port = port;

    /* If currently connected or connecting, disconnect to reconnect to new server */
    if (g_client_pcb != NULL) {
        tcp_abort(g_client_pcb);
        g_client_pcb = NULL;
    }
    g_client_state = TCP_CLIENT_DISCONNECTED;
    g_last_reconnect_tick = sys_now() - TCP_CLIENT_RECONNECT_INTERVAL_MS;
}

/*!
    \brief      TCP Connected Callback
*/
static err_t app_tcp_client_connected(void *arg, struct tcp_pcb *tpcb, err_t err)
{
    (void)arg;

    if (err == ERR_OK) {
        g_client_state = TCP_CLIENT_CONNECTED;

        /* 核心优化 1: 立即彻底禁用 Nagle 算法，消除 40ms~200ms 的推流抖动 */
        tcp_nagle_disable(tpcb);

        /* 注册接收、错误及发送确认回调 */
        tcp_recv(tpcb, app_tcp_client_recv);
        tcp_err(tpcb, app_tcp_client_error);
        tcp_sent(tpcb, app_tcp_client_sent);

        proto_bin_parser_init(&g_bin_parser);

        printf("[TCP Client] Connected to Center Server (%d.%d.%d.%d:%d) successfully! (Nagle Disabled)\r\n",
               (uint8_t)(g_server_ip.addr),
               (uint8_t)(g_server_ip.addr >> 8),
               (uint8_t)(g_server_ip.addr >> 16),
               (uint8_t)(g_server_ip.addr >> 24),
               g_server_port);
    } else {
        printf("[TCP Client] Connection failed callback with err = %d\r\n", err);
        g_client_state = TCP_CLIENT_RETRY_WAIT;
        g_last_reconnect_tick = sys_now();
    }

    return ERR_OK;
}

/*!
    \brief      TCP Receive Callback
*/
static err_t app_tcp_client_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
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
        /* Center server closed connection */
        printf("[TCP Client] Center Server closed connection.\r\n");
        if (g_client_pcb == tpcb) {
            g_client_pcb = NULL;
        }
        g_client_state = TCP_CLIENT_RETRY_WAIT;
        g_last_reconnect_tick = sys_now();
        return tcp_close(tpcb);
    }

    return ERR_OK;
}

/*!
    \brief      TCP Error Callback (PCB is already deallocated by LwIP before this callback)
*/
static void app_tcp_client_error(void *arg, err_t err)
{
    (void)arg;
    printf("[TCP Client] TCP Error occurred (err = %d). Entering retry wait state.\r\n", err);
    g_client_pcb = NULL;
    g_client_state = TCP_CLIENT_RETRY_WAIT;
    g_last_reconnect_tick = sys_now();
}

/*!
    \brief      TCP Sent Callback
*/
static err_t app_tcp_client_sent(void *arg, struct tcp_pcb *tpcb, u16_t len)
{
    (void)arg;
    (void)tpcb;
    (void)len;
    return ERR_OK;
}

/*!
    \brief      Periodic TCP Client State Machine Poll (Auto-Reconnect)
*/
void tcp_client_app_poll(void)
{
    uint32_t now = sys_now();
    err_t err;

    switch (g_client_state) {
        case TCP_CLIENT_DISCONNECTED:
        case TCP_CLIENT_RETRY_WAIT:
            /* 每隔 3000ms 尝试自动重连中心服务器 */
            if ((now - g_last_reconnect_tick) >= TCP_CLIENT_RECONNECT_INTERVAL_MS) {
                g_last_reconnect_tick = now;

                if (g_client_pcb != NULL) {
                    tcp_abort(g_client_pcb);
                    g_client_pcb = NULL;
                }

                g_client_pcb = tcp_new();
                if (g_client_pcb != NULL) {
                    g_client_state = TCP_CLIENT_CONNECTING;
                    tcp_err(g_client_pcb, app_tcp_client_error);

                    /* 主动发起 TCP 握手 */
                    err = tcp_connect(g_client_pcb, &g_server_ip, g_server_port, app_tcp_client_connected);
                    if (err != ERR_OK) {
                        printf("[TCP Client] tcp_connect initiation returned err = %d\r\n", err);
                        g_client_state = TCP_CLIENT_RETRY_WAIT;
                    }
                } else {
                    printf("[TCP Client] Error: Cannot allocate new TCP PCB!\r\n");
                }
            }
            break;

        case TCP_CLIENT_CONNECTING:
            /* Connecting in progress, timeout handled by tcp_tmr / tcp_err */
            break;

        case TCP_CLIENT_CONNECTED:
            /* Connection established and healthy */
            break;

        default:
            g_client_state = TCP_CLIENT_DISCONNECTED;
            break;
    }
}

/*!
    \brief      发送 50Hz 遥测状态数据帧 (极低延迟、零抖动推流)
*/
void tcp_client_send_telemetry(void)
{
    dyno_system_status_t status;
    uint8_t tx_buf[128];
    uint32_t tx_len;
    err_t err;

    if (g_client_state != TCP_CLIENT_CONNECTED || g_client_pcb == NULL) {
        return;
    }

    if (g_client_pcb->state != ESTABLISHED) {
        return;
    }

    app_dyno_get_status(&status);
    tx_len = proto_bin_pack_telemetry(g_telemetry_seq++, &status, tx_buf, sizeof(tx_buf));

    if (tx_len > 0) {
        if (tcp_sndbuf(g_client_pcb) >= tx_len) {
            err = tcp_write(g_client_pcb, tx_buf, (u16_t)tx_len, TCP_WRITE_FLAG_COPY);
            if (err == ERR_OK) {
                /* 立即刷出，通知以太网 DMA 零延迟发送 */
                tcp_output(g_client_pcb);
            }
        }
    }
}

/*!
    \brief      从 SDRAM 极速突发回传数据块 (大包非阻塞流控)
    \param[in]  chunk_data: 数据块指针 (例如 1024 字节)
    \param[in]  chunk_len: 数据块长度
    \param[in]  has_more_data: 是否还有后续数据块待发送
    \retval     true: 发送成功写入缓冲区, false: 缓冲区满需下一拍重试
*/
bool tcp_client_send_sdram_burst_chunk(const uint8_t *chunk_data, uint16_t chunk_len, bool has_more_data)
{
    u16_t available_buf;
    u8_t write_flags;
    err_t err;

    if (g_client_state != TCP_CLIENT_CONNECTED || g_client_pcb == NULL) {
        return false;
    }

    if (g_client_pcb->state != ESTABLISHED) {
        return false;
    }

    /* 1. 检查当前 TCP 发送缓冲区可用余量 */
    available_buf = tcp_sndbuf(g_client_pcb);

    if (available_buf >= chunk_len) {
        /* 2. 写入数据：如果有后续数据，打上 TCP_WRITE_FLAG_MORE 标志，允许网卡智能打包 */
        write_flags = TCP_WRITE_FLAG_COPY;
        if (has_more_data) {
            write_flags |= TCP_WRITE_FLAG_MORE;
        }

        err = tcp_write(g_client_pcb, chunk_data, chunk_len, write_flags);
        if (err == ERR_OK) {
            /* 3. 立即刷新输出，通知网卡 DMA 开始发送，零延迟发出 */
            tcp_output(g_client_pcb);
            return true;
        }
    }

    return false;
}

/*!
    \brief      Check if TCP Client is connected to Center Server
*/
bool tcp_client_is_connected(void)
{
    return (g_client_state == TCP_CLIENT_CONNECTED) && (g_client_pcb != NULL) && (g_client_pcb->state == ESTABLISHED);
}

/*!
    \brief      Get current client state
*/
tcp_client_state_t tcp_client_get_state(void)
{
    return g_client_state;
}

/*!
    \brief      Get client state description string
*/
const char *tcp_client_get_state_str(void)
{
    switch (g_client_state) {
        case TCP_CLIENT_DISCONNECTED: return "DISCONNECTED";
        case TCP_CLIENT_CONNECTING:   return "CONNECTING";
        case TCP_CLIENT_CONNECTED:    return "CONNECTED";
        case TCP_CLIENT_RETRY_WAIT:   return "RETRY_WAIT";
        default:                      return "UNKNOWN";
    }
}

/*!
    \brief      Set / Get burst transmission mode flag for adaptive FreeRTOS scheduling
*/
void tcp_client_set_burst_mode(bool is_bursting)
{
    g_is_burst_transmitting = is_bursting;
}

bool tcp_client_is_burst_mode(void)
{
    return g_is_burst_transmitting;
}
