/*!
    \file    bsp_sdram.c
    \brief   Driver implementation for W9825G6KH-6I SDRAM on GD32F470 (LiangShanPi)
*/

#include "bsp_sdram.h"
#include <stdio.h>
#include <string.h>

/* 私有函数声明 */
static void sdram_gpio_config(void);
static void sdram_delay_us(uint32_t us);

/**
 * @brief  微秒级简单阻塞延时 (基于主频 240MHz 计算)
 */
static void sdram_delay_us(uint32_t us)
{
    /* GD32F470 @ 240MHz: 大约 40 个循环周期对应 1us */
    volatile uint32_t count = us * 40U;
    while (count--) {
        __NOP();
    }
}

/**
 * @brief  SDRAM 关联的所有 GPIO 引脚初始化
 * @note   原理图连接关系:
 *         地址线 A0~A12: PF0~PF5, PF12~PF15, PG0~PG2
 *         数据线 D0~D15: PD14, PD15, PD0, PD1, PE7~PE15, PD8~PD10
 *         控制线:
 *           PE0  -> SDR_NBL0 (LDQM)
 *           PE1  -> SDR_NBL1 (UDQM)
 *           PC3  -> SDR_CKE0
 *           PC2  -> SDR_NE0  (CS#)
 *           PC0  -> SDR_NWE  (WE#)
 *           PF11 -> SDR_NRAS (RAS#)
 *           PG15 -> SDR_NCAS (CAS#)
 *           PG4  -> SDR_BA0
 *           PG5  -> SDR_BA1
 *           PG8  -> SDR_CLK
 *         全部复用为 GPIO_AF_12
 */
static void sdram_gpio_config(void)
{
    uint32_t gpioc_pins;
    uint32_t gpiod_pins;
    uint32_t gpioe_pins;
    uint32_t gpiof_pins;
    uint32_t gpiog_pins;

    /* 1. 使能各 GPIO 端口时钟与 EXMC 时钟 */
    rcu_periph_clock_enable(RCU_GPIOC);
    rcu_periph_clock_enable(RCU_GPIOD);
    rcu_periph_clock_enable(RCU_GPIOE);
    rcu_periph_clock_enable(RCU_GPIOF);
    rcu_periph_clock_enable(RCU_GPIOG);
    rcu_periph_clock_enable(RCU_EXMC);

    /* ---------------- GPIOC 引脚配置 ---------------- */
    /* PC0: SDR_NWE, PC2: SDR_NE0, PC3: SDR_CKE0 */
    gpioc_pins = GPIO_PIN_0 | GPIO_PIN_2 | GPIO_PIN_3;
    gpio_af_set(GPIOC, GPIO_AF_12, gpioc_pins);
    gpio_mode_set(GPIOC, GPIO_MODE_AF, GPIO_PUPD_PULLUP, gpioc_pins);
    gpio_output_options_set(GPIOC, GPIO_OTYPE_PP, GPIO_OSPEED_MAX, gpioc_pins);

    /* ---------------- GPIOD 引脚配置 ---------------- */
    /* PD0: EXMC_D2, PD1: EXMC_D3, PD8: EXMC_D13, PD9: EXMC_D14, PD10: EXMC_D15, PD14: EXMC_D0, PD15: EXMC_D1 */
    gpiod_pins = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_8 | GPIO_PIN_9 |
                 GPIO_PIN_10 | GPIO_PIN_14 | GPIO_PIN_15;
    gpio_af_set(GPIOD, GPIO_AF_12, gpiod_pins);
    gpio_mode_set(GPIOD, GPIO_MODE_AF, GPIO_PUPD_PULLUP, gpiod_pins);
    gpio_output_options_set(GPIOD, GPIO_OTYPE_PP, GPIO_OSPEED_MAX, gpiod_pins);

    /* ---------------- GPIOE 引脚配置 ---------------- */
    /* PE0: SDR_NBL0, PE1: SDR_NBL1, PE7~PE15: EXMC_D4~EXMC_D12 */
    gpioe_pins = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_7 | GPIO_PIN_8 |
                 GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12 |
                 GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
    gpio_af_set(GPIOE, GPIO_AF_12, gpioe_pins);
    gpio_mode_set(GPIOE, GPIO_MODE_AF, GPIO_PUPD_PULLUP, gpioe_pins);
    gpio_output_options_set(GPIOE, GPIO_OTYPE_PP, GPIO_OSPEED_MAX, gpioe_pins);

    /* ---------------- GPIOF 引脚配置 ---------------- */
    /* PF0~PF5: EXMC_A0~EXMC_A5, PF11: SDR_NRAS, PF12~PF15: EXMC_A6~EXMC_A9 */
    gpiof_pins = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 |
                 GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_11 | GPIO_PIN_12 |
                 GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
    gpio_af_set(GPIOF, GPIO_AF_12, gpiof_pins);
    gpio_mode_set(GPIOF, GPIO_MODE_AF, GPIO_PUPD_PULLUP, gpiof_pins);
    gpio_output_options_set(GPIOF, GPIO_OTYPE_PP, GPIO_OSPEED_MAX, gpiof_pins);

    /* ---------------- GPIOG 引脚配置 ---------------- */
    /* PG0~PG2: EXMC_A10~EXMC_A12, PG4: SDR_BA0, PG5: SDR_BA1, PG8: SDR_CLK, PG15: SDR_NCAS */
    gpiog_pins = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_4 |
                 GPIO_PIN_5 | GPIO_PIN_8 | GPIO_PIN_15;
    gpio_af_set(GPIOG, GPIO_AF_12, gpiog_pins);
    gpio_mode_set(GPIOG, GPIO_MODE_AF, GPIO_PUPD_PULLUP, gpiog_pins);
    gpio_output_options_set(GPIOG, GPIO_OTYPE_PP, GPIO_OSPEED_MAX, gpiog_pins);
}

/**
 * @brief  初始化 SDRAM (引脚配置、EXMC控制器、时序以及初始化序列)
 */
void bsp_sdram_init(void)
{
    exmc_sdram_parameter_struct         sdram_init_struct;
    exmc_sdram_timing_parameter_struct  sdram_timing_init_struct;
    exmc_sdram_command_parameter_struct sdram_command_init_struct;
    uint32_t ahb_clk;
    uint32_t sdclk_freq;
    uint32_t refresh_count;

    /* 1. 配置 GPIO 引脚 */
    sdram_gpio_config();

    /* 2. 配置 EXMC SDRAM 时序参数
     * HCLK = 240MHz, SDCLK = HCLK / 2 = 120MHz (tCK ≈ 8.33ns)
     * W9825G6KH-6I 时序要求:
     * - tRCD (RAS to CAS delay): min 18ns => 18 / 8.33 = 2.16 => 3 SDCLK
     * - tRP  (Row Precharge time): min 18ns => 18 / 8.33 = 2.16 => 3 SDCLK
     * - tWR  (Write Recovery time): 2 SDCLK
     * - tRC  (Auto refresh delay): min 60ns => 60 / 8.33 = 7.2 => 8 SDCLK
     * - tRAS (Row Address Select): min 42ns => 42 / 8.33 = 5.04 => 6 SDCLK
     * - tXSR (Exit Self-Refresh): min 72ns => 72 / 8.33 = 8.64 => 9 SDCLK
     * - tMRD (Load Mode Register delay): 2 SDCLK
     */
    sdram_timing_init_struct.row_to_column_delay        = 3U; /* tRCD */
    sdram_timing_init_struct.row_precharge_delay        = 3U; /* tRP */
    sdram_timing_init_struct.write_recovery_delay       = 2U; /* tWR */
    sdram_timing_init_struct.auto_refresh_delay         = 8U; /* tRC / tRFC */
    sdram_timing_init_struct.row_address_select_delay   = 6U; /* tRAS */
    sdram_timing_init_struct.exit_selfrefresh_delay     = 9U; /* tXSR */
    sdram_timing_init_struct.load_mode_register_delay   = 2U; /* tMRD */

    /* 3. 配置 EXMC SDRAM 控制器结构体 */
    sdram_init_struct.sdram_device          = EXMC_SDRAM_DEVICE0;
    sdram_init_struct.column_address_width  = EXMC_SDRAM_COW_ADDRESS_9;     /* 9位列地址 (A0~A8) */
    sdram_init_struct.row_address_width     = EXMC_SDRAM_ROW_ADDRESS_13;    /* 13位行地址 (A0~A12) */
    sdram_init_struct.data_width            = EXMC_SDRAM_DATABUS_WIDTH_16B; /* 16位数据宽度 */
    sdram_init_struct.internal_bank_number  = EXMC_SDRAM_4_INTER_BANK;      /* 4个内部 Bank (BA0, BA1) */
    sdram_init_struct.cas_latency           = EXMC_CAS_LATENCY_3_SDCLK;     /* CAS 延时 = 3 */
    sdram_init_struct.write_protection      = DISABLE;                      /* 关闭写保护 */
    sdram_init_struct.sdclock_config        = EXMC_SDCLK_PERIODS_2_HCLK;    /* SDCLK = HCLK / 2 */
    sdram_init_struct.burst_read_switch     = ENABLE;                       /* 使能突发读 */
    sdram_init_struct.pipeline_read_delay   = EXMC_PIPELINE_DELAY_1_HCLK;   /* 流水线延时 1 HCLK */
    sdram_init_struct.timing                = &sdram_timing_init_struct;

    exmc_sdram_init(&sdram_init_struct);

    /* 4. SDRAM 标准上电初始化流程序列 */

    /* Step 4.1: 使能时钟 (Clock Enable) 命令 */
    sdram_command_init_struct.command               = EXMC_SDRAM_CLOCK_ENABLE;
    sdram_command_init_struct.bank_select           = EXMC_SDRAM_DEVICE0_SELECT;
    sdram_command_init_struct.auto_refresh_number   = EXMC_SDRAM_AUTO_REFLESH_1_SDCLK;
    sdram_command_init_struct.mode_register_content = 0U;
    exmc_sdram_command_config(&sdram_command_init_struct);

    /* 等待时钟稳定至少 200us */
    sdram_delay_us(300U);

    /* Step 4.2: 预充电所有 Bank (Precharge All) 命令 */
    sdram_command_init_struct.command               = EXMC_SDRAM_PRECHARGE_ALL;
    sdram_command_init_struct.bank_select           = EXMC_SDRAM_DEVICE0_SELECT;
    sdram_command_init_struct.auto_refresh_number   = EXMC_SDRAM_AUTO_REFLESH_1_SDCLK;
    sdram_command_init_struct.mode_register_content = 0U;
    exmc_sdram_command_config(&sdram_command_init_struct);

    /* Step 4.3: 发送连续 8 次自动刷新 (Auto Refresh) 命令 */
    sdram_command_init_struct.command               = EXMC_SDRAM_AUTO_REFRESH;
    sdram_command_init_struct.bank_select           = EXMC_SDRAM_DEVICE0_SELECT;
    sdram_command_init_struct.auto_refresh_number   = EXMC_SDRAM_AUTO_REFLESH_8_SDCLK;
    sdram_command_init_struct.mode_register_content = 0U;
    exmc_sdram_command_config(&sdram_command_init_struct);

    /* Step 4.4: 配置并写入模式寄存器 (Load Mode Register) 命令 */
    /* 突发长度: 1, 突发类型: 连续, CAS延迟: 3, 标准操作模式, 单次写模式 */
    sdram_command_init_struct.command               = EXMC_SDRAM_LOAD_MODE_REGISTER;
    sdram_command_init_struct.bank_select           = EXMC_SDRAM_DEVICE0_SELECT;
    sdram_command_init_struct.auto_refresh_number   = EXMC_SDRAM_AUTO_REFLESH_1_SDCLK;
    sdram_command_init_struct.mode_register_content = (uint32_t)(SDRAM_MODEREG_BURST_LENGTH_1 |
                                                                SDRAM_MODEREG_BURST_TYPE_SEQUENTIAL |
                                                                SDRAM_MODEREG_CAS_LATENCY_3 |
                                                                SDRAM_MODEREG_OPERATING_MODE_STANDARD |
                                                                SDRAM_MODEREG_WRITEBURST_MODE_SINGLE);
    exmc_sdram_command_config(&sdram_command_init_struct);

    /* Step 4.5: 配置自动刷新计数器 (Auto Refresh Interval)
     * 刷新要求: 64ms 内完成 8192 次刷新 => 单次刷新间隔 = 64ms / 8192 = 7.8125us
     * 刷新计数计数值公式: count = (7.8125us * SDCLK_Freq) - 20
     * 当 AHB=240MHz, SDCLK=120MHz: 7.8125 * 120 - 20 = 937.5 - 20 = 917
     * 当 AHB=200MHz, SDCLK=100MHz: 7.8125 * 100 - 20 = 781.25 - 20 = 761
     */
    ahb_clk = rcu_clock_freq_get(CK_AHB);
    sdclk_freq = ahb_clk / 2U;
    refresh_count = (uint32_t)(((uint64_t)sdclk_freq * 78125ULL) / 10000000000ULL) - 20U;
    if (refresh_count > 0x1FFFU) {
        refresh_count = 917U; /* 安全兜底 */
    }
    exmc_sdram_refresh_count_set(refresh_count);

    /* Step 4.6: 启用高速读采样延迟控制 (优化高频读信号建立时间) */
    exmc_sdram_readsample_enable(ENABLE);
    exmc_sdram_readsample_config(EXMC_SDRAM_6_DELAY_CELL, EXMC_SDRAM_READSAMPLE_0_EXTRAHCLK);
}

/**
 * @brief  向 SDRAM 写入 8位 数据缓冲区
 */
void bsp_sdram_write_buffer(const uint8_t *p_buffer, uint32_t write_addr, uint32_t num_byte)
{
    __IO uint8_t *p_sdram = (__IO uint8_t *)(SDRAM_DEVICE0_ADDR + write_addr);
    uint32_t i;
    for (i = 0; i < num_byte; i++) {
        p_sdram[i] = p_buffer[i];
    }
}

/**
 * @brief  从 SDRAM 读取 8位 数据到缓冲区
 */
void bsp_sdram_read_buffer(uint8_t *p_buffer, uint32_t read_addr, uint32_t num_byte)
{
    __IO const uint8_t *p_sdram = (__IO const uint8_t *)(SDRAM_DEVICE0_ADDR + read_addr);
    uint32_t i;
    for (i = 0; i < num_byte; i++) {
        p_buffer[i] = p_sdram[i];
    }
}

/**
 * @brief  向 SDRAM 写入 32位 数据缓冲区
 */
void bsp_sdram_write_words(const uint32_t *p_buffer, uint32_t write_addr, uint32_t num_words)
{
    __IO uint32_t *p_sdram = (__IO uint32_t *)(SDRAM_DEVICE0_ADDR + write_addr);
    uint32_t i;
    for (i = 0; i < num_words; i++) {
        p_sdram[i] = p_buffer[i];
    }
}

/**
 * @brief  从 SDRAM 读取 32位 数据到缓冲区
 */
void bsp_sdram_read_words(uint32_t *p_buffer, uint32_t read_addr, uint32_t num_words)
{
    __IO const uint32_t *p_sdram = (__IO const uint32_t *)(SDRAM_DEVICE0_ADDR + read_addr);
    uint32_t i;
    for (i = 0; i < num_words; i++) {
        p_buffer[i] = p_sdram[i];
    }
}

/**
 * @brief  对指定范围进行读写压力测试
 * @param  start_offset: 起始偏移地址 (建议4字节对齐)
 * @param  byte_length: 测试字节数
 * @return 0: 成功, 1: 失败
 */
uint8_t bsp_sdram_test_range(uint32_t start_offset, uint32_t byte_length)
{
    __IO uint32_t *p_sdram = (__IO uint32_t *)(SDRAM_DEVICE0_ADDR + start_offset);
    uint32_t word_count = byte_length / 4U;
    uint32_t i;
    uint32_t expected;
    uint32_t actual;

    /* 阶段1: 写入递增特征码 */
    for (i = 0; i < word_count; i++) {
        p_sdram[i] = (uint32_t)(0xA5000000U ^ (i * 1103515245U + 12345U));
    }

    /* 阶段2: 读回并校验 */
    for (i = 0; i < word_count; i++) {
        expected = (uint32_t)(0xA5000000U ^ (i * 1103515245U + 12345U));
        actual = p_sdram[i];
        if (actual != expected) {
            printf("[SDRAM Error] Range Test Failed @ 0x%08X: Exp=0x%08X, Act=0x%08X\r\n",
                   (unsigned int)(SDRAM_DEVICE0_ADDR + start_offset + i * 4U),
                   (unsigned int)expected, (unsigned int)actual);
            return 1U;
        }
    }

    return 0U;
}

/**
 * @brief  执行 SDRAM 全面完整性测试
 * @return 0: 测试全部通过, 非0: 发生错误的具体测试阶段代码
 */
uint8_t bsp_sdram_test(void)
{
    __IO uint16_t *p_sdram16;
    __IO uint32_t *p_sdram32;
    __IO uint8_t  *p_u8;
    __IO uint16_t *p_u16;
    __IO uint32_t *p_u32;
    uint32_t offset;
    uint32_t expected;
    uint32_t actual;
    uint32_t i;
    uint8_t bit;
    uint16_t pattern;

    printf("\r\n========================================\r\n");
    printf("         SDRAM Self-Test Start          \r\n");
    printf(" Chip: W9825G6KH-6I (32MB, 16-bit)      \r\n");
    printf(" Base Addr: 0x%08X, Size: %u MB\r\n", (unsigned int)SDRAM_DEVICE0_ADDR, (unsigned int)(SDRAM_SIZE / 1024 / 1024));
    printf("========================================\r\n");

    /* 1. 数据总线走步1测试 (Walking 1's on 16-bit Data Bus) */
    printf("[Test 1/4] Data Bus Walking 1s Test... ");
    p_sdram16 = (__IO uint16_t *)SDRAM_DEVICE0_ADDR;
    for (bit = 0; bit < 16; bit++) {
        pattern = (uint16_t)(1U << bit);
        *p_sdram16 = pattern;
        if (*p_sdram16 != pattern) {
            printf("FAILED! (Bit %u: Exp 0x%04X, Read 0x%04X)\r\n", bit, pattern, *p_sdram16);
            return 1U;
        }
    }
    printf("PASSED\r\n");

    /* 2. 地址线反相测试 (Address Bus Test) */
    printf("[Test 2/4] Address Bus Integrity Test... ");
    p_sdram32 = (__IO uint32_t *)SDRAM_DEVICE0_ADDR;
    /* 遍历各地址线引脚对应偏移 */
    for (offset = 4; offset < SDRAM_SIZE; offset <<= 1) {
        p_sdram32[offset / 4U] = (uint32_t)(0xAA550000U | offset);
    }
    for (offset = 4; offset < SDRAM_SIZE; offset <<= 1) {
        expected = (uint32_t)(0xAA550000U | offset);
        actual = p_sdram32[offset / 4U];
        if (actual != expected) {
            printf("FAILED! (@ Offset 0x%08X: Exp 0x%08X, Read 0x%08X)\r\n",
                   (unsigned int)offset, (unsigned int)expected, (unsigned int)actual);
            return 2U;
        }
    }
    printf("PASSED\r\n");

    /* 3. 混合位宽 (8-bit, 16-bit, 32-bit) 读写对齐测试 */
    printf("[Test 3/4] Mixed Data Width Alignment Test... ");
    p_u8  = (__IO uint8_t  *)SDRAM_DEVICE0_ADDR;
    p_u16 = (__IO uint16_t *)SDRAM_DEVICE0_ADDR;
    p_u32 = (__IO uint32_t *)SDRAM_DEVICE0_ADDR;

    /* 8-bit 测试 */
    for (i = 0; i < 256; i++) {
        p_u8[i] = (uint8_t)(i & 0xFFU);
    }
    for (i = 0; i < 256; i++) {
        if (p_u8[i] != (uint8_t)(i & 0xFFU)) {
            printf("8-bit FAILED @ %u\r\n", (unsigned int)i);
            return 3U;
        }
    }

    /* 16-bit 测试 */
    for (i = 0; i < 256; i++) {
        p_u16[i] = (uint16_t)(0x5A00U + i);
    }
    for (i = 0; i < 256; i++) {
        if (p_u16[i] != (uint16_t)(0x5A00U + i)) {
            printf("16-bit FAILED @ %u\r\n", (unsigned int)i);
            return 3U;
        }
    }

    /* 32-bit 测试 */
    for (i = 0; i < 256; i++) {
        p_u32[i] = (uint32_t)(0x12340000U + i);
    }
    for (i = 0; i < 256; i++) {
        if (p_u32[i] != (uint32_t)(0x12340000U + i)) {
            printf("32-bit FAILED @ %u\r\n", (unsigned int)i);
            return 3U;
        }
    }
    printf("PASSED\r\n");

    /* 4. 快速跨 Bank 4MB 压力读写测试 (前4MB全覆盖) */
    printf("[Test 4/4] 4MB Block Stress Test... ");
    if (bsp_sdram_test_range(0, 4U * 1024U * 1024U) != 0U) {
        printf("FAILED\r\n");
        return 4U;
    }
    printf("PASSED (4MB Verified OK)\r\n");

    printf("========================================\r\n");
    printf("      SDRAM All Tests PASSED! OK        \r\n");
    printf("========================================\r\n\r\n");

    return 0U;
}
