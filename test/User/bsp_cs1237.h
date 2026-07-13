#ifndef BSP_CS1237_H
#define BSP_CS1237_H

#include "gd32f4xx.h"

/* GPIO Pin Configuration for CS1237 */
#define CS1237_CLK_RCU       RCU_GPIOC
#define CS1237_CLK_PORT      GPIOC
#define CS1237_CLK_PIN       GPIO_PIN_1

#define CS1237_DOUT_RCU      RCU_GPIOC
#define CS1237_DOUT_PORT     GPIOC
#define CS1237_DOUT_PIN      GPIO_PIN_2

/* CS1237 SPI commands (7-bit commands) */
#define CS1237_CMD_READ      0x56  /* Read Configuration Register */
#define CS1237_CMD_WRITE     0x65  /* Write Configuration Register */

/* 
 * CS1237 Configuration Register Bit Definitions (8-bit register)
 * 
 * Bit 7:   Reserved (must write 0)
 * Bit 6:   REFO_OFF (Reference output off control)
 *          0: Internal reference output enabled (REFO active)
 *          1: Internal reference output disabled (REFO high impedance)
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
 *          00: Channel A (Differential input AINP - AINN)
 *          01: Reserved
 *          10: Temperature Sensor
 *          11: Internal Short (for calibration)
 */

/* PGA Gain settings */
typedef enum {
    CS1237_PGA_1X   = 0x00,
    CS1237_PGA_2X   = 0x01,
    CS1237_PGA_64X  = 0x02,
    CS1237_PGA_128X = 0x03
} cs1237_pga_t;

/* Output rate (SPEED) settings */
typedef enum {
    CS1237_SPEED_10HZ   = 0x00,
    CS1237_SPEED_40HZ   = 0x01,
    CS1237_SPEED_640HZ  = 0x02,
    CS1237_SPEED_1280HZ = 0x03
} cs1237_speed_t;

/* Channel selection */
typedef enum {
    CS1237_CH_A     = 0x00,
    CS1237_CH_TEMP  = 0x02,
    CS1237_CH_SHORT = 0x03
} cs1237_ch_t;

/* VREF Output mode */
typedef enum {
    CS1237_VREF_ON  = 0x00, /* Internal reference output enabled */
    CS1237_VREF_OFF = 0x01  /* Internal reference output disabled */
} cs1237_vref_t;

/* API Functions */
void cs1237_init(void);
int32_t cs1237_read_adc_raw(uint8_t *success);
int32_t cs1237_read_adc_signed(uint8_t *success);
uint8_t cs1237_read_reg(void);
void cs1237_write_reg(uint8_t reg_val);
void cs1237_configure(cs1237_pga_t gain, cs1237_speed_t speed, cs1237_ch_t ch, cs1237_vref_t vref);

#endif /* BSP_CS1237_H */
