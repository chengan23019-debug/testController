/*!
    \file    lwipopts.h
    \brief   LwIP options configuration for GD32F470 FreeRTOS project
*/

#ifndef __LWIPOPTS_H__
#define __LWIPOPTS_H__

/* No RTOS (Bare-metal / Task polling environment for LwIP core) */
#define NO_SYS                  1
#define SYS_LIGHTWEIGHT_PROT    0

/* Memory options */
#define MEM_ALIGNMENT           4
#define MEM_SIZE                (32 * 1024)

#define MEMP_NUM_PBUF           32
#define MEMP_NUM_UDP_PCB        6
#define MEMP_NUM_TCP_PCB        10
#define MEMP_NUM_TCP_PCB_LISTEN 2
#define MEMP_NUM_TCP_SEG        32
#define MEMP_NUM_SYS_TIMEOUT    8

/* Pbuf options */
#define PBUF_POOL_SIZE          32
#define PBUF_POOL_BUFSIZE       1536

/* TCP options (High-Speed & Large Window Optimization) */
#define LWIP_TCP                1
#define TCP_TTL                 255
#define TCP_QUEUE_OOSEQ         0
#define TCP_MSS                 (1500 - 40)
#define TCP_SND_BUF             (8 * TCP_MSS)
#define TCP_SND_QUEUELEN        (4 * TCP_SND_BUF / TCP_MSS)
#define TCP_WND                 (8 * TCP_MSS)

/* ICMP options */
#define LWIP_ICMP               1

/* UDP options */
#define LWIP_UDP                1
#define UDP_TTL                 255

/* DHCP options */
#define LWIP_DHCP               1
#define DHCP_DOES_ARP_CHECK     0  /* 禁用 DHCP 慢速 ARP 冲突检测，极大加快获取 IP 速度 */

/* Netif Link & Status Callback options */
#define LWIP_NETIF_LINK_CALLBACK    1
#define LWIP_NETIF_STATUS_CALLBACK  1

/* Stats options */
#define LWIP_STATS              0
#define LWIP_PROVIDE_ERRNO      1

/* 
 * Checksum options:
 * 开启软件校验和计算，彻底消除硬件 MAC 卸载引擎在 UDP/DHCP 广播报文 (0.0.0.0:68 -> 255.255.255.255:67)
 * 计算 Pseudo-header 异常导致的路由器/DHCP 服务器丢包问题。
 */
#define CHECKSUM_BY_HARDWARE    0

#define CHECKSUM_GEN_IP         1
#define CHECKSUM_GEN_UDP        1
#define CHECKSUM_GEN_TCP        1
#define CHECKSUM_GEN_ICMP       1

#define CHECKSUM_CHECK_IP       1
#define CHECKSUM_CHECK_UDP      1
#define CHECKSUM_CHECK_TCP      1

/* Disable Sequential and Socket API in NO_SYS mode */
#define LWIP_NETCONN            0
#define LWIP_SOCKET             0

#endif /* __LWIPOPTS_H__ */
