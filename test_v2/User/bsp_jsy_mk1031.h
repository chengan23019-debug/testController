/*!
    \file    bsp_jsy_mk1031.h
    \brief   JSY-MK-1031 AC/DC Energy & Power Meter Driver for GD32F470 (LiangShanPi)
    \version 2026-10-25
*/

#ifndef BSP_JSY_MK1031_H
#define BSP_JSY_MK1031_H

#include "gd32f4xx.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 
 * ============================================================================
 * JSY-MK-1031 硬件接口定义 (GD32F470ZGT6 立创·梁山派)
 * 通信接口: USART1 (PD5: TX, PD6: RX), AF7 复用
 * 模块为原生 TTL 电平 (3.3V~5V 兼容)，直接与 MCU 串口相连:
 *   MCU PD5 (TX)  -->  接模块对应的通信引脚
 *   MCU PD6 (RX)  <--  接模块对应的通信引脚
 *   MCU 5V        -->  JSY-MK-1031 VCC (模块标称5V供电)
 *   MCU GND       -->  JSY-MK-1031 GND
 * ============================================================================
 */
#define JSY_MK1031_USART               USART1
#define JSY_MK1031_USART_CLK           RCU_USART1
#define JSY_MK1031_GPIO_CLK            RCU_GPIOD
#define JSY_MK1031_GPIO_PORT           GPIOD
#define JSY_MK1031_TX_PIN              GPIO_PIN_5
#define JSY_MK1031_RX_PIN              GPIO_PIN_6
#define JSY_MK1031_GPIO_AF             GPIO_AF_7
#define JSY_MK1031_IRQn                USART1_IRQn
#define JSY_MK1031_IRQHandler          USART1_IRQHandler

/* 串口中断优先级配置 (需符合 FreeRTOS 系统调用要求: 抢占优先级 >= 5) */
#define JSY_MK1031_PRE_PRIORITY        6
#define JSY_MK1031_SUB_PRIORITY        0

/* 
 * ============================================================================
 * Modbus-RTU 通信参数与寄存器地址定义
 * ============================================================================
 */
#define JSY_MK1031_DEFAULT_ADDR        0x01U      /* 默认从机地址 */
#define JSY_MK1031_DEFAULT_BAUD        9600U      /* 默认波特率 9600 8-N-1 */

/* Modbus 功能码 */
#define MODBUS_FC_READ_HOLDING_REGS    0x03U      /* 读保持寄存器 */
#define MODBUS_FC_WRITE_MULTIPLE_REGS  0x10U      /* 写多个寄存器 */

/* 核心寄存器起始地址定义 */
#define JSY_REG_MODEL_1                0x0000U    /* 型号1 (只读 0x1031) */
#define JSY_REG_MODEL_2                0x0001U    /* 型号2 (高字节: 01H交流, 02H直流) */
#define JSY_REG_VOLT_RANGE             0x0002U    /* 额定电压规格 (如 250V) */
#define JSY_REG_CURR_RANGE             0x0003U    /* 额定电流规格 (如 16A) */
#define JSY_REG_SYS_COMM_CFG           0x0004U    /* 通信地址与波特率设置 */
#define JSY_REG_SIG_SRC_CFG            0x0005U    /* 信号源选择 (写 0x0001 为自动切换) */

#define JSY_REG_VOLTAGE                0x0048U    /* 电压 (1寄存器, /100 -> V) */
#define JSY_REG_CURRENT                0x0049U    /* 电流 (2寄存器 32位, /10000 -> A) */
#define JSY_REG_ACTIVE_PWR             0x004BU    /* 有功功率 (2寄存器 32位, /10 -> W) */
#define JSY_REG_ENERGY                 0x004DU    /* 有功总电能 (2寄存器 32位, /100 -> kWh) */
#define JSY_REG_POWER_FACTOR           0x004FU    /* 功率因数 (1寄存器, /1000) */
#define JSY_REG_FREQUENCY              0x0050U    /* 频率 (1寄存器, /100 -> Hz) */
#define JSY_REG_APPARENT_PWR           0x0057U    /* 视在功率 (2寄存器 32位, /10 -> VA) */
#define JSY_REG_REACTIVE_PWR           0x0059U    /* 无功功率 (2寄存器 32位, /10 -> var) */
#define JSY_REG_PHASE_ANGLE            0x005BU    /* 相位角 (1寄存器, /100 -> 度) */

/* 遥测采集寄存器数量定义 (从 0x0048 开始) */
#define JSY_READ_REG_COUNT_STANDARD    15U        /* 标准遥测: 读 15 个寄存器 (0x0048~0x0056) */
#define JSY_READ_REG_COUNT_FULL        20U        /* 全量遥测: 读 20 个寄存器 (0x0048~0x005B) */

/* 
 * ============================================================================
 * JSY-MK-1031 实时解析数据结构体
 * ============================================================================
 */
typedef struct {
    float    voltage;            /* 电压有效值 (V) */
    float    current;            /* 电流有效值 (A) */
    float    active_power;       /* 有功功率 (W) */
    float    active_power_kw;    /* 有功功率 (kW) */
    float    active_energy;      /* 累计有功电能 (kWh) */
    float    power_factor;       /* 功率因数 (-1.000 ~ +1.000) */
    float    frequency;          /* 电网/交流频率 (Hz) */
    float    apparent_power;     /* 视在功率 (VA) */
    float    reactive_power;     /* 无功功率 (var) */
    float    phase_angle;        /* 相位角 (度 °) */

    uint32_t rx_success_count;   /* 成功应答次数 */
    uint32_t crc_error_count;    /* CRC 校验错误次数 */
    uint32_t timeout_count;      /* 超时无应答次数 */
    uint32_t last_update_ms;     /* 上次成功更新的时间戳 (ms) */
    bool     is_online;          /* 模块在线通信状态 */
} jsy_mk1031_data_t;

/* 
 * ============================================================================
 * 驱动与控制 API 函数原型
 * ============================================================================
 */

/*!
    \brief      初始化 JSY-MK-1031 硬件串口与接收中断
    \param[in]  baudrate: 串口波特率 (默认 9600)
    \retval     none
*/
void bsp_jsy_mk1031_init(uint32_t baudrate);

/*!
    \brief      同步执行一次 Modbus-RTU 采样轮询 (可在后台任务中调用)
    \param[in]  slave_addr: 从机地址 (默认 0x01)
    \param[in]  timeout_ms: 最大等待超时时间 (ms, 建议 150~250)
    \retval     bool: true 成功解析且 CRC 正确, false 失败或超时
*/
bool jsy_mk1031_poll_sync(uint8_t slave_addr, uint32_t timeout_ms);

/*!
    \brief      获取最新解析的电能计量快照 (线程安全，支持高频 50Hz 调用)
    \param[out] out_data: 输出数据结构体指针
    \retval     none
*/
void jsy_mk1031_get_data(jsy_mk1031_data_t *out_data);

/*!
    \brief      下发清除电能命令 (向 0x004D 写入 0x00000000)
    \param[in]  slave_addr: 从机地址
    \param[in]  timeout_ms: 超时时间 (ms)
    \retval     bool: true 清除成功, false 失败
*/
bool jsy_mk1031_reset_energy(uint8_t slave_addr, uint32_t timeout_ms);

/*!
    \brief      USART1 中断服务回调函数 (在 gd32f4xx_it.c 的 USART1_IRQHandler 中调用)
    \retval     none
*/
void jsy_mk1031_uart_isr(void);

/*!
    \brief      计算 Modbus CRC16 校验码
    \param[in]  data: 数据缓冲区
    \param[in]  length: 字节长度
    \retval     uint16_t: 16位校验码 (低字节在前)
*/
uint16_t jsy_mk1031_calc_crc16(const uint8_t *data, uint16_t length);

/*!
    \brief      FreeRTOS 后台异步采样轮询任务 (推荐 10Hz/100ms 周期)
    \param[in]  pvParameters: 任务入参
*/
void app_task_jsy_mk1031(void *pvParameters);

/*!
    \brief      创建并启动 JSY-MK-1031 FreeRTOS 采样任务
    \param[in]  priority: 任务优先级 (建议 tskIDLE_PRIORITY + 2)
*/
void jsy_mk1031_create_task(uint32_t priority);

#ifdef __cplusplus
}
#endif

#endif /* BSP_JSY_MK1031_H */
