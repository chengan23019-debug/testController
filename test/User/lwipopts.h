/*!
    \file    lwipopts.h
    \brief   LwIP options configuration for FreeRTOS
*/

#ifndef LWIPOPTS_H
#define LWIPOPTS_H

/* OS Support Configuration */
#define NO_SYS                  0        /* NO_SYS==0: Use OS features (FreeRTOS) */
#define SYS_LIGHTWEIGHT_PROT    1        /* 1: enable task protection for critical regions */

/* Memory options */
#define MEM_ALIGNMENT           4        /* 4 byte alignment for Cortex-M4 */
#define MEM_SIZE                (16*1024)/* heap size */
#define MEMP_NUM_PBUF           10
#define MEMP_NUM_UDP_PCB        6
#define MEMP_NUM_TCP_PCB        10
#define MEMP_NUM_TCP_PCB_LISTEN 6
#define MEMP_NUM_TCP_SEG        12
#define MEMP_NUM_SYS_TIMEOUT    10
#define MEMP_NUM_NETBUF         8

/* Pbuf options */
#define PBUF_POOL_SIZE          10
#define PBUF_POOL_BUFSIZE       1500

/* TCP options */
#define LWIP_TCP                1
#define TCP_TTL                 255
#define TCP_QUEUE_OOSEQ         0
#define TCP_MSS                 (1500 - 40)
#define TCP_SND_BUF             (2*TCP_MSS)
#define TCP_SND_QUEUELEN        ((6* TCP_SND_BUF)/TCP_MSS)
#define TCP_WND                 (2*TCP_MSS)

/* ICMP options */
#define LWIP_ICMP               1

/* DHCP options */
#define LWIP_DHCP               1
#define LWIP_NETIF_STATUS_CALLBACK 1

/* UDP options */
#define LWIP_UDP                1
#define UDP_TTL                 255

/* Statistics options */
#define LWIP_STATS              0
#define LWIP_PROVIDE_ERRNO      1

/* FreeRTOS Specific OS Porting Options */
#define TCPIP_THREAD_NAME              "TCPIP"
#define TCPIP_THREAD_STACKSIZE         1024
#define TCPIP_THREAD_PRIO              3
#define TCPIP_MBOX_SIZE                16
#define DEFAULT_UDP_RECVMBOX_SIZE      8
#define DEFAULT_TCP_RECVMBOX_SIZE      8
#define DEFAULT_ACCEPTMBOX_SIZE        8
#define DEFAULT_THREAD_STACKSIZE       512
#define DEFAULT_THREAD_PRIO            1

/* Sequential & Socket API configurations */
#define LWIP_NETCONN            1        /* Enable Netconn API */
#define LWIP_SOCKET             1        /* Enable Socket API */
#define LWIP_SO_RCVTIMEO        1

/* Checksum options: default to software to ensure compatibility and ease of debugging */
#define CHECKSUM_GEN_IP                 1
#define CHECKSUM_GEN_UDP                1
#define CHECKSUM_GEN_TCP                1
#define CHECKSUM_CHECK_IP               1
#define CHECKSUM_CHECK_UDP              1
#define CHECKSUM_CHECK_TCP              1
#define CHECKSUM_GEN_ICMP               1

#endif /* LWIPOPTS_H */
