#ifndef BSP_HLW8112_H
#define BSP_HLW8112_H

#include "gd32f4xx.h"

/* HLW8112 Register Map Definitions */
#define REG_SYSCON_ADDR         0x00    /* System Control Register (16-bit) */
#define REG_EMUCON_ADDR         0x01    /* Energy Measure Control Register (16-bit) */
#define REG_HFCONST_ADDR        0x02    /* High Frequency Constant (16-bit) */
#define REG_EMUCON2_ADDR        0x13    /* Energy Measure Control Register 2 (16-bit) */

#define REG_ANGLE_ADDR          0x22    /* Phase Angle Register (16-bit) */
#define REG_UFREQ_ADDR          0x23    /* Voltage Line Frequency Register (16-bit) */
#define REG_RMSIA_ADDR          0x24    /* Current RMS Channel A (24-bit) */
#define REG_RMSIB_ADDR          0x25    /* Current RMS Channel B (24-bit) */
#define REG_RMSU_ADDR           0x26    /* Voltage RMS Channel (24-bit) */
#define REG_PF_ADDR             0x27    /* Power Factor Register (24-bit) */
#define REG_ENERGY_PA_ADDR      0x28    /* Active Energy Channel A (24-bit) */
#define REG_ENERGY_PB_ADDR      0x29    /* Active Energy Channel B (24-bit) */
#define REG_POWER_PA_ADDR       0x2C    /* Active Power Channel A (32-bit signed) */
#define REG_POWER_PB_ADDR       0x2D    /* Active Power Channel B (32-bit signed) */

#define REG_SAGCYC_ADDR         0x17    /* Voltage Sag Cycle (16-bit) */
#define REG_SAGLVL_ADDR         0x18    /* Voltage Sag Level (16-bit) */
#define REG_OVLVL_ADDR          0x19    /* Overvoltage Threshold (16-bit) */
#define REG_OIALVL_ADDR         0x1A    /* Overcurrent Channel A Threshold (16-bit) */

#define REG_INT_ADDR            0x1D    /* Interrupt Configuration Register (16-bit) */
#define REG_IE_ADDR             0x40    /* Interrupt Enable Register (16-bit) */
#define REG_IF_ADDR             0x41    /* Interrupt Flags Register (16-bit) */
#define REG_RIF_ADDR            0x42    /* Raw Interrupt Flags Register (16-bit) */

#define REG_CHECKSUM_ADDR       0x6F    /* Calibration Parameter Checksum (16-bit) */
#define REG_RMS_IAC_ADDR        0x70    /* Current A RMS Calibration Coefficient (16-bit) */
#define REG_RMS_IBC_ADDR        0x71    /* Current B RMS Calibration Coefficient (16-bit) */
#define REG_RMS_UC_ADDR         0x72    /* Voltage RMS Calibration Coefficient (16-bit) */
#define REG_POWER_PAC_ADDR      0x73    /* Active Power A Calibration Coefficient (16-bit) */
#define REG_POWER_PBC_ADDR      0x74    /* Active Power B Calibration Coefficient (16-bit) */
#define REG_POWER_SC_ADDR       0x75    /* Apparent Power Calibration Coefficient (16-bit) */
#define REG_ENERGY_AC_ADDR      0x76    /* Active Energy A Calibration Coefficient (16-bit) */
#define REG_ENERGY_BC_ADDR      0x77    /* Active Energy B Calibration Coefficient (16-bit) */

/* Special Commands */
#define HLW8112_CMD_WRITE_EN    0xE5    /* Unlock Write Protection */
#define HLW8112_CMD_WRITE_DIS   0xDC    /* Lock Write Protection */
#define HLW8112_CMD_SEL_CHA     0x5A    /* Select Channel A for Active/Apparent Power */
#define HLW8112_CMD_SEL_CHB     0xA5    /* Select Channel B for Active/Apparent Power */

/* Software SPI GPIO Pin Definitions (GPIOB) */
#define HLW8112_GPIO_RCU        RCU_GPIOB
#define HLW8112_GPIO_PORT       GPIOB

#define HLW8112_EN_PIN          GPIO_PIN_11  /* Enable/Reset Pin (Output) */
#define HLW8112_CS_PIN          GPIO_PIN_12  /* Chip Select Pin (Output) */
#define HLW8112_SCLK_PIN        GPIO_PIN_13  /* Clock Pin (Output) */
#define HLW8112_SDO_PIN         GPIO_PIN_14  /* Data Output (Input to MCU) */
#define HLW8112_SDI_PIN         GPIO_PIN_15  /* Data Input (Output from MCU) */

/* HLW8112 Measurement Data Structure */
typedef struct {
    float voltage;         /* Voltage (V) */
    float current_a;       /* Channel A Current (A) */
    float current_b;       /* Channel B Current (A) */
    float active_power_a;  /* Channel A Active Power (W) */
    float active_power_b;  /* Channel B Active Power (W) */
    float active_energy_a; /* Channel A Active Energy (kWh) */
    float active_energy_b; /* Channel B Active Energy (kWh) */
    float power_factor;    /* Power Factor */
    float frequency;       /* Frequency (Hz) */
    float phase_angle;     /* Phase Angle (Degrees) */
} hlw8112_data_t;

/* API Declarations */
void hlw8112_init(void);
void hlw8112_read_data(hlw8112_data_t *data);
uint8_t hlw8112_check_calibration(void);

#endif /* BSP_HLW8112_H */
