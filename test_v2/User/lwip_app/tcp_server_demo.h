/*!
    \file    tcp_server_demo.h
    \brief   LwIP TCP Client/Server Compatibility Header
*/

#ifndef __TCP_SERVER_DEMO_H
#define __TCP_SERVER_DEMO_H

#include "tcp_client_app.h"

#define TCP_SERVER_PORT TCP_CLIENT_DEFAULT_SERVER_PORT

/* Compatibility Wrappers */
#define tcp_server_demo_init      tcp_client_app_init
#define tcp_server_send_telemetry tcp_client_send_telemetry

#endif /* __TCP_SERVER_DEMO_H */
