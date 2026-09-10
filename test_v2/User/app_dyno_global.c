/**
  ******************************************************************************
  * @file    app_dyno_global.c
  * @brief   测功机全局数据中心与状态控制实现 (ARMCC V5 兼容)
  ******************************************************************************
  */

#include "app_dyno_global.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include <string.h>

/* 全局数据单例及互斥锁 */
static dyno_system_status_t g_dyno_status;
static dyno_pid_config_t     g_torque_pid_cfg;
static dyno_pid_config_t     g_power_pid_cfg;
static SemaphoreHandle_t     g_dyno_mutex = NULL;

/**
  * @brief  初始化测功机全局数据中心
  */
void app_dyno_global_init(void)
{
    if (g_dyno_mutex == NULL) {
        g_dyno_mutex = xSemaphoreCreateMutex();
    }

    if (g_dyno_mutex != NULL) {
        if (xSemaphoreTake(g_dyno_mutex, portMAX_DELAY) == pdTRUE) {
            memset(&g_dyno_status, 0, sizeof(g_dyno_status));
            g_dyno_status.mode          = DYNO_MODE_IDLE;
            g_dyno_status.target_value  = 0.0f;
            g_dyno_status.max_dac_limit = 3.3f;
            g_dyno_status.alarm_flags   = 0;

            /* 恒扭矩 PID 参数默认值 */
            g_torque_pid_cfg.kp = 0.5f;
            g_torque_pid_cfg.ki = 0.05f;
            g_torque_pid_cfg.kd = 0.01f;
            g_torque_pid_cfg.max_dac_output = 3.3f;

            /* 恒功率 PID 参数默认值 */
            g_power_pid_cfg.kp = 0.2f;
            g_power_pid_cfg.ki = 0.02f;
            g_power_pid_cfg.kd = 0.005f;
            g_power_pid_cfg.max_dac_output = 3.3f;

            xSemaphoreGive(g_dyno_mutex);
        }
    }
}

/**
  * @brief  获取当前工作模式
  */
dyno_mode_t app_dyno_get_mode(void)
{
    dyno_mode_t mode = DYNO_MODE_IDLE;
    if (g_dyno_mutex != NULL && xSemaphoreTake(g_dyno_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        mode = g_dyno_status.mode;
        xSemaphoreGive(g_dyno_mutex);
    }
    return mode;
}

/**
  * @brief  设置工作模式与目标设定值
  */
void app_dyno_set_mode(dyno_mode_t mode, float target_value)
{
    if (g_dyno_mutex != NULL && xSemaphoreTake(g_dyno_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        if (g_dyno_status.mode == DYNO_MODE_ESTOP && mode != DYNO_MODE_IDLE) {
            xSemaphoreGive(g_dyno_mutex);
            return;
        }

        g_dyno_status.mode = mode;
        g_dyno_status.target_value = target_value;

        if (mode == DYNO_MODE_IDLE || mode == DYNO_MODE_ESTOP) {
            g_dyno_status.target_value = 0.0f;
        }

        xSemaphoreGive(g_dyno_mutex);
    }
}

/**
  * @brief  触发紧急停止 (E-Stop)
  */
void app_dyno_emergency_stop(void)
{
    if (g_dyno_mutex != NULL && xSemaphoreTake(g_dyno_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        g_dyno_status.mode = DYNO_MODE_ESTOP;
        g_dyno_status.target_value = 0.0f;
        g_dyno_status.alarm_flags |= DYNO_ALARM_ESTOP;
        xSemaphoreGive(g_dyno_mutex);
    }
}

/**
  * @brief  获取全局系统状态快照
  */
void app_dyno_get_status(dyno_system_status_t *out_status)
{
    if (out_status == NULL) return;

    if (g_dyno_mutex != NULL && xSemaphoreTake(g_dyno_mutex, pdMS_TO_TICKS(20)) == pdTRUE) {
        g_dyno_status.timestamp_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
        memcpy(out_status, &g_dyno_status, sizeof(dyno_system_status_t));
        xSemaphoreGive(g_dyno_mutex);
    }
}

/**
  * @brief  更新传感器采样数据
  */
void app_dyno_update_telemetry(float torque_nm, float speed_rpm, float elec_v, float elec_i, float elec_p, float pf)
{
    if (g_dyno_mutex != NULL && xSemaphoreTake(g_dyno_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        g_dyno_status.torque_nm    = torque_nm;
        g_dyno_status.speed_rpm    = speed_rpm;
        
        if (speed_rpm > 0.1f || speed_rpm < -0.1f) {
            g_dyno_status.mech_power_w = (torque_nm * speed_rpm) / 9.549297f;
        } else {
            g_dyno_status.mech_power_w = 0.0f;
        }

        g_dyno_status.elec_voltage = elec_v;
        g_dyno_status.elec_current = elec_i;
        g_dyno_status.elec_power   = elec_p;
        g_dyno_status.power_factor = pf;

        if (elec_p > 1.0f && g_dyno_status.mech_power_w > 0.0f) {
            g_dyno_status.efficiency = (g_dyno_status.mech_power_w / elec_p) * 100.0f;
            if (g_dyno_status.efficiency > 100.0f) g_dyno_status.efficiency = 100.0f;
        } else {
            g_dyno_status.efficiency = 0.0f;
        }

        xSemaphoreGive(g_dyno_mutex);
    }
}

/**
  * @brief  更新 CS1237 扭矩测量数据
  */
void app_dyno_update_torque(float torque_nm)
{
    if (g_dyno_mutex != NULL && xSemaphoreTake(g_dyno_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        g_dyno_status.torque_nm = torque_nm;

        if (g_dyno_status.speed_rpm > 0.1f || g_dyno_status.speed_rpm < -0.1f) {
            g_dyno_status.mech_power_w = (torque_nm * g_dyno_status.speed_rpm) / 9.549297f;
        } else {
            g_dyno_status.mech_power_w = 0.0f;
        }

        if (g_dyno_status.elec_power > 1.0f && g_dyno_status.mech_power_w > 0.0f) {
            g_dyno_status.efficiency = (g_dyno_status.mech_power_w / g_dyno_status.elec_power) * 100.0f;
            if (g_dyno_status.efficiency > 100.0f) g_dyno_status.efficiency = 100.0f;
        } else {
            g_dyno_status.efficiency = 0.0f;
        }

        xSemaphoreGive(g_dyno_mutex);
    }
}

/**
  * @brief  更新转速测量数据
  */
void app_dyno_update_speed(float speed_rpm)
{
    if (g_dyno_mutex != NULL && xSemaphoreTake(g_dyno_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        g_dyno_status.speed_rpm = speed_rpm;

        if (speed_rpm > 0.1f || speed_rpm < -0.1f) {
            g_dyno_status.mech_power_w = (g_dyno_status.torque_nm * speed_rpm) / 9.549297f;
        } else {
            g_dyno_status.mech_power_w = 0.0f;
        }

        if (g_dyno_status.elec_power > 1.0f && g_dyno_status.mech_power_w > 0.0f) {
            g_dyno_status.efficiency = (g_dyno_status.mech_power_w / g_dyno_status.elec_power) * 100.0f;
            if (g_dyno_status.efficiency > 100.0f) g_dyno_status.efficiency = 100.0f;
        } else {
            g_dyno_status.efficiency = 0.0f;
        }

        xSemaphoreGive(g_dyno_mutex);
    }
}

/**
  * @brief  更新 CS1238 直流测量数据 (CH1=电压, CH2=电流)
  */
void app_dyno_update_dc(float dc_voltage_v, float dc_current_a)
{
    if (g_dyno_mutex != NULL && xSemaphoreTake(g_dyno_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        g_dyno_status.dc_voltage = dc_voltage_v;
        g_dyno_status.dc_current = dc_current_a;
        g_dyno_status.dc_power   = dc_voltage_v * dc_current_a;

        xSemaphoreGive(g_dyno_mutex);
    }
}

/**
  * @brief  更新 HLW8112 交流电力参量
  */
void app_dyno_update_hlw8112(float elec_v, float elec_i, float elec_p, float pf)
{
    if (g_dyno_mutex != NULL && xSemaphoreTake(g_dyno_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        g_dyno_status.elec_voltage = elec_v;
        g_dyno_status.elec_current = elec_i;
        g_dyno_status.elec_power   = elec_p;
        g_dyno_status.power_factor = pf;

        if (elec_p > 1.0f && g_dyno_status.mech_power_w > 0.0f) {
            g_dyno_status.efficiency = (g_dyno_status.mech_power_w / elec_p) * 100.0f;
            if (g_dyno_status.efficiency > 100.0f) g_dyno_status.efficiency = 100.0f;
        } else {
            g_dyno_status.efficiency = 0.0f;
        }

        xSemaphoreGive(g_dyno_mutex);
    }
}

/**
  * @brief  更新实际 DAC 输出电压记录
  */
void app_dyno_update_dac_voltage(float dac_v)
{
    if (g_dyno_mutex != NULL && xSemaphoreTake(g_dyno_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
        g_dyno_status.dac_voltage = dac_v;
        xSemaphoreGive(g_dyno_mutex);
    }
}

/**
  * @brief  设置 PID 参数 (预留)
  */
void app_dyno_set_pid_config(uint8_t loop_type, float kp, float ki, float kd, float max_dac)
{
    dyno_pid_config_t *target;
    if (g_dyno_mutex != NULL && xSemaphoreTake(g_dyno_mutex, pdMS_TO_TICKS(20)) == pdTRUE) {
        target = (loop_type == 0) ? &g_torque_pid_cfg : &g_power_pid_cfg;
        target->kp = kp;
        target->ki = ki;
        target->kd = kd;
        if (max_dac > 0.0f) {
            target->max_dac_output = max_dac;
        }
        xSemaphoreGive(g_dyno_mutex);
    }
}

/**
  * @brief  获取 PID 参数 (预留)
  */
void app_dyno_get_pid_config(uint8_t loop_type, dyno_pid_config_t *out_pid)
{
    dyno_pid_config_t *src;
    if (out_pid == NULL) return;

    if (g_dyno_mutex != NULL && xSemaphoreTake(g_dyno_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        src = (loop_type == 0) ? &g_torque_pid_cfg : &g_power_pid_cfg;
        memcpy(out_pid, src, sizeof(dyno_pid_config_t));
        xSemaphoreGive(g_dyno_mutex);
    }
}
