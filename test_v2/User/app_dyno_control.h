/**
  ******************************************************************************
  * @file    app_dyno_control.h
  * @brief   测功机加载控制与状态机管理任务头文件
  ******************************************************************************
  */

#ifndef __APP_DYNO_CONTROL_H
#define __APP_DYNO_CONTROL_H

#include "gd32f4xx.h"
#include "app_dyno_global.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 初始化测功机控制任务及硬件驱动 (包含 DAC 初始化) */
void app_dyno_control_init(void);

/* FreeRTOS 100Hz 闭环/状态机控制主任务 */
void app_task_dyno_control(void *pvParameters);

/* 闭环 PID 算子占位接口 (预留，后续添加闭环算法时在此扩展) */
float dyno_pid_calc_stub(uint8_t loop_type, float target, float feedback, float dt);

#ifdef __cplusplus
}
#endif

#endif /* __APP_DYNO_CONTROL_H */
