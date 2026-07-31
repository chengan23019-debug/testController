/*!
    \file    tcp_server_demo.h
    \brief   LwIP TCP Server Header
*/

#ifndef __TCP_SERVER_DEMO_H
#define __TCP_SERVER_DEMO_H

#include "lwip/tcp.h"

#define TCP_SERVER_PORT 8080

void tcp_server_demo_init(void);
void tcp_server_send_telemetry(void);

#endif /* __TCP_SERVER_DEMO_H */
