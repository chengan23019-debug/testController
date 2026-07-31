/**
  ******************************************************************************
  * @file    app_protocol_binary.h
  * @brief   测功机以太网自定义高频二进制协议头文件 (ARMCC V5 兼容)
  ******************************************************************************
  */

#ifndef __APP_PROTOCOL_BINARY_H
#define __APP_PROTOCOL_BINARY_H

#include "gd32f4xx.h"
#include "app_dyno_global.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PROTO_BIN_HEADER_HIGH   0x55
#define PROTO_BIN_HEADER_LOW    0xAA

/* 功能码 Command IDs */
#define CMD_BIN_TELEMETRY_REPORT  0x01  /* 50Hz 高频测试数据主动上报 (MCU -> PC) */
#define CMD_BIN_SET_MODE          0x10  /* 设置测试模式与目标加载值 (PC -> MCU) */
#define CMD_BIN_ESTOP             0x11  /* 紧急停止 (PC -> MCU) */
#define CMD_BIN_SET_PID           0x12  /* 设置/修改 PID 参数 (PC -> MCU) */
#define CMD_BIN_CALIBRATE         0x13  /* 零点/增益标定指令 (PC -> MCU) */
#define CMD_BIN_ACK_RESPONSE      0x80  /* 通用确认应答 (MCU -> PC) */

#pragma pack(push, 1)

/* 50Hz 上报帧 Payload 结构体 */
typedef struct {
    uint32_t timestamp_ms;    /* 系统时间戳 (ms) */
    uint8_t  work_mode;       /* 当前工作模式 */
    uint8_t  alarm_flags;     /* 报警与状态标志位 */

    /* 机械测量参量 */
    float    torque_nm;       /* 实际扭矩 (N.m) */
    float    speed_rpm;       /* 实际转速 (RPM) */
    float    mech_power_w;    /* 计算机械功率 (W) */

    /* 当前控制输出 */
    float    dac_voltage;     /* 当前 DAC 输出电压 (V) */

    /* HLW8112 电参量 */
    float    elec_voltage;    /* 电机电压 (V) */
    float    elec_current;    /* 电机电流 (A) */
    float    elec_power;      /* 电机输入功率 (W) */
    float    power_factor;    /* 功率因数 */
    float    efficiency;      /* 效率 (%) */
} proto_bin_telemetry_payload_t;

/* 设置模式 Payload 结构体 (Cmd 0x10) */
typedef struct {
    uint8_t  target_mode;     /* 目标模式: 1-Manual, 2-Const Torque, 3-Const Power */
    float    target_value;    /* 目标设定值 (DAC电压/扭矩/功率) */
} proto_bin_set_mode_payload_t;

/* PID 参数设置 Payload 结构体 (Cmd 0x12) */
typedef struct {
    uint8_t  loop_type;       /* 0-恒扭矩, 1-恒功率 */
    float    kp;
    float    ki;
    float    kd;
    float    max_dac;
} proto_bin_set_pid_payload_t;

/* 应答帧 Payload 结构体 (Cmd 0x80) */
typedef struct {
    uint8_t  reply_cmd_id;    /* 被响应的功能码 */
    uint8_t  status_code;     /* 0:成功, 1:模式非法, 2:超出范围, 3:校验错误 */
} proto_bin_ack_payload_t;

#pragma pack(pop)

/* CRC16 计算工具函数 */
uint16_t proto_bin_calc_crc16(const uint8_t *buffer, uint32_t length);

/* 组包与发送接口 */
uint32_t proto_bin_pack_telemetry(uint8_t seq, const dyno_system_status_t *status, uint8_t *tx_buf, uint32_t max_len);
uint32_t proto_bin_pack_ack(uint8_t seq, uint8_t reply_cmd, uint8_t status_code, uint8_t *tx_buf, uint32_t max_len);

/* 接收流解析接口 (状态机逐字节解包) */
typedef struct {
    uint8_t  state;
    uint8_t  cmd_id;
    uint8_t  seq;
    uint16_t rx_len;
    uint16_t rx_cnt;
    uint8_t  payload_buf[256];
} proto_bin_parser_t;

void proto_bin_parser_init(proto_bin_parser_t *parser);
bool proto_bin_parse_byte(proto_bin_parser_t *parser, uint8_t byte);
void proto_bin_process_frame(uint8_t cmd_id, uint8_t seq, const uint8_t *payload, uint16_t len, uint8_t *ack_buf, uint32_t *ack_len);

#ifdef __cplusplus
}
#endif

#endif /* __APP_PROTOCOL_BINARY_H */
