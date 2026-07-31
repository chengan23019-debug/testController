/**
  ******************************************************************************
  * @file    app_protocol_modbus.c
  * @brief   测功机 RS485 Modbus RTU 从机协议解析实现 (ARMCC V5 兼容)
  ******************************************************************************
  */

#include "app_protocol_modbus.h"
#include <string.h>

/**
  * @brief  计算 Modbus CRC16
  */
uint16_t app_modbus_rtu_calc_crc(const uint8_t *buf, uint16_t len)
{
    uint16_t crc = 0xFFFF;
    uint16_t pos;
    int i;

    for (pos = 0; pos < len; pos++) {
        crc ^= (uint16_t)buf[pos];
        for (i = 8; i != 0; i--) {
            if ((crc & 0x0001) != 0) {
                crc >>= 1;
                crc ^= 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

static void float_to_modbus_regs(float val, uint16_t *reg0, uint16_t *reg1)
{
    union {
        float f;
        uint32_t u32;
    } conv;
    conv.f = val;
    *reg0 = (uint16_t)(conv.u32 >> 16);
    *reg1 = (uint16_t)(conv.u32 & 0xFFFF);
}

static float modbus_regs_to_float(uint16_t reg0, uint16_t reg1)
{
    union {
        float f;
        uint32_t u32;
    } conv;
    conv.u32 = (((uint32_t)reg0) << 16) | (uint32_t)reg1;
    return conv.f;
}

/**
  * @brief  读取只读输入寄存器 (0x04)
  */
static bool handle_read_input_registers(uint16_t start_addr, uint16_t reg_count, uint8_t *tx_buf, uint16_t *tx_len)
{
    dyno_system_status_t status;
    uint16_t regs[16];
    uint8_t byte_cnt;
    uint16_t ptr;
    uint16_t i;

    app_dyno_get_status(&status);
    memset(regs, 0, sizeof(regs));

    float_to_modbus_regs(status.torque_nm,     &regs[0],  &regs[1]);
    float_to_modbus_regs(status.speed_rpm,     &regs[2],  &regs[3]);
    float_to_modbus_regs(status.mech_power_w,  &regs[4],  &regs[5]);
    float_to_modbus_regs(status.dac_voltage,   &regs[6],  &regs[7]);
    float_to_modbus_regs(status.elec_voltage,  &regs[8],  &regs[9]);
    float_to_modbus_regs(status.elec_current,  &regs[10], &regs[11]);
    float_to_modbus_regs(status.elec_power,    &regs[12], &regs[13]);
    regs[14] = status.alarm_flags;

    if (start_addr + reg_count > 15) {
        return false;
    }

    byte_cnt = reg_count * 2;
    tx_buf[2] = byte_cnt;

    ptr = 3;
    for (i = 0; i < reg_count; i++) {
        uint16_t reg_val = regs[start_addr + i];
        tx_buf[ptr++] = (uint8_t)(reg_val >> 8);
        tx_buf[ptr++] = (uint8_t)(reg_val & 0xFF);
    }

    *tx_len = ptr;
    return true;
}

/**
  * @brief  读取保持寄存器 (0x03)
  */
static bool handle_read_holding_registers(uint16_t start_addr, uint16_t reg_count, uint8_t *tx_buf, uint16_t *tx_len)
{
    uint16_t offset;
    dyno_system_status_t status;
    dyno_pid_config_t t_pid, p_pid;
    uint16_t regs[16];
    uint16_t ptr;
    uint16_t i;

    if (start_addr < MB_REG_HOLDING_START_ADDR) return false;
    offset = start_addr - MB_REG_HOLDING_START_ADDR;

    app_dyno_get_status(&status);
    app_dyno_get_pid_config(0, &t_pid);
    app_dyno_get_pid_config(1, &p_pid);

    memset(regs, 0, sizeof(regs));

    regs[0] = (uint16_t)status.mode;
    float_to_modbus_regs(status.target_value, &regs[1], &regs[2]);
    float_to_modbus_regs(t_pid.kp,            &regs[3], &regs[4]);
    float_to_modbus_regs(t_pid.ki,            &regs[5], &regs[6]);
    float_to_modbus_regs(t_pid.kd,            &regs[7], &regs[8]);
    float_to_modbus_regs(status.max_dac_limit,&regs[9], &regs[10]);

    if (offset + reg_count > 11) {
        return false;
    }

    tx_buf[2] = reg_count * 2;
    ptr = 3;
    for (i = 0; i < reg_count; i++) {
        uint16_t r = regs[offset + i];
        tx_buf[ptr++] = (uint8_t)(r >> 8);
        tx_buf[ptr++] = (uint8_t)(r & 0xFF);
    }

    *tx_len = ptr;
    return true;
}

/**
  * @brief  处理 Modbus RTU 接收数据帧
  */
bool app_modbus_rtu_process_frame(uint8_t slave_id, const uint8_t *rx_buf, uint16_t rx_len, uint8_t *tx_buf, uint16_t *tx_len)
{
    uint16_t rx_crc, calc_crc, tx_crc;
    uint8_t func_code;
    uint16_t reg_addr, reg_count;
    bool success = false;
    uint16_t val;
    uint16_t mode_val, val_r0, val_r1;
    float target_val;

    if (rx_buf == NULL || tx_buf == NULL || tx_len == NULL || rx_len < 8) {
        return false;
    }

    if (rx_buf[0] != slave_id && rx_buf[0] != 0x00) {
        return false;
    }

    rx_crc = ((uint16_t)rx_buf[rx_len - 1] << 8) | rx_buf[rx_len - 2];
    calc_crc = app_modbus_rtu_calc_crc(rx_buf, rx_len - 2);
    if (rx_crc != calc_crc) {
        return false;
    }

    func_code  = rx_buf[1];
    reg_addr  = ((uint16_t)rx_buf[2] << 8) | rx_buf[3];
    reg_count = ((uint16_t)rx_buf[4] << 8) | rx_buf[5];

    tx_buf[0] = rx_buf[0];
    tx_buf[1] = func_code;

    switch (func_code) {
        case MB_FUNC_READ_INPUT_REG:
            success = handle_read_input_registers(reg_addr, reg_count, tx_buf, tx_len);
            break;

        case MB_FUNC_READ_HOLDING_REG:
            success = handle_read_holding_registers(reg_addr, reg_count, tx_buf, tx_len);
            break;

        case MB_FUNC_WRITE_SINGLE_REG:
            if (reg_addr == MB_REG_HOLDING_START_ADDR) {
                val = reg_count;
                if (val <= 4) {
                    if (val == 4) {
                        app_dyno_emergency_stop();
                    } else {
                        app_dyno_set_mode((dyno_mode_t)val, 0.0f);
                    }
                    memcpy(tx_buf, rx_buf, 6);
                    *tx_len = 6;
                    success = true;
                }
            }
            break;

        case MB_FUNC_WRITE_MULTI_REG:
            if (reg_addr == MB_REG_HOLDING_START_ADDR && reg_count >= 3) {
                mode_val = ((uint16_t)rx_buf[7] << 8) | rx_buf[8];
                val_r0   = ((uint16_t)rx_buf[9] << 8) | rx_buf[10];
                val_r1   = ((uint16_t)rx_buf[11] << 8) | rx_buf[12];
                target_val = modbus_regs_to_float(val_r0, val_r1);

                if (mode_val <= 4) {
                    if (mode_val == 4) {
                        app_dyno_emergency_stop();
                    } else {
                        app_dyno_set_mode((dyno_mode_t)mode_val, target_val);
                    }
                    memcpy(tx_buf, rx_buf, 6);
                    *tx_len = 6;
                    success = true;
                }
            }
            break;

        default:
            break;
    }

    if (!success) {
        tx_buf[0] = rx_buf[0];
        tx_buf[1] = func_code | 0x80;
        tx_buf[2] = MB_ERR_ILLEGAL_DATA_ADDR;
        *tx_len = 3;
    }

    tx_crc = app_modbus_rtu_calc_crc(tx_buf, *tx_len);
    tx_buf[*tx_len]     = (uint8_t)(tx_crc & 0xFF);
    tx_buf[*tx_len + 1] = (uint8_t)(tx_crc >> 8);
    *tx_len += 2;

    return true;
}
