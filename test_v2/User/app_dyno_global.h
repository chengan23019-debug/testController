/**
  ******************************************************************************
  * @file    app_dyno_global.h
  * @brief   测功机全局数据中心与状态中心头文件
  ******************************************************************************
  */

#ifndef __APP_DYNO_GLOBAL_H
#define __APP_DYNO_GLOBAL_H

#include "gd32f4xx.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 测功机工作模式定义 */
typedef enum {
    DYNO_MODE_IDLE         = 0,  /* 空闲/停止模式 (DAC输出0V) */
    DYNO_MODE_MANUAL       = 1,  /* 手动开环DAC控制模式 (目标值为DAC电压/百分比) */
    DYNO_MODE_CONST_TORQUE = 2,  /* 恒扭矩控制模式 (预留PID接口) */
    DYNO_MODE_CONST_POWER  = 3,  /* 恒功率控制模式 (预留PID接口) */
    DYNO_MODE_ESTOP        = 4   /* 紧急停止状态 (强制DAC=0V，拒绝非复位指令) */
} dyno_mode_t;

/* 报警与故障标志位定义 */
#define DYNO_ALARM_OVER_TORQUE    (1 << 0)  /* 扭矩超限报警 */
#define DYNO_ALARM_OVER_SPEED     (1 << 1)  /* 超速报警 */
#define DYNO_ALARM_OVER_TEMP      (1 << 2)  /* 制动器/电机超温报警 */
#define DYNO_ALARM_ESTOP          (1 << 3)  /* 急停触发标志 */
#define DYNO_ALARM_SENSOR_ERR     (1 << 4)  /* ADC/传感器通信故障 */

/* PID 参数结构体 (预留) */
typedef struct {
    float kp;              /* 比例系数 */
    float ki;              /* 积分系数 */
    float kd;              /* 微分系数 */
    float max_dac_output;  /* DAC输出上限 (V) */
} dyno_pid_config_t;

/* 测功机全局系统状态结构体 */
typedef struct {
    dyno_mode_t mode;           /* 当前运行模式 */
    float       target_value;   /* 当前目标设定值 (手动V / 恒扭矩N.m / 恒功率W) */
    
    /* 机械参量 (来自于 CS1237 扭矩测量与脉冲转速) */
    float       torque_nm;      /* 实际扭矩 (N.m, CS1237 测量) */
    float       speed_rpm;      /* 实际转速 (RPM) */
    float       mech_power_w;   /* 计算机械功率 (W) */

    /* 实际驱动输出 */
    float       dac_voltage;    /* 当前 DAC 实时输出电压 (V) */

    /* 直流电参量 (来自于 CS1238: CH1=电压, CH2=电流) */
    float       dc_voltage;     /* 直流电压 (V, CS1238 CH1) */
    float       dc_current;     /* 直流电流 (A, CS1238 CH2) */
    float       dc_power;       /* 直流功率 (W) = V * I */

    /* 交流电力参量 (来自于 HLW8112) */
    float       elec_voltage;   /* 交流电压 (V) */
    float       elec_current;   /* 交流电流 (A) */
    float       elec_power;     /* 交流电功率 (W) */
    float       power_factor;   /* 功率因数 */
    float       efficiency;     /* 效率 (%) */

    /* 安全与控制参数 */
    float       max_dac_limit;  /* 全局 DAC 安全上限 (V) */
    uint16_t    alarm_flags;    /* 报警标志位集合 */
    uint32_t    timestamp_ms;   /* 系统时间戳 (ms) */
} dyno_system_status_t;

/* 全局数据中心初始化 API */
void app_dyno_global_init(void);

/* 模式与目标值控制 API */
dyno_mode_t app_dyno_get_mode(void);
void app_dyno_set_mode(dyno_mode_t mode, float target_value);
void app_dyno_emergency_stop(void);

/* 状态读取 API (拷贝安全快照) */
void app_dyno_get_status(dyno_system_status_t *out_status);

/* 传感器数据更新 API (供底层采样任务调用) */
void app_dyno_update_torque(float torque_nm);
void app_dyno_update_speed(float speed_rpm);
void app_dyno_update_dc(float dc_voltage_v, float dc_current_a);
void app_dyno_update_hlw8112(float elec_v, float elec_i, float elec_p, float pf);
void app_dyno_update_telemetry(float torque_nm, float speed_rpm, float elec_v, float elec_i, float elec_p, float pf);
void app_dyno_update_dac_voltage(float dac_v);

/* PID 参数设置与读取 API (预留) */
void app_dyno_set_pid_config(uint8_t loop_type, float kp, float ki, float kd, float max_dac);
void app_dyno_get_pid_config(uint8_t loop_type, dyno_pid_config_t *out_pid);

#ifdef __cplusplus
}
#endif

#endif /* __APP_DYNO_GLOBAL_H */
