/*!
    \file    bsp_jsy_mk1031.c
    \brief   JSY-MK-1031 AC/DC Energy & Power Meter Driver Implementation (Robust Polling)
    \version 2026-10-25
*/

#include "bsp_jsy_mk1031.h"
#include <string.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"

/* 接收缓冲区大小 (单帧 35~45 字节) */
#define JSY_RX_BUF_SIZE                128U

/* 私有内部变量 */
static uint8_t           sg_rx_buf[JSY_RX_BUF_SIZE];
static volatile uint16_t sg_rx_len = 0;

static jsy_mk1031_data_t sg_jsy_data;
static uint8_t           sg_consecutive_timeouts = 0;

/* 
 * ============================================================================
 * CRC-16 (Modbus RTU: 多项式 0xA001) 计算函数
 * ============================================================================
 */
uint16_t jsy_mk1031_calc_crc16(const uint8_t *data, uint16_t length)
{
    uint16_t crc = 0xFFFFU;
    uint16_t i, j;

    for (i = 0; i < length; i++) {
        crc ^= (uint16_t)data[i];
        for (j = 0; j < 8; j++) {
            if (crc & 0x0001U) {
                crc = (crc >> 1) ^ 0xA001U;
            } else {
                crc = crc >> 1;
            }
        }
    }
    return crc;
}

/* 
 * ============================================================================
 * 硬件初始化: USART1 (PD5:TX, PD6:RX, AF7)
 * ============================================================================
 */
void bsp_jsy_mk1031_init(uint32_t baudrate)
{
    /* 1. 使能 GPIOD 与 USART1 时钟 */
    rcu_periph_clock_enable(JSY_MK1031_GPIO_CLK);
    rcu_periph_clock_enable(JSY_MK1031_USART_CLK);

    /* 2. 配置 PD5 (TX) 与 PD6 (RX) 复用功能为 AF7 */
    gpio_af_set(JSY_MK1031_GPIO_PORT, JSY_MK1031_GPIO_AF, JSY_MK1031_TX_PIN | JSY_MK1031_RX_PIN);
    gpio_mode_set(JSY_MK1031_GPIO_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, JSY_MK1031_TX_PIN | JSY_MK1031_RX_PIN);
    gpio_output_options_set(JSY_MK1031_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, JSY_MK1031_TX_PIN);

    /* 3. 复位并配置 USART1 串口参数 (8-N-1) */
    usart_deinit(JSY_MK1031_USART);
    usart_baudrate_set(JSY_MK1031_USART, baudrate);
    usart_word_length_set(JSY_MK1031_USART, USART_WL_8BIT);
    usart_stop_bit_set(JSY_MK1031_USART, USART_STB_1BIT);
    usart_parity_config(JSY_MK1031_USART, USART_PM_NONE);
    usart_hardware_flow_coherence_config(JSY_MK1031_USART, USART_HCM_NONE);

    /* 使能发送与接收 */
    usart_transmit_config(JSY_MK1031_USART, USART_TRANSMIT_ENABLE);
    usart_receive_config(JSY_MK1031_USART, USART_RECEIVE_ENABLE);

    /* 4. 使能 USART1 接收中断 (RBNE) */
    nvic_irq_enable(JSY_MK1031_IRQn, JSY_MK1031_PRE_PRIORITY, JSY_MK1031_SUB_PRIORITY);
    usart_interrupt_enable(JSY_MK1031_USART, USART_INT_RBNE);

    /* 5. 使能 USART1 */
    usart_enable(JSY_MK1031_USART);

    /* 6. 初始化测量缓存 */
    memset(&sg_jsy_data, 0, sizeof(sg_jsy_data));
    sg_jsy_data.is_online = false;
    sg_consecutive_timeouts = 0;
}

/* 
 * ============================================================================
 * 底层发送辅助函数 (带超时计数，绝对不挂死)
 * ============================================================================
 */
static void jsy_usart_send_bytes(const uint8_t *data, uint16_t len)
{
    uint16_t i;
    uint32_t timeout;

    /* 清除发送完成标志 */
    usart_flag_clear(JSY_MK1031_USART, USART_FLAG_TC);

    for (i = 0; i < len; i++) {
        /* 1. 等待发送缓冲区为空 (TBE) */
        timeout = 2000000U; /* 240MHz 主频下约 10~15ms 充足超时 */
        while ((RESET == usart_flag_get(JSY_MK1031_USART, USART_FLAG_TBE)) && (timeout > 0)) {
            timeout--;
        }
        /* 2. 写入数据寄存器 */
        usart_data_transmit(JSY_MK1031_USART, (uint8_t)data[i]);
    }

    /* 3. 等待所有字节物理引脚完全移出 (TC) */
    timeout = 2000000U;
    while ((RESET == usart_flag_get(JSY_MK1031_USART, USART_FLAG_TC)) && (timeout > 0)) {
        timeout--;
    }
}

/* 
 * ============================================================================
 * USART1 中断服务回调实现 (接收单字节，极简可靠)
 * ============================================================================
 */
void jsy_mk1031_uart_isr(void)
{
    /* 1. 溢出错误 (ORERR) 保护清理 */
    if (RESET != usart_flag_get(JSY_MK1031_USART, USART_FLAG_ORERR)) {
        (void)USART_STAT0(JSY_MK1031_USART);
        (void)USART_DATA(JSY_MK1031_USART);
    }

    /* 2. 接收字符中断 (RBNE) */
    if (RESET != usart_interrupt_flag_get(JSY_MK1031_USART, USART_INT_FLAG_RBNE)) {
        uint8_t ch = (uint8_t)usart_data_receive(JSY_MK1031_USART);
        if (sg_rx_len < JSY_RX_BUF_SIZE) {
            sg_rx_buf[sg_rx_len++] = ch;
        }
    }
}

/* 
 * ============================================================================
 * 同步轮询一次 JSY-MK-1031 采样数据 (0x0048 开始读 15 个寄存器)
 * ============================================================================
 */
bool jsy_mk1031_poll_sync(uint8_t slave_addr, uint32_t timeout_ms)
{
    uint8_t  tx_buf[8];
    uint16_t tx_crc;
    uint16_t rx_crc, calc_crc;
    uint8_t  expected_data_len;
    uint16_t expected_total_len;
    uint16_t reg_count = JSY_READ_REG_COUNT_STANDARD; /* 读 15 个寄存器 (30字节数据) */

    uint32_t wait_ticks;
    uint16_t last_len = 0;
    uint32_t silence_ticks = 0;

    uint16_t raw_u;
    uint32_t raw_i;
    uint32_t raw_p;
    uint32_t raw_e;
    int16_t  raw_pf;
    uint16_t raw_f;

    /* 1. 组装 Modbus 查询帧: 01 03 00 48 00 0F [CRC_L] [CRC_H] */
    tx_buf[0] = slave_addr;
    tx_buf[1] = MODBUS_FC_READ_HOLDING_REGS;
    tx_buf[2] = (uint8_t)(JSY_REG_VOLTAGE >> 8);
    tx_buf[3] = (uint8_t)(JSY_REG_VOLTAGE & 0xFFU);
    tx_buf[4] = (uint8_t)(reg_count >> 8);
    tx_buf[5] = (uint8_t)(reg_count & 0xFFU);

    tx_crc = jsy_mk1031_calc_crc16(tx_buf, 6);
    tx_buf[6] = (uint8_t)(tx_crc & 0xFFU);        /* CRC 低字节在前 */
    tx_buf[7] = (uint8_t)((tx_crc >> 8) & 0xFFU); /* CRC 高字节在后 */

    /* 2. 清空接收缓冲区计数 */
    sg_rx_len = 0;

    /* 3. 发送 Modbus 查询指令 */
    jsy_usart_send_bytes(tx_buf, 8);

    /* 4. 等待应答接收：收到 35 字节或静默超时 */
    wait_ticks = timeout_ms;
    while (wait_ticks > 0) {
        if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
            vTaskDelay(pdMS_TO_TICKS(1));
        } else {
            volatile uint32_t wait_cnt = 20000U;
            while (wait_cnt--) { __NOP(); }
        }
        wait_ticks--;

        if (sg_rx_len > 0) {
            if (sg_rx_len == last_len) {
                silence_ticks++;
                /* 静默超过 10ms 且至少收到了 5 字节，认为报文发送完毕 */
                if (silence_ticks >= 10 && sg_rx_len >= 5) {
                    break;
                }
            } else {
                last_len = sg_rx_len;
                silence_ticks = 0;
                /* 已收满 35 字节标准响应帧，直接跳出 */
                if (sg_rx_len >= 35) {
                    break;
                }
            }
        }
    }

    /* 5. 超时处理 */
    if (sg_rx_len < 5) {
        taskENTER_CRITICAL();
        sg_jsy_data.timeout_count++;
        sg_consecutive_timeouts++;
        if (sg_consecutive_timeouts >= 3) {
            sg_jsy_data.is_online = false;
        }
        taskEXIT_CRITICAL();
        return false;
    }

    /* 6. 校验地址与功能码 */
    if (sg_rx_buf[0] != slave_addr || sg_rx_buf[1] != MODBUS_FC_READ_HOLDING_REGS) {
        return false;
    }

    /* 7. CRC 校验 */
    expected_data_len = sg_rx_buf[2];
    expected_total_len = (uint16_t)expected_data_len + 5U; /* 站号(1) + 功能码(1) + 字节数(1) + 数据(N) + CRC(2) */
    if (sg_rx_len < expected_total_len) {
        return false;
    }

    calc_crc = jsy_mk1031_calc_crc16(sg_rx_buf, expected_total_len - 2U);
    rx_crc = ((uint16_t)sg_rx_buf[expected_total_len - 1U] << 8) | sg_rx_buf[expected_total_len - 2U];
    if (calc_crc != rx_crc) {
        taskENTER_CRITICAL();
        sg_jsy_data.crc_error_count++;
        taskEXIT_CRITICAL();
        return false;
    }

    /* 8. 数据解包与物理单位转换 (大端字节序 Big-Endian) */
    raw_u   = ((uint16_t)sg_rx_buf[3] << 8) | sg_rx_buf[4];
    raw_i   = ((uint32_t)sg_rx_buf[5] << 24) | ((uint32_t)sg_rx_buf[6] << 16) |
              ((uint32_t)sg_rx_buf[7] << 8)  | sg_rx_buf[8];
    raw_p   = ((uint32_t)sg_rx_buf[9] << 24) | ((uint32_t)sg_rx_buf[10] << 16) |
              ((uint32_t)sg_rx_buf[11] << 8) | sg_rx_buf[12];
    raw_e   = ((uint32_t)sg_rx_buf[13] << 24) | ((uint32_t)sg_rx_buf[14] << 16) |
              ((uint32_t)sg_rx_buf[15] << 8) | sg_rx_buf[16];
    raw_pf  = (int16_t)(((uint16_t)sg_rx_buf[17] << 8) | sg_rx_buf[18]);
    raw_f   = ((uint16_t)sg_rx_buf[19] << 8) | sg_rx_buf[20];

    /* 9. 原子更新内部测量数据 */
    taskENTER_CRITICAL();

    sg_jsy_data.voltage         = (float)raw_u / 100.0f;           /* 0.01V 分辨率 */
    sg_jsy_data.current         = (float)raw_i / 10000.0f;         /* 0.0001A 分辨率 */
    sg_jsy_data.active_power_kw = (float)raw_p / 10000.0f;         /* 0.0001kW 分辨率 */
    sg_jsy_data.active_power    = sg_jsy_data.active_power_kw * 1000.0f; /* 换算为 W */
    sg_jsy_data.active_energy   = (float)raw_e / 100.0f;           /* 0.01kWh 分辨率 */
    sg_jsy_data.power_factor    = (float)raw_pf / 1000.0f;         /* 0.001 分辨率 */
    sg_jsy_data.frequency       = (float)raw_f / 100.0f;           /* 0.01Hz 分辨率 */

    sg_jsy_data.rx_success_count++;
    sg_jsy_data.is_online = true;
    sg_consecutive_timeouts = 0;
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        sg_jsy_data.last_update_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
    }

    taskEXIT_CRITICAL();

    return true;
}

/* 
 * ============================================================================
 * 线程安全获取最新测量数据快照
 * ============================================================================
 */
void jsy_mk1031_get_data(jsy_mk1031_data_t *out_data)
{
    if (out_data == NULL) {
        return;
    }

    taskENTER_CRITICAL();
    memcpy(out_data, &sg_jsy_data, sizeof(jsy_mk1031_data_t));
    taskEXIT_CRITICAL();
}

/* 
 * ============================================================================
 * FreeRTOS 异步后台采样轮询任务 (推荐以 10Hz/100ms 周期运行)
 * ============================================================================
 */
void app_task_jsy_mk1031(void *pvParameters)
{
    uint8_t slave_addr = JSY_MK1031_DEFAULT_ADDR;
    uint32_t print_cnt = 0;
    jsy_mk1031_data_t cur_data;
    bool poll_ret;
    (void)pvParameters;
    (void)poll_ret;

    printf("[JSY-1031] Task Started and Running!\r\n");

    for (;;) {
        /* 单次轮询: 最大等待 120ms */
        poll_ret = jsy_mk1031_poll_sync(slave_addr, 120);

        /* 首次立即打印，之后每 5 次循环 (约 1 秒) 打印一次 */
        if (print_cnt == 0 || print_cnt >= 5U) {
            print_cnt = 1U;
            jsy_mk1031_get_data(&cur_data);

            if (cur_data.is_online) {
                printf("[JSY-1031] ONLINE  | U: %.2f V | I: %.4f A | P: %.1f W | E: %.2f kWh | Freq: %.2f Hz | PF: %.3f (RxOK: %u)\r\n",
                       cur_data.voltage,
                       cur_data.current,
                       cur_data.active_power,
                       cur_data.active_energy,
                       cur_data.frequency,
                       cur_data.power_factor,
                       cur_data.rx_success_count);
            } else {
                if (sg_rx_len > 0) {
                    printf("[JSY-1031] RECV_ERR | Received %u bytes (CRC Err: %u) | First bytes: %02X %02X %02X\r\n",
                           sg_rx_len, cur_data.crc_error_count, sg_rx_buf[0], sg_rx_buf[1], sg_rx_buf[2]);
                } else {
                    printf("[JSY-1031] OFFLINE  | Timeout: %u (No reply) -> Check PD5(TX), PD6(RX), 5V, GND\r\n",
                           cur_data.timeout_count);
                }
            }
        } else {
            print_cnt++;
        }

        /* 轮询间隔延时 100ms */
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/* 
 * ============================================================================
 * 启动 JSY-MK-1031 采样任务 API (分配 1024 字充足堆栈)
 * ============================================================================
 */
void jsy_mk1031_create_task(uint32_t priority)
{
    BaseType_t ret;
    ret = xTaskCreate(app_task_jsy_mk1031, "JSY_MK1031", 1024, NULL, priority, NULL);
    if (ret != pdPASS) {
        printf("[ERROR] Failed to create JSY_MK1031 Task! ret = %d (Free Heap: %u bytes)\r\n", 
               (int)ret, (unsigned int)xPortGetFreeHeapSize());
    } else {
        printf("JSY_MK1031 Task Created (Stack: 1024, Priority: %u, Free Heap: %u bytes)\r\n", 
               (unsigned int)priority, (unsigned int)xPortGetFreeHeapSize());
    }
}
