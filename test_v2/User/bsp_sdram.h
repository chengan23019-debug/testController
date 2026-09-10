/*!
    \file    bsp_sdram.h
    \brief   Header file for W9825G6KH-6I SDRAM driver on GD32F470 (LiangShanPi)
*/

#ifndef BSP_SDRAM_H
#define BSP_SDRAM_H

#include "gd32f4xx.h"
#include <stdint.h>
#include <stdbool.h>

/* ==============================================================================
 * SDRAM 芯片参数定义 (W9825G6KH-6I)
 * 容量: 256Mbit = 32MByte = 16M x 16bit
 * 组织结构: 4 Banks x 4,194,304 words x 16 bits
 * 行地址: A0 ~ A12 (13 bits)
 * 列地址: A0 ~ A8  (9 bits)
 * 数据位宽: 16 bits
 * ============================================================================== */

/* SDRAM 映射基地址 (EXMC SDRAM Device 0: 0xC0000000, Device 1: 0xD0000000) */
#define SDRAM_DEVICE0_ADDR              ((uint32_t)0xC0000000U)
#define SDRAM_SIZE                      ((uint32_t)(32U * 1024U * 1024U)) /* 32 MBytes */

/* SDRAM 模式寄存器 (Mode Register) 位定义 */
#define SDRAM_MODEREG_BURST_LENGTH_1             ((uint16_t)0x0000U)
#define SDRAM_MODEREG_BURST_LENGTH_2             ((uint16_t)0x0001U)
#define SDRAM_MODEREG_BURST_LENGTH_4             ((uint16_t)0x0002U)
#define SDRAM_MODEREG_BURST_LENGTH_8             ((uint16_t)0x0003U)

#define SDRAM_MODEREG_BURST_TYPE_SEQUENTIAL      ((uint16_t)0x0000U)
#define SDRAM_MODEREG_BURST_TYPE_INTERLEAVED     ((uint16_t)0x0008U)

#define SDRAM_MODEREG_CAS_LATENCY_2              ((uint16_t)0x0020U)
#define SDRAM_MODEREG_CAS_LATENCY_3              ((uint16_t)0x0030U)

#define SDRAM_MODEREG_OPERATING_MODE_STANDARD    ((uint16_t)0x0000U)

#define SDRAM_MODEREG_WRITEBURST_MODE_PROGRAMMED ((uint16_t)0x0000U)
#define SDRAM_MODEREG_WRITEBURST_MODE_SINGLE     ((uint16_t)0x0200U)

/* ==============================================================================
 * 函数声明
 * ============================================================================== */

/**
 * @brief  初始化 SDRAM (引脚配置、EXMC控制器、时序以及初始化序列)
 */
void bsp_sdram_init(void);

/**
 * @brief  向 SDRAM 写入 8位 数据缓冲区
 * @param  p_buffer: 待写入数据指针
 * @param  write_addr: 写入目标偏移地址 (相对 0xC0000000)
 * @param  num_byte: 写入字节数
 */
void bsp_sdram_write_buffer(const uint8_t *p_buffer, uint32_t write_addr, uint32_t num_byte);

/**
 * @brief  从 SDRAM 读取 8位 数据到缓冲区
 * @param  p_buffer: 读取数据存放指针
 * @param  read_addr: 读取源偏移地址 (相对 0xC0000000)
 * @param  num_byte: 读取字节数
 */
void bsp_sdram_read_buffer(uint8_t *p_buffer, uint32_t read_addr, uint32_t num_byte);

/**
 * @brief  向 SDRAM 写入 32位 数据缓冲区
 * @param  p_buffer: 待写入数据指针
 * @param  write_addr: 写入目标偏移地址 (字节偏移，需4字节对齐)
 * @param  num_words: 写入32位字数
 */
void bsp_sdram_write_words(const uint32_t *p_buffer, uint32_t write_addr, uint32_t num_words);

/**
 * @brief  从 SDRAM 读取 32位 数据到缓冲区
 * @param  p_buffer: 读取数据存放指针
 * @param  read_addr: 读取源偏移地址 (字节偏移，需4字节对齐)
 * @param  num_words: 读取32位字数
 */
void bsp_sdram_read_words(uint32_t *p_buffer, uint32_t read_addr, uint32_t num_words);

/**
 * @brief  执行 SDRAM 完整性测试 (数据线走步、地址线、不同位宽读写与范围测试)
 * @return 0: 测试通过, 非0: 发生错误的测试项代码
 */
uint8_t bsp_sdram_test(void);

/**
 * @brief  对指定范围进行读写压力测试
 * @param  start_offset: 起始偏移地址
 * @param  byte_length: 测试字节数
 * @return 0: 成功, 1: 失败
 */
uint8_t bsp_sdram_test_range(uint32_t start_offset, uint32_t byte_length);

#endif /* BSP_SDRAM_H */
