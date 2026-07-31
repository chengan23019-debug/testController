/**
  ******************************************************************************
  * @file    app_dyno_control.c
  * @brief   测功机加载控制与状态机管理任务实现 (ARMCC V5 兼容)
  ******************************************************************************
  */

#include "app_dyno_control.h"
#include "bsp_dac.h"
#include "FreeRTOS.h"
#include "task.h"

/**
  * @brief  PID 算子桩接口 (预留)
  */
float dyno_pid_calc_stub(uint8_t loop_type, float target, float feedback, float dt)
{
    dyno_pid_config_t pid_cfg;
    float dac_out = 0.0f;

    (void)feedback;
    (void)dt;

    app_dyno_get_pid_config(loop_type, &pid_cfg);

    if (loop_type == 0) {
        dac_out = target * 0.1f; 
    } else {
        dac_out = target * 0.005f;
    }

    if (dac_out > pid_cfg.max_dac_output) {
        dac_out = pid_cfg.max_dac_output;
    }

    return dac_out;
}

/**
  * @brief  初始化测功机控制模块
  */
void app_dyno_control_init(void)
{
    bsp_dac_init();
    bsp_dac_set_voltage(0, 0.0f);
    app_dyno_update_dac_voltage(0.0f);
}

/**
  * @brief  测功机状态机与加载控制任务 (100Hz / 10ms 周期)
  */
void app_task_dyno_control(void *pvParameters)
{
    dyno_system_status_t status;
    float target_dac;

    (void)pvParameters;

    app_dyno_control_init();

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(10));

        target_dac = 0.0f;
        app_dyno_get_status(&status);

        /* 测功机状态机刷新 */
        switch (status.mode) {
            case DYNO_MODE_IDLE:
                target_dac = 0.0f;
                break;

            case DYNO_MODE_MANUAL:
                target_dac = status.target_value;
                break;

            case DYNO_MODE_CONST_TORQUE:
                target_dac = dyno_pid_calc_stub(0, status.target_value, status.torque_nm, 0.01f);
                break;

            case DYNO_MODE_CONST_POWER:
                target_dac = dyno_pid_calc_stub(1, status.target_value, status.mech_power_w, 0.01f);
                break;

            case DYNO_MODE_ESTOP:
            default:
                target_dac = 0.0f;
                break;
        }

        /* 限幅 */
        if (target_dac < 0.0f) {
            target_dac = 0.0f;
        }
        if (target_dac > status.max_dac_limit) {
            target_dac = status.max_dac_limit;
        }

        /* 更新 DAC */
        bsp_dac_set_voltage(0, target_dac);
        app_dyno_update_dac_voltage(target_dac);
    }
}
