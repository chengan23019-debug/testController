/*!
    \file    bsp_timer.c
    \brief   GD32F470 Hardware Timer Input Capture M/T Speed Measurement Driver
*/

#include "bsp_timer.h"
#include "gd32f4xx_timer.h"
#include <stdio.h>

static uint16_t sg_ppr = SPEED_PULSE_PPR;
static volatile uint32_t sg_pulse_count = 0;
static volatile uint32_t sg_last_pulse_count = 0;
static volatile uint32_t sg_last_calc_time_us = 0;

/**
 * @brief  微秒级高精度系统时钟读取 (基于 DWT 周期计数器)
 */
static uint32_t get_time_us(void)
{
    /* GD32F470 Core running at 240MHz: DWT->CYCCNT / 240 */
    return (DWT->CYCCNT / 240U);
}

/**
 * @brief  初始化 DWT 纳秒/微秒高精度计数器
 */
static void dwt_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

/**
 * @brief  初始化硬件测速定时器 (TIMER1_CH0 / PA0) 与硬件数字滤波 (CH0FLT)
 */
void bsp_timer_init(void)
{
    timer_parameter_struct timer_initpara;
    timer_ic_parameter_struct timer_icinitpara;

    /* 1. 初始化 DWT 微秒高精时钟 */
    dwt_init();

    /* 2. 使能 GPIOA 与 TIMER1 时钟 */
    rcu_periph_clock_enable(SPEED_TIMER_GPIO_RCU);
    rcu_periph_clock_enable(SPEED_TIMER_RCU);

    /* 3. 配置 PA0 为复用功能 (TIMER1_CH0) */
    gpio_af_set(SPEED_TIMER_GPIO_PORT, SPEED_TIMER_AF, SPEED_TIMER_PIN);
    gpio_mode_set(SPEED_TIMER_GPIO_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, SPEED_TIMER_PIN);
    gpio_output_options_set(SPEED_TIMER_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, SPEED_TIMER_PIN);

    /* 4. 复位 TIMER1 */
    timer_deinit(SPEED_TIMER);

    /* 5. 配置 TIMER1 时基：1MHz 计数频率 (1us 分辨率)，32位重载计数 */
    timer_struct_para_init(&timer_initpara);
    timer_initpara.prescaler         = (rcu_clock_freq_get(CK_APB1) * 2 / 1000000U) - 1U;
    timer_initpara.alignedmode       = TIMER_COUNTER_EDGE;
    timer_initpara.counterdirection  = TIMER_COUNTER_UP;
    timer_initpara.period            = 0xFFFFFFFFU;
    timer_initpara.clockdivision     = TIMER_CKDIV_DIV1;
    timer_initpara.repetitioncounter = 0;
    timer_init(SPEED_TIMER, &timer_initpara);

    /* 6. 配置通道 0 硬件输入捕获与数字滤波 (CH0FLT) */
    /* filter = 8 (连续 8 个时钟采样确认，纳秒级滤除小于 500ns 的毛刺) */
    timer_channel_input_struct_para_init(&timer_icinitpara);
    timer_icinitpara.icpolarity  = TIMER_IC_POLARITY_RISING;
    timer_icinitpara.icselection = TIMER_IC_SELECTION_DIRECTTI;
    timer_icinitpara.icprescaler = TIMER_IC_PSC_DIV1;
    timer_icinitpara.icfilter    = 8;
    timer_input_capture_config(SPEED_TIMER, SPEED_TIMER_CHANNEL, &timer_icinitpara);

    /* 7. 使能输入捕获中断 */
    timer_interrupt_flag_clear(SPEED_TIMER, TIMER_INT_CH0);
    timer_interrupt_enable(SPEED_TIMER, TIMER_INT_CH0);
    nvic_irq_enable(SPEED_TIMER_IRQn, 5, 0);

    /* 8. 启动定时器 */
    timer_enable(SPEED_TIMER);

    sg_last_calc_time_us = get_time_us();
    sg_last_pulse_count  = 0;
    sg_pulse_count       = 0;
}

/**
 * @brief  设置测速编码器每转脉冲数 PPR
 */
void bsp_timer_set_ppr(uint16_t ppr)
{
    if (ppr > 0) {
        sg_ppr = ppr;
    }
}

/**
 * @brief  获取累计总脉冲数
 */
uint32_t bsp_timer_get_total_pulses(void)
{
    return sg_pulse_count;
}

/**
 * @brief  基于 20ms 窗口 M/T 测速法获取实时物理转速 (RPM) (50Hz 任务调用)
 * @return 旋转转速 (RPM), 零附加滤波相位滞后
 */
float bsp_timer_get_speed_rpm(void)
{
    uint32_t now_us = get_time_us();
    uint32_t now_pulses = sg_pulse_count;
    
    uint32_t delta_pulses = 0;
    uint32_t delta_time_us = 0;
    float delta_sec = 0.0f;
    float rpm = 0.0f;

    /* 计算脉冲增量与时间增量 */
    delta_pulses = now_pulses - sg_last_pulse_count;
    delta_time_us = now_us - sg_last_calc_time_us;

    /* 窗口更新 */
    sg_last_pulse_count = now_pulses;
    sg_last_calc_time_us = now_us;

    if (delta_time_us == 0) {
        return 0.0f;
    }

    if (delta_pulses == 0) {
        /* 无脉冲，判定转速为 0 */
        return 0.0f;
    }

    /* M/T 测速数学模型: RPM = (delta_pulses / PPR) / (delta_time_us * 1e-6) * 60 */
    delta_sec = (float)delta_time_us / 1000000.0f;
    rpm = ((float)delta_pulses / (float)sg_ppr) / delta_sec * 60.0f;

    return rpm;
}

/**
 * @brief  TIMER1 捕获中断服务例程
 */
void bsp_timer_irq_handler(void)
{
    if (RESET != timer_interrupt_flag_get(SPEED_TIMER, TIMER_INT_CH0)) {
        timer_interrupt_flag_clear(SPEED_TIMER, TIMER_INT_CH0);
        sg_pulse_count++;
    }
}
