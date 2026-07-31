#include "bsp_cs1238.h"
#include <stdlib.h>

/* Global state tracking for CS1238 */
static cs1238_pga_t   sg_current_pga   = CS1238_PGA_128X;
static cs1238_speed_t sg_current_speed = CS1238_SPEED_10HZ;
static cs1238_ch_t    sg_current_ch    = CS1238_CH1;
static cs1238_vref_t  sg_current_vref  = CS1238_VREF_ON;

/* Independent per-channel PGA gain state memory for CH1 and CH2 */
static cs1238_pga_t   sg_channel_pga[2] = {CS1238_PGA_128X, CS1238_PGA_128X};

/* Clock delay helper for GD32F470 at 240MHz */
static void cs1238_delay_us(uint32_t us)
{
    volatile uint32_t count = us * 120;
    while (count--) {
        __NOP();
    }
}

/* Configure DOUT/DRDY pin as INPUT (with pull-up) */
static void cs1238_dout_as_input(void)
{
    gpio_mode_set(CS1238_DOUT_PORT, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, CS1238_DOUT_PIN);
}

/* Configure DOUT/DRDY pin as OUTPUT (push-pull) */
static void cs1238_dout_as_output(void)
{
    gpio_mode_set(CS1238_DOUT_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, CS1238_DOUT_PIN);
    gpio_output_options_set(CS1238_DOUT_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, CS1238_DOUT_PIN);
}

/* CLK line helper macros */
#define CS1238_CLK_H()    gpio_bit_set(CS1238_CLK_PORT, CS1238_CLK_PIN)
#define CS1238_CLK_L()    gpio_bit_reset(CS1238_CLK_PORT, CS1238_CLK_PIN)

/* DOUT line helper macros */
#define CS1238_DOUT_H()   gpio_bit_set(CS1238_DOUT_PORT, CS1238_DOUT_PIN)
#define CS1238_DOUT_L()   gpio_bit_reset(CS1238_DOUT_PORT, CS1238_DOUT_PIN)
#define CS1238_DOUT_R()   gpio_input_bit_get(CS1238_DOUT_PORT, CS1238_DOUT_PIN)

/**
 * @brief Wait for DOUT/DRDY pin to fall LOW (Data Ready) with configurable millisecond timeout
 * @param timeout_ms Maximum time to wait in milliseconds (e.g. 150ms for 10Hz data rate)
 * @return 1 if DRDY went LOW, 0 if timed out
 */
static uint8_t cs1238_wait_drdy_low(uint32_t timeout_ms)
{
    uint32_t max_us = timeout_ms * 1000;
    while (CS1238_DOUT_R() == SET) {
        if (max_us < 10) {
            return 0; /* Timed out */
        }
        cs1238_delay_us(10);
        max_us -= 10;
    }
    return 1;
}

/**
 * @brief Initialize GPIO pins and hardware state for CS1238
 */
void cs1238_init(void)
{
    /* Enable clocks for GPIO ports */
    rcu_periph_clock_enable(CS1238_CLK_RCU);
    rcu_periph_clock_enable(CS1238_DOUT_RCU);

    /* SCLK as push-pull output */
    gpio_mode_set(CS1238_CLK_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, CS1238_CLK_PIN);
    gpio_output_options_set(CS1238_CLK_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, CS1238_CLK_PIN);

    /* SCLK initially low */
    CS1238_CLK_L();

    /* DOUT/DRDY initially as input with pull-up */
    cs1238_dout_as_input();

    /* Power-Down & Reset Sequence (SCLK high for >100us resets CS1238) */
    CS1238_CLK_H();
    cs1238_delay_us(200);
    CS1238_CLK_L();
    cs1238_delay_us(2000); /* Short stabilization */

    /* Wait up to 200ms for chip power-up conversion completion */
    (void)cs1238_wait_drdy_low(200);

    /* Reset per-channel PGA memory */
    sg_channel_pga[0] = CS1238_PGA_128X;
    sg_channel_pga[1] = CS1238_PGA_128X;

    /* Default configuration: PGA 128X, 10Hz, Channel 1, VREF On */
    cs1238_configure(CS1238_PGA_128X, CS1238_SPEED_10HZ, CS1238_CH1, CS1238_VREF_ON);
}

/**
 * @brief Read raw 24-bit data from CS1238 ADC
 * @param success Pointer to status flag (1 = success, 0 = timeout/error)
 * @return 24-bit raw unsigned value
 */
int32_t cs1238_read_adc_raw(uint8_t *success)
{
    int32_t raw_data = 0;
    int i;

    /* Wait for DOUT/DRDY to go LOW (data ready) up to 150ms (for 10Hz mode) */
    if (!cs1238_wait_drdy_low(150)) {
        if (success) {
            *success = 0;
        }
        return 0;
    }

    if (success) {
        *success = 1;
    }

    /* Read 24-bit data MSB first (clock pulses 1-24) */
    for (i = 0; i < 24; i++) {
        CS1238_CLK_H();
        cs1238_delay_us(1);
        raw_data = (raw_data << 1);
        if (CS1238_DOUT_R() == SET) {
            raw_data |= 1;
        }
        CS1238_CLK_L();
        cs1238_delay_us(1);
    }

    /* Send 3 extra clock pulses (pulses 25, 26, 27) */
    for (i = 0; i < 3; i++) {
        CS1238_CLK_H();
        cs1238_delay_us(1);
        CS1238_CLK_L();
        cs1238_delay_us(1);
    }

    return raw_data;
}

/**
 * @brief Read 24-bit signed data from CS1238 ADC (includes sign extension)
 * @param success Pointer to status flag (1 = success, 0 = timeout/error)
 * @return 32-bit signed ADC value
 */
int32_t cs1238_read_adc_signed(uint8_t *success)
{
    int32_t val = cs1238_read_adc_raw(success);
    
    /* Sign extension from 24-bit to 32-bit */
    if (val & 0x800000) {
        val |= 0xFF000000;
    }
    
    return val;
}

/**
 * @brief Write data to CS1238 configuration register
 * @param reg_val New register byte to write
 */
void cs1238_write_reg(uint8_t reg_val)
{
    int i;
    uint8_t cmd = CS1238_CMD_WRITE;

    /* 1. Wait for DOUT/DRDY to go LOW (up to 150ms) */
    if (!cs1238_wait_drdy_low(150)) {
        return;
    }

    /* 2. Clock out 24 dummy/ADC pulses (pulses 1-24) */
    for (i = 0; i < 24; i++) {
        CS1238_CLK_H();
        cs1238_delay_us(1);
        CS1238_CLK_L();
        cs1238_delay_us(1);
    }

    /* 3. Clock 3 dummy pulses (pulses 25, 26, 27) */
    for (i = 0; i < 3; i++) {
        CS1238_CLK_H();
        cs1238_delay_us(1);
        CS1238_CLK_L();
        cs1238_delay_us(1);
    }

    /* 4. Configure DOUT pin as output to write the command */
    cs1238_dout_as_output();

    /* 5. Clock 2 transit pulses (pulses 28, 29), keep DOUT low */
    CS1238_DOUT_L();
    for (i = 0; i < 2; i++) {
        CS1238_CLK_H();
        cs1238_delay_us(1);
        CS1238_CLK_L();
        cs1238_delay_us(1);
    }

    /* 6. Shift out the 7-bit WRITE command (0x65, MSB first, pulses 30-36) */
    for (i = 0; i < 7; i++) {
        CS1238_CLK_H();
        if ((cmd >> (6 - i)) & 0x01) {
            CS1238_DOUT_H();
        } else {
            CS1238_DOUT_L();
        }
        cs1238_delay_us(1);
        CS1238_CLK_L();
        cs1238_delay_us(1);
    }

    /* 7. Clock 37 (transit pulse) */
    CS1238_CLK_H();
    cs1238_delay_us(1);
    CS1238_CLK_L();
    cs1238_delay_us(1);

    /* 8. Shift out the 8-bit register data (MSB first, pulses 38-45) */
    for (i = 0; i < 8; i++) {
        CS1238_CLK_H();
        if ((reg_val >> (7 - i)) & 0x01) {
            CS1238_DOUT_H();
        } else {
            CS1238_DOUT_L();
        }
        cs1238_delay_us(1);
        CS1238_CLK_L();
        cs1238_delay_us(1);
    }

    /* 9. Clock 46 (finalize clock) */
    CS1238_CLK_H();
    cs1238_delay_us(1);
    CS1238_CLK_L();
    cs1238_delay_us(1);

    /* 10. Restore DOUT as input for future readings */
    cs1238_dout_as_input();
}

/**
 * @brief Read data from CS1238 configuration register
 * @return The 8-bit register byte read
 */
uint8_t cs1238_read_reg(void)
{
    uint8_t reg_val = 0;
    int i;
    uint8_t cmd = CS1238_CMD_READ;

    /* 1. Wait for DOUT/DRDY to go LOW (up to 150ms) */
    if (!cs1238_wait_drdy_low(150)) {
        return 0;
    }

    /* 2. Clock out 24 dummy/ADC pulses (pulses 1-24) */
    for (i = 0; i < 24; i++) {
        CS1238_CLK_H();
        cs1238_delay_us(1);
        CS1238_CLK_L();
        cs1238_delay_us(1);
    }

    /* 3. Clock 3 dummy pulses (pulses 25, 26, 27) */
    for (i = 0; i < 3; i++) {
        CS1238_CLK_H();
        cs1238_delay_us(1);
        CS1238_CLK_L();
        cs1238_delay_us(1);
    }

    /* 4. Configure DOUT pin as output to write command */
    cs1238_dout_as_output();

    /* 5. Clock 2 transit pulses (pulses 28, 29) */
    CS1238_DOUT_L();
    for (i = 0; i < 2; i++) {
        CS1238_CLK_H();
        cs1238_delay_us(1);
        CS1238_CLK_L();
        cs1238_delay_us(1);
    }

    /* 6. Shift out the 7-bit READ command (0x56, MSB first, pulses 30-36) */
    for (i = 0; i < 7; i++) {
        CS1238_CLK_H();
        if ((cmd >> (6 - i)) & 0x01) {
            CS1238_DOUT_H();
        } else {
            CS1238_DOUT_L();
        }
        cs1238_delay_us(1);
        CS1238_CLK_L();
        cs1238_delay_us(1);
    }

    /* 7. Clock 37 (transit pulse, switch DOUT to input mode) */
    CS1238_CLK_H();
    cs1238_delay_us(1);
    CS1238_CLK_L();
    cs1238_dout_as_input();
    cs1238_delay_us(1);

    /* 8. Shift in the 8-bit register data (MSB first, pulses 38-45) */
    for (i = 0; i < 8; i++) {
        CS1238_CLK_H();
        cs1238_delay_us(1);
        reg_val = (reg_val << 1);
        if (CS1238_DOUT_R() == SET) {
            reg_val |= 1;
        }
        CS1238_CLK_L();
        cs1238_delay_us(1);
    }

    /* 9. Clock 46 (finalize clock) */
    CS1238_CLK_H();
    cs1238_delay_us(1);
    CS1238_CLK_L();
    cs1238_delay_us(1);

    return reg_val;
}

/**
 * @brief Configure CS1238 parameters
 * @param gain PGA gain (1x, 2x, 64x, 128x)
 * @param speed Sampling rate (10Hz, 40Hz, 640Hz, 1280Hz)
 * @param ch Input channel (CH1, CH2, Temp Sensor, Short)
 * @param vref VREF status (On, Off)
 */
void cs1238_configure(cs1238_pga_t gain, cs1238_speed_t speed, cs1238_ch_t ch, cs1238_vref_t vref)
{
    uint8_t reg_val = 0;
    
    sg_current_pga   = gain;
    sg_current_speed = speed;
    sg_current_ch    = ch;
    sg_current_vref  = vref;

    /* Construct configuration register byte */
    reg_val |= (vref & 0x01) << 6;
    reg_val |= (speed & 0x03) << 4;
    reg_val |= (gain & 0x03) << 2;
    reg_val |= (ch & 0x03);
    
    cs1238_write_reg(reg_val);
}

/**
 * @brief Select specific input channel on CS1238
 * @param ch Target channel (CS1238_CH1, CS1238_CH2, CS1238_CH_TEMP, CS1238_CH_SHORT)
 */
void cs1238_select_channel(cs1238_ch_t ch)
{
    if (sg_current_ch != ch) {
        cs1238_pga_t target_pga = sg_current_pga;
        if (ch == CS1238_CH1) {
            target_pga = sg_channel_pga[0];
        } else if (ch == CS1238_CH2) {
            target_pga = sg_channel_pga[1];
        }
        cs1238_configure(target_pga, sg_current_speed, ch, sg_current_vref);
        /* Discard 1 transition frame for channel settling */
        (void)cs1238_read_adc_signed(NULL);
    }
}

/**
 * @brief Get numerical multiplier value for PGA enum
 */
uint16_t cs1238_get_pga_multiplier(cs1238_pga_t pga)
{
    switch (pga) {
        case CS1238_PGA_1X:   return 1;
        case CS1238_PGA_2X:   return 2;
        case CS1238_PGA_64X:  return 64;
        case CS1238_PGA_128X: return 128;
        default: return 1;
    }
}

/**
 * @brief Get currently active PGA gain
 */
cs1238_pga_t cs1238_get_current_pga(void)
{
    return sg_current_pga;
}

/**
 * @brief Get currently active channel selection
 */
cs1238_ch_t cs1238_get_current_channel(void)
{
    return sg_current_ch;
}

/**
 * @brief Read ADC value from a specified channel
 * @param ch Selected channel
 * @param success Pointer to status flag
 * @return 32-bit signed ADC value
 */
int32_t cs1238_read_channel_adc(cs1238_ch_t ch, uint8_t *success)
{
    cs1238_select_channel(ch);
    return cs1238_read_adc_signed(success);
}

/**
 * @brief Smart Adaptive Gain Control Algorithm Engine
 * Calculates the optimal PGA gain based on input amplitude and hysteresis limits.
 * @param raw_adc Current 32-bit signed raw ADC reading
 * @param current_pga Currently active PGA gain
 * @return Recommended optimal target PGA gain
 */
static cs1238_pga_t cs1238_evaluate_smart_gain(int32_t raw_adc, cs1238_pga_t current_pga)
{
    int32_t abs_adc = (raw_adc < 0) ? -raw_adc : raw_adc;
    cs1238_pga_t target_pga = current_pga;

    /* 1. Over-Range Protection (Upper Threshold: 7,500,000, ~89.4% Full Scale) */
    if (abs_adc > 7500000) {
        switch (current_pga) {
            case CS1238_PGA_128X: target_pga = CS1238_PGA_64X; break;
            case CS1238_PGA_64X:  target_pga = CS1238_PGA_2X;  break;
            case CS1238_PGA_2X:   target_pga = CS1238_PGA_1X;  break;
            default: break;
        }
    } 
    /* 2. Resolution Enhancement (Under-Range Hysteresis Guardband) */
    else {
        switch (current_pga) {
            case CS1238_PGA_1X:
                /* 1X -> 2X: safe if abs_adc < 3,700,000 (2x gives ~7.4M < 8.38M) */
                if (abs_adc < 3700000) target_pga = CS1238_PGA_2X;
                break;
            case CS1238_PGA_2X:
                /* 2X -> 64X: safe if abs_adc < 200,000 (32x gives ~6.4M < 8.38M) */
                if (abs_adc < 200000) target_pga = CS1238_PGA_64X;
                break;
            case CS1238_PGA_64X:
                /* 64X -> 128X: safe if abs_adc < 3,700,000 (2x gives ~7.4M < 8.38M) */
                if (abs_adc < 3700000) target_pga = CS1238_PGA_128X;
                break;
            default: break;
        }
    }

    return target_pga;
}

/**
 * @brief Read specified channel with Smart Adaptive Gain Control
 * Features per-channel memory, threshold hysteresis, and transition frame flush.
 * @param ch Channel to read (CS1238_CH1 or CS1238_CH2)
 * @param success Pointer to status flag
 * @param out_pga Pointer to store the active PGA gain used
 * @return 32-bit signed ADC value
 */
int32_t cs1238_read_channel_smart_auto_range(cs1238_ch_t ch, uint8_t *success, cs1238_pga_t *out_pga)
{
    uint8_t ch_idx = (ch == CS1238_CH2) ? 1 : 0;
    cs1238_pga_t desired_pga = (ch <= CS1238_CH2) ? sg_channel_pga[ch_idx] : sg_current_pga;
    int32_t adc_val = 0;
    cs1238_pga_t target_pga = desired_pga;

    /* Ensure target channel and PGA are selected */
    if (sg_current_ch != ch || sg_current_pga != desired_pga) {
        cs1238_configure(desired_pga, sg_current_speed, ch, sg_current_vref);
        /* Flush 1 invalid transition frame due to filter settling */
        (void)cs1238_read_adc_signed(NULL);
    }

    /* Read ADC sample */
    adc_val = cs1238_read_adc_signed(success);
    if (success && *success == 0) {
        if (out_pga) *out_pga = sg_current_pga;
        return 0;
    }

    /* Smart Gain Evaluation for CH1 / CH2 */
    if (ch <= CS1238_CH2) {
        target_pga = cs1238_evaluate_smart_gain(adc_val, sg_current_pga);
        if (target_pga != sg_current_pga) {
            /* Reconfigure chip with updated target PGA */
            cs1238_configure(target_pga, sg_current_speed, ch, sg_current_vref);
            sg_channel_pga[ch_idx] = target_pga;

            /* Flush transition frame and take fresh measurement */
            (void)cs1238_read_adc_signed(NULL);
            adc_val = cs1238_read_adc_signed(success);
        }
    }

    if (out_pga) {
        *out_pga = sg_current_pga;
    }

    return adc_val;
}

/**
 * @brief Convert raw ADC signed value to physical voltage in millivolts (mV)
 * @param raw_val 24-bit signed raw ADC reading
 * @param pga Active PGA setting
 * @param vref_volts Reference voltage in volts (e.g. 3.3f)
 * @return Calculated voltage in millivolts (mV)
 */
float cs1238_raw_to_voltage_mv(int32_t raw_val, cs1238_pga_t pga, float vref_volts)
{
    uint16_t mult = cs1238_get_pga_multiplier(pga);
    float full_scale_mv = (vref_volts * 1000.0f / 2.0f) / (float)mult;
    return ((float)raw_val / 8388607.0f) * full_scale_mv;
}

/**
 * @brief Read all channels sequentially (CH1, CH2, Temp) with Smart Adaptive Gain Control
 * @param data Output data structure pointer
 * @param vref_volts VREF voltage in Volts (e.g. 3.3f)
 */
void cs1238_read_all_channels(cs1238_data_t *data, float vref_volts)
{
    uint8_t flag1 = 0, flag2 = 0, flagt = 0;

    if (!data) return;

    /* 1. Read Channel 1 with Smart Adaptive Gain Control */
    data->raw_ch1 = cs1238_read_channel_smart_auto_range(CS1238_CH1, &flag1, &data->pga_ch1);
    data->volt_ch1_mv = cs1238_raw_to_voltage_mv(data->raw_ch1, data->pga_ch1, vref_volts);

    /* 2. Read Channel 2 with Smart Adaptive Gain Control */
    data->raw_ch2 = cs1238_read_channel_smart_auto_range(CS1238_CH2, &flag2, &data->pga_ch2);
    data->volt_ch2_mv = cs1238_raw_to_voltage_mv(data->raw_ch2, data->pga_ch2, vref_volts);

    /* 3. Read Internal Temperature Sensor */
    data->raw_temp = cs1238_read_channel_adc(CS1238_CH_TEMP, &flagt);

    /* Switch back to CH1 for default monitoring */
    cs1238_select_channel(CS1238_CH1);

    data->success = (flag1 && flag2 && flagt) ? 1 : 0;
}
