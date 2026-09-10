#ifndef BSP_CS1238_H
#define BSP_CS1238_H

#include "gd32f4xx.h"
#include <stdint.h>

/* GPIO Pin Configuration for CS1238 (Software 2-Wire Interface) */
#define CS1238_CLK_RCU       RCU_GPIOF
#define CS1238_CLK_PORT      GPIOF
#define CS1238_CLK_PIN       GPIO_PIN_8

#define CS1238_DOUT_RCU      RCU_GPIOF
#define CS1238_DOUT_PORT     GPIOF
#define CS1238_DOUT_PIN      GPIO_PIN_9

/* CS1238 SPI commands (7-bit commands shifted during pulses 30-36) */
#define CS1238_CMD_READ      0x56  /* Read Configuration Register */
#define CS1238_CMD_WRITE     0x65  /* Write Configuration Register */

/* 物理转换缩放系数: mV -> V 或 mV -> A */
#ifndef DC_V_SCALE
#define DC_V_SCALE           0.001f  /* mV -> V (默认直接除以1000) */
#endif

#ifndef DC_I_SCALE
#define DC_I_SCALE           0.001f  /* mV -> A */
#endif

/* 
 * CS1238 Configuration Register Bit Definitions (8-bit register)
 * 
 * Bit 7:   Reserved (must write 0)
 * Bit 6:   REFO_OFF (Reference output off control)
 *          0: Internal reference output enabled
 *          1: Internal reference output disabled (high impedance)
 * Bit 5-4: SPEED (Output Data Rate)
 *          00: 10 Hz
 *          01: 40 Hz
 *          10: 640 Hz
 *          11: 1280 Hz
 * Bit 3-2: PGA (Programmable Gain Amplifier)
 *          00: 1x
 *          01: 2x
 *          10: 64x
 *          11: 128x
 * Bit 1-0: CH_SEL (Input Channel Select)
 *          00: Channel 1 (Differential input AIN1P - AIN1N)
 *          01: Channel 2 (Differential input AIN2P - AIN2N)
 *          10: Temperature Sensor
 *          11: Internal Short (for offset calibration)
 */

/* PGA Gain settings */
typedef enum {
    CS1238_PGA_1X   = 0x00,
    CS1238_PGA_2X   = 0x01,
    CS1238_PGA_64X  = 0x02,
    CS1238_PGA_128X = 0x03
} cs1238_pga_t;

/* Output rate (SPEED) settings */
typedef enum {
    CS1238_SPEED_10HZ   = 0x00,
    CS1238_SPEED_40HZ   = 0x01,
    CS1238_SPEED_640HZ  = 0x02,
    CS1238_SPEED_1280HZ = 0x03
} cs1238_speed_t;

/* Channel selection for CS1238 Dual-Channel ADC */
typedef enum {
    CS1238_CH1      = 0x00, /* Differential Channel 1 (AIN1P - AIN1N) */
    CS1238_CH2      = 0x01, /* Differential Channel 2 (AIN2P - AIN2N) */
    CS1238_CH_TEMP  = 0x02, /* Internal Temperature Sensor */
    CS1238_CH_SHORT = 0x03  /* Internal Short Calibration */
} cs1238_ch_t;

/* VREF Output mode */
typedef enum {
    CS1238_VREF_ON  = 0x00, /* Internal reference output enabled */
    CS1238_VREF_OFF = 0x01  /* Internal reference output disabled */
} cs1238_vref_t;

/* 一阶 IIR 低通滤波器结构体 */
typedef struct {
    float   filtered_val;
    float   alpha;
    uint8_t initialized;
} lowpass_filter_t;

/* Multi-channel CS1238 Measurement Result Structure with Independent Per-Channel PGA */
typedef struct {
    int32_t      raw_ch1;       /* CH1 Signed 24-bit raw ADC reading */
    float        volt_ch1_mv;   /* CH1 Calculated voltage (mV) */
    cs1238_pga_t pga_ch1;       /* CH1 Active PGA gain setting */

    int32_t      raw_ch2;       /* CH2 Signed 24-bit raw ADC reading */
    float        volt_ch2_mv;   /* CH2 Calculated voltage (mV) */
    cs1238_pga_t pga_ch2;       /* CH2 Active PGA gain setting */

    int32_t      raw_temp;      /* Temperature Sensor raw reading */
    uint8_t      success;       /* 1 if all channels sampled successfully, 0 on timeout */
} cs1238_data_t;

/* Standard API Functions */
void cs1238_init(void);
int32_t cs1238_read_adc_raw(uint8_t *success);
int32_t cs1238_read_adc_signed(uint8_t *success);
uint8_t cs1238_read_reg(void);
void cs1238_write_reg(uint8_t reg_val);
void cs1238_configure(cs1238_pga_t gain, cs1238_speed_t speed, cs1238_ch_t ch, cs1238_vref_t vref);
void cs1238_select_channel(cs1238_ch_t ch);

/* Smart Adaptive Gain Control & Multi-Channel APIs */
uint16_t cs1238_get_pga_multiplier(cs1238_pga_t pga);
cs1238_pga_t cs1238_get_current_pga(void);
cs1238_ch_t cs1238_get_current_channel(void);
int32_t cs1238_read_channel_adc(cs1238_ch_t ch, uint8_t *success);
int32_t cs1238_read_channel_smart_auto_range(cs1238_ch_t ch, uint8_t *success, cs1238_pga_t *out_pga);
float cs1238_raw_to_voltage_mv(int32_t raw_val, cs1238_pga_t pga, float vref_volts);
void cs1238_read_all_channels(cs1238_data_t *data, float vref_volts);

/* 一阶 IIR 滤波器 API */
float lowpass_filter_apply(lowpass_filter_t *filter, float raw_val);
void  lowpass_filter_reset(lowpass_filter_t *filter, float initial_val);
void  cs1238_read_dc_filtered(float *out_dc_v, float *out_dc_i);

#endif /* BSP_CS1238_H */
