/**
  ******************************************************************************
  * @file    app_protocol_modbus.h
  * @brief   测功机 RS485 Modbus RTU 从机协议头文件
  ******************************************************************************
  */

#ifndef __APP_PROTOCOL_MODBUS_H
#define __APP_PROTOCOL_MODBUS_H

#include "gd32f4xx.h"
#include "app_dyno_global.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MODBUS_DEFAULT_SLAVE_ID     0x01

/* Modbus 功能码定义 */
#define MB_FUNC_READ_HOLDING_REG    0x03
#define MB_FUNC_READ_INPUT_REG      0x04
#define MB_FUNC_WRITE_SINGLE_REG    0x06
#define MB_FUNC_WRITE_MULTI_REG     0x10

/* 异常码 Exception Codes */
#define MB_ERR_ILLEGAL_FUNCTION     0x01
#define MB_ERR_ILLEGAL_DATA_ADDR    0x02
#define MB_ERR_ILLEGAL_DATA_VAL     0x03
#define MB_ERR_SLAVE_DEVICE_BUSY    0x06

/* 输入寄存器地址基址 (0x04) */
#define MB_REG_INPUT_START_ADDR     0x0000
#define MB_REG_INPUT_NUM_REGS       15      /* 15 个 16-bit 寄存器 */

/* 保持寄存器地址基址 (0x03/0x06/0x10) */
#define MB_REG_HOLDING_START_ADDR   0x1000
#define MB_REG_HOLDING_NUM_REGS     11      /* 11 个 16-bit 寄存器 */

/* 解析与处理接口 */
uint16_t app_modbus_rtu_calc_crc(const uint8_t *buf, uint16_t len);
bool app_modbus_rtu_process_frame(uint8_t slave_id, const uint8_t *rx_buf, uint16_t rx_len, uint8_t *tx_buf, uint16_t *tx_len);

#ifdef __cplusplus
}
#endif

#endif /* __APP_PROTOCOL_MODBUS_H */
