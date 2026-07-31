#ifndef BSP_HLW8112_H
#define BSP_HLW8112_H

#include "gd32f4xx.h"
#include <stdint.h>
#include <stdbool.h>

/* 
 * ============================================================================
 * HLW8112 GPIO 引脚定义 (立创梁山派 GD32F470ZGT6)
 * 默认使用 PORTB 引脚：PB5(CS), PB6(SCLK), PB7(SDI), PB8(SDO), PB9(EN)
 * 如插在 PB11~PB15 排针上，只需按注释修改下方 PIN 宏定义即可。
 * ============================================================================
 */
#define HLW8112_GPIO_RCU        RCU_GPIOB
#define HLW8112_GPIO_PORT       GPIOB

#define HLW8112_CS_PIN          GPIO_PIN_5   /* 片选引脚 (CS) */
#define HLW8112_SCLK_PIN        GPIO_PIN_6   /* 时钟引脚 (SCLK) */
#define HLW8112_SDI_PIN         GPIO_PIN_7   /* 数据输入 (MOSI / SDI) */
#define HLW8112_SDO_PIN         GPIO_PIN_8   /* 数据输出 (MISO / SDO) */
#define HLW8112_EN_PIN          GPIO_PIN_9   /* 芯片使能/复位 (EN / SEL) */

/* 
 * ============================================================================
 * HLW8112 内部寄存器地址映射 (标准 HLW8112 协议)
 * ============================================================================
 */
#define REG_SYSCON_ADDR         0x00    /* 系统控制寄存器 (16-bit) */
#define REG_EMUCON_ADDR         0x01    /* 电力计量控制寄存器 (16-bit) */
#define REG_HFCONST_ADDR        0x02    /* 脉冲常数寄存器 (16-bit) */
#define REG_EMUCON2_ADDR        0x13    /* 计量控制寄存器2 (16-bit) */

#define REG_ANGLE_ADDR          0x22    /* 相角寄存器 (16-bit) */
#define REG_UFREQ_ADDR          0x23    /* 电压线频率寄存器 (16-bit) */
#define REG_RMSIA_ADDR          0x24    /* 通道 A 电流有效值 (24-bit) */
#define REG_RMSIB_ADDR          0x25    /* 通道 B 电流有效值 (24-bit) */
#define REG_RMSU_ADDR           0x26    /* 电压有效值 (24-bit) */
#define REG_PF_ADDR             0x27    /* 功率因数 (24-bit) */
#define REG_ENERGY_PA_ADDR      0x28    /* 通道 A 有功电能 (24-bit) */
#define REG_ENERGY_PB_ADDR      0x29    /* 通道 B 有功电能 (24-bit) */
#define REG_POWER_PA_ADDR       0x2C    /* 通道 A 有功功率 (32-bit signed) */
#define REG_POWER_PB_ADDR       0x2D    /* 通道 B 有功功率 (32-bit signed) */

#define REG_IE_ADDR             0x40    /* 中断使能寄存器 (16-bit) */

/* HLW8112 芯片内部出厂标定参数寄存器 */
#define REG_CHECKSUM_ADDR       0x6F    /* 校验和寄存器 (16-bit) */
#define REG_RMS_IAC_ADDR        0x70    /* 通道 A 电流校准系数 (16-bit) */
#define REG_RMS_IBC_ADDR        0x71    /* 通道 B 电流校准系数 (16-bit) */
#define REG_RMS_UC_ADDR         0x72    /* 电压校准系数 (16-bit) */
#define REG_POWER_PAC_ADDR      0x73    /* 通道 A 有功功率校准系数 (16-bit) */
#define REG_POWER_PBC_ADDR      0x74    /* 通道 B 有功功率校准系数 (16-bit) */
#define REG_POWER_SC_ADDR       0x75    /* 视在功率校准系数 (16-bit) */
#define REG_ENERGY_AC_ADDR      0x76    /* 通道 A 电能校准系数 (16-bit) */
#define REG_ENERGY_BC_ADDR      0x77    /* 通道 B 电能校准系数 (16-bit) */

/* 命令字 */
#define HLW8112_CMD_WRITE_EN    0xE5    /* 解锁寄存器写入 */
#define HLW8112_CMD_WRITE_DIS   0xDC    /* 锁定寄存器写入 */
#define HLW8112_CMD_SEL_CHA     0x5A    /* 选择通道 A 进行有功/视在功率计算 */
#define HLW8112_CMD_SEL_CHB     0xA5    /* 选择通道 B 进行有功/视在功率计算 */

/* 
 * ============================================================================
 * HLW8112 全量电力测量数据结构体
 * ============================================================================
 */
typedef struct {
    float voltage;         /* 电压有效值 (V) */
    float current_a;       /* 通道 A 电流有效值 (A) */
    float current_a_ma;    /* 通道 A 电流有效值 (mA) */
    float current_b;       /* 通道 B 电流有效值 (A) */
    float active_power_a;  /* 通道 A 有功功率 (W) */
    float active_power_b;  /* 通道 B 有功功率 (W) */
    float active_energy_a; /* 通道 A 有功电能 (kWh) */
    float active_energy_b; /* 通道 B 有功电能 (kWh) */
    float power_factor;    /* 功率因数 (0.000 ~ 1.000) */
    float frequency;       /* 电网频率 (Hz) */
    float phase_angle;     /* 相位角 (度 °) */

    bool  calib_ok;        /* 芯片内部校准参数校验状态 Flag */
} hlw8112_data_t;

/* 
 * ============================================================================
 * 驱动 API Declarations
 * ============================================================================
 */
void hlw8112_init(void);
void hlw8112_read_data(hlw8112_data_t *data);
uint8_t hlw8112_check_calibration(void);

#endif /* BSP_HLW8112_H */
