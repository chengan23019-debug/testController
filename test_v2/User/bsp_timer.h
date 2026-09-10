#ifndef BSP_TIMER_H
#define BSP_TIMER_H

#include "gd32f4xx.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 测速码盘默认每转脉冲数 PPR (Pulses Per Revolution) */
#ifndef SPEED_PULSE_PPR
#define SPEED_PULSE_PPR         60U
#endif

/* 定时器引脚映射 (PA0 -> TIMER1_CH0 / TIMER4_CH0) */
#define SPEED_TIMER_RCU         RCU_TIMER1
#define SPEED_TIMER             TIMER1
#define SPEED_TIMER_GPIO_RCU    RCU_GPIOA
#define SPEED_TIMER_GPIO_PORT   GPIOA
#define SPEED_TIMER_PIN         GPIO_PIN_0
#define SPEED_TIMER_AF          GPIO_AF_1
#define SPEED_TIMER_CHANNEL     TIMER_CH_0
#define SPEED_TIMER_IRQn        TIMER1_IRQn

/* 驱动接口声明 */
void  bsp_timer_init(void);
float bsp_timer_get_speed_rpm(void);
void  bsp_timer_set_ppr(uint16_t ppr);
uint32_t bsp_timer_get_total_pulses(void);
void  bsp_timer_irq_handler(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_TIMER_H */
