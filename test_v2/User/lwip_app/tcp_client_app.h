/*!
    \file    tcp_client_app.h
    \brief   LwIP High-Performance TCP Client Header with Auto-Reconnect & Nagle Disabled
*/

#ifndef __TCP_CLIENT_APP_H
#define __TCP_CLIENT_APP_H

#include "lwip/tcp.h"
#include "lwip/ip_addr.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Compatibility macro for disabling Nagle algorithm in LwIP */
#ifndef tcp_nagle_disable
#define tcp_nagle_disable(npcb)   ((npcb)->flags |= TF_NODELAY)
#define tcp_nagle_enable(npcb)    ((npcb)->flags &= ~TF_NODELAY)
#define tcp_nagle_disabled(npcb)  (((npcb)->flags & TF_NODELAY) != 0)
#endif

/* Default Target Center Server Configuration */
#define TCP_CLIENT_DEFAULT_SERVER_IP0   192
#define TCP_CLIENT_DEFAULT_SERVER_IP1   168
#define TCP_CLIENT_DEFAULT_SERVER_IP2   31
#define TCP_CLIENT_DEFAULT_SERVER_IP3   73
#define TCP_CLIENT_DEFAULT_SERVER_PORT  8080

/* TCP Client Reconnect Interval (ms) */
#define TCP_CLIENT_RECONNECT_INTERVAL_MS 3000U

/* TCP Client Connection State Machine */
typedef enum {
    TCP_CLIENT_DISCONNECTED = 0,
    TCP_CLIENT_CONNECTING,
    TCP_CLIENT_CONNECTED,
    TCP_CLIENT_RETRY_WAIT
} tcp_client_state_t;

/* TCP Client API */
void tcp_client_app_init(void);
void tcp_client_app_poll(void);
void tcp_client_set_server(uint8_t ip0, uint8_t ip1, uint8_t ip2, uint8_t ip3, uint16_t port);
void tcp_client_send_telemetry(void);
bool tcp_client_send_sdram_burst_chunk(const uint8_t *chunk_data, uint16_t chunk_len, bool has_more_data);

bool tcp_client_is_connected(void);
tcp_client_state_t tcp_client_get_state(void);
const char *tcp_client_get_state_str(void);

void tcp_client_set_burst_mode(bool is_bursting);
bool tcp_client_is_burst_mode(void);

#ifdef __cplusplus
}
#endif

#endif /* __TCP_CLIENT_APP_H */
