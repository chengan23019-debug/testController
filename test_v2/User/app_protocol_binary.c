/**
  ******************************************************************************
  * @file    app_protocol_binary.c
  * @brief   测功机自定义二进制高频协议实现 (ARMCC V5 兼容)
  ******************************************************************************
  */

#include "app_protocol_binary.h"
#include <string.h>

/**
  * @brief  计算 CRC16-Modbus
  */
uint16_t proto_bin_calc_crc16(const uint8_t *buffer, uint32_t length)
{
    uint16_t crc = 0xFFFF;
    uint32_t i;
    uint8_t j;

    for (i = 0; i < length; i++) {
        crc ^= buffer[i];
        for (j = 0; j < 8; j++) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc = (crc >> 1);
            }
        }
    }
    return crc;
}

/**
  * @brief  打包 50Hz 实时数据上报帧 (Cmd 0x01)
  */
uint32_t proto_bin_pack_telemetry(uint8_t seq, const dyno_system_status_t *status, uint8_t *tx_buf, uint32_t max_len)
{
    uint16_t payload_len = sizeof(proto_bin_telemetry_payload_t);
    uint32_t total_len   = 2 + 1 + 1 + 2 + payload_len + 2;
    proto_bin_telemetry_payload_t *payload;
    uint16_t crc;

    if (max_len < total_len || status == NULL || tx_buf == NULL) {
        return 0;
    }

    tx_buf[0] = PROTO_BIN_HEADER_HIGH;
    tx_buf[1] = PROTO_BIN_HEADER_LOW;
    tx_buf[2] = CMD_BIN_TELEMETRY_REPORT;
    tx_buf[3] = seq;
    
    tx_buf[4] = (uint8_t)(payload_len >> 8);
    tx_buf[5] = (uint8_t)(payload_len & 0xFF);

    payload = (proto_bin_telemetry_payload_t *)&tx_buf[6];
    payload->timestamp_ms = status->timestamp_ms;
    payload->work_mode    = (uint8_t)status->mode;
    payload->alarm_flags  = (uint8_t)status->alarm_flags;
    payload->torque_nm    = status->torque_nm;
    payload->speed_rpm    = status->speed_rpm;
    payload->mech_power_w = status->mech_power_w;
    payload->dac_voltage  = status->dac_voltage;
    payload->dc_voltage   = status->dc_voltage;
    payload->dc_current   = status->dc_current;
    payload->elec_voltage = status->elec_voltage;
    payload->elec_current = status->elec_current;
    payload->elec_power   = status->elec_power;
    payload->power_factor = status->power_factor;
    payload->efficiency   = status->efficiency;

    crc = proto_bin_calc_crc16(tx_buf, 6 + payload_len);
    tx_buf[6 + payload_len]     = (uint8_t)(crc & 0xFF);
    tx_buf[6 + payload_len + 1] = (uint8_t)(crc >> 8);

    return total_len;
}

/**
  * @brief  打包 ACK 确认响应包 (Cmd 0x80)
  */
uint32_t proto_bin_pack_ack(uint8_t seq, uint8_t reply_cmd, uint8_t status_code, uint8_t *tx_buf, uint32_t max_len)
{
    uint16_t payload_len = sizeof(proto_bin_ack_payload_t);
    uint32_t total_len   = 2 + 1 + 1 + 2 + payload_len + 2;
    proto_bin_ack_payload_t *payload;
    uint16_t crc;

    if (max_len < total_len || tx_buf == NULL) {
        return 0;
    }

    tx_buf[0] = PROTO_BIN_HEADER_HIGH;
    tx_buf[1] = PROTO_BIN_HEADER_LOW;
    tx_buf[2] = CMD_BIN_ACK_RESPONSE;
    tx_buf[3] = seq;
    tx_buf[4] = (uint8_t)(payload_len >> 8);
    tx_buf[5] = (uint8_t)(payload_len & 0xFF);

    payload = (proto_bin_ack_payload_t *)&tx_buf[6];
    payload->reply_cmd_id = reply_cmd;
    payload->status_code  = status_code;

    crc = proto_bin_calc_crc16(tx_buf, 6 + payload_len);
    tx_buf[6 + payload_len]     = (uint8_t)(crc & 0xFF);
    tx_buf[6 + payload_len + 1] = (uint8_t)(crc >> 8);

    return total_len;
}

/**
  * @brief  解包状态机初始化
  */
void proto_bin_parser_init(proto_bin_parser_t *parser)
{
    if (parser) {
        memset(parser, 0, sizeof(proto_bin_parser_t));
    }
}

/**
  * @brief  流解析状态机 (ARMCC V5 C90 兼容)
  */
bool proto_bin_parse_byte(proto_bin_parser_t *p, uint8_t b)
{
    uint8_t header_tmp[6];
    uint16_t crc_val;
    uint16_t rx_crc;
    int i, j;

    if (!p) return false;

    switch (p->state) {
        case 0:
            if (b == PROTO_BIN_HEADER_HIGH) p->state = 1;
            break;

        case 1:
            if (b == PROTO_BIN_HEADER_LOW) {
                p->state = 2;
            } else {
                p->state = (b == PROTO_BIN_HEADER_HIGH) ? 1 : 0;
            }
            break;

        case 2:
            p->cmd_id = b;
            p->state = 3;
            break;

        case 3:
            p->seq = b;
            p->state = 4;
            break;

        case 4:
            p->rx_len = ((uint16_t)b) << 8;
            p->state = 5;
            break;

        case 5:
            p->rx_len |= b;
            p->rx_cnt = 0;
            if (p->rx_len > sizeof(p->payload_buf)) {
                p->state = 0;
            } else if (p->rx_len == 0) {
                p->state = 7;
            } else {
                p->state = 6;
            }
            break;

        case 6:
            p->payload_buf[p->rx_cnt++] = b;
            if (p->rx_cnt >= p->rx_len) {
                p->state = 7;
            }
            break;

        case 7:
            p->payload_buf[p->rx_cnt] = b;
            p->state = 8;
            break;

        case 8:
            p->state = 0;

            header_tmp[0] = PROTO_BIN_HEADER_HIGH;
            header_tmp[1] = PROTO_BIN_HEADER_LOW;
            header_tmp[2] = p->cmd_id;
            header_tmp[3] = p->seq;
            header_tmp[4] = (uint8_t)(p->rx_len >> 8);
            header_tmp[5] = (uint8_t)(p->rx_len & 0xFF);

            crc_val = 0xFFFF;
            for (i = 0; i < 6; i++) {
                crc_val ^= header_tmp[i];
                for (j = 0; j < 8; j++) {
                    crc_val = (crc_val & 1) ? ((crc_val >> 1) ^ 0xA001) : (crc_val >> 1);
                }
            }
            for (i = 0; i < (int)p->rx_len; i++) {
                crc_val ^= p->payload_buf[i];
                for (j = 0; j < 8; j++) {
                    crc_val = (crc_val & 1) ? ((crc_val >> 1) ^ 0xA001) : (crc_val >> 1);
                }
            }

            rx_crc = ((uint16_t)b << 8) | p->payload_buf[p->rx_cnt];
            if (crc_val == rx_crc) {
                return true;
            }
            break;

        default:
            p->state = 0;
            break;
    }
    return false;
}

/**
  * @brief  处理接收到的完整二进制命令帧
  */
void proto_bin_process_frame(uint8_t cmd_id, uint8_t seq, const uint8_t *payload, uint16_t len, uint8_t *ack_buf, uint32_t *ack_len)
{
    uint8_t status_code = 0;
    proto_bin_set_mode_payload_t *set_mode;
    proto_bin_set_pid_payload_t *pid_cmd;

    switch (cmd_id) {
        case CMD_BIN_SET_MODE:
            if (len >= sizeof(proto_bin_set_mode_payload_t)) {
                set_mode = (proto_bin_set_mode_payload_t *)payload;
                if (set_mode->target_mode >= 1 && set_mode->target_mode <= 3) {
                    app_dyno_set_mode((dyno_mode_t)set_mode->target_mode, set_mode->target_value);
                } else {
                    status_code = 1;
                }
            } else {
                status_code = 3;
            }
            break;

        case CMD_BIN_ESTOP:
            app_dyno_emergency_stop();
            break;

        case CMD_BIN_SET_PID:
            if (len >= sizeof(proto_bin_set_pid_payload_t)) {
                pid_cmd = (proto_bin_set_pid_payload_t *)payload;
                app_dyno_set_pid_config(pid_cmd->loop_type, pid_cmd->kp, pid_cmd->ki, pid_cmd->kd, pid_cmd->max_dac);
            } else {
                status_code = 3;
            }
            break;

        default:
            status_code = 1;
            break;
    }

    if (ack_buf && ack_len) {
        *ack_len = proto_bin_pack_ack(seq, cmd_id, status_code, ack_buf, *ack_len);
    }
}
