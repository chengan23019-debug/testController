#include "bsp_cs1237.h"
#include <stdlib.h>

/* Global state tracking for CS1237 */
static cs1237_pga_t   sg_current_pga   = CS1237_PGA_128X;
static cs1237_speed_t sg_current_speed = CS1237_SPEED_640HZ;
static cs1237_ch_t    sg_current_ch    = CS1237_CH_A;
static cs1237_vref_t  sg_current_vref  = CS1237_VREF_ON;

/* 16点环形缓冲区实例 */
static torque_ring_buffer_t g_torque_buf = {0};

/* Clock delay helper */
static void cs1237_delay_us(uint32_t us)
{
    /* GD32F470 at 240MHz runs 240 cycles per microsecond.
       We use a multiplier (120) to ensure SCLK high/low time 
       is safely above the 455ns minimum limit even under compiler optimizations. */
    volatile uint32_t count = us * 120;
    while (count--) {
        __NOP();
    }
}

/* Configure DOUT/DRDY pin as INPUT (with pull-up) */
static void cs1237_dout_as_input(void)
{
    gpio_mode_set(CS1237_DOUT_PORT, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, CS1237_DOUT_PIN);
}

/* Configure DOUT/DRDY pin as OUTPUT (push-pull) */
static void cs1237_dout_as_output(void)
{
    gpio_mode_set(CS1237_DOUT_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, CS1237_DOUT_PIN);
    gpio_output_options_set(CS1237_DOUT_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, CS1237_DOUT_PIN);
}

/* CLK line helper macros */
#define CS1237_CLK_H()    gpio_bit_set(CS1237_CLK_PORT, CS1237_CLK_PIN)
#define CS1237_CLK_L()    gpio_bit_reset(CS1237_CLK_PORT, CS1237_CLK_PIN)

/* DOUT line helper macros */
#define CS1237_DOUT_H()   gpio_bit_set(CS1237_DOUT_PORT, CS1237_DOUT_PIN)
#define CS1237_DOUT_L()   gpio_bit_reset(CS1237_DOUT_PORT, CS1237_DOUT_PIN)
#define CS1237_DOUT_R()   gpio_input_bit_get(CS1237_DOUT_PORT, CS1237_DOUT_PIN)

/**
 * @brief Wait for DOUT/DRDY pin to fall LOW (Data Ready) with configurable millisecond timeout
 * @param timeout_ms Maximum time to wait in milliseconds (e.g. 10ms for 640Hz data rate)
 * @return 1 if DRDY went LOW, 0 if timed out
 */
static uint8_t cs1237_wait_drdy_low(uint32_t timeout_ms)
{
    uint32_t max_us = timeout_ms * 1000U;
    if (max_us > 1000U) {
        max_us = 1000U; /* Cap timeout to 1ms to prevent task starvation */
    }
    while (CS1237_DOUT_R() == SET) {
        if (max_us < 5U) {
            return 0U; /* Timed out */
        }
        cs1237_delay_us(5U);
        max_us -= 5U;
    }
    return 1U;
}

/**
 * @brief Initialize GPIO pins and default 640Hz config for CS1237
 */
void cs1237_init(void)
{
    /* Enable clocks for GPIO ports */
    rcu_periph_clock_enable(CS1237_CLK_RCU);
    rcu_periph_clock_enable(CS1237_DOUT_RCU);

    /* SCLK as push-pull output, no pull-up/pull-down */
    gpio_mode_set(CS1237_CLK_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, CS1237_CLK_PIN);
    gpio_output_options_set(CS1237_CLK_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, CS1237_CLK_PIN);

    /* SCLK initially low */
    CS1237_CLK_L();

    /* DOUT/DRDY initially as input with pull-up */
    cs1237_dout_as_input();

    /* Force a reset/wake-up sequence to place the chip in a known working state */
    CS1237_CLK_H();
    cs1237_delay_us(200); /* Keep SCLK high for >100us to trigger power-down/reset */
    CS1237_CLK_L();
    cs1237_delay_us(2000); /* Delay 2ms for startup/wake-up stabilization */

    /* Wait up to 50ms for chip power-up conversion completion */
    (void)cs1237_wait_drdy_low(50U);

    /* Default configuration: PGA 128X, 640Hz (Doc requirement), Channel A, VREF On */
    cs1237_configure(CS1237_PGA_128X, CS1237_SPEED_640HZ, CS1237_CH_A, CS1237_VREF_ON);

    /* Initialize EXTI interrupt for DOUT falling edge */
    cs1237_exti_init();
}

/**
 * @brief Initialize EXTI Falling Edge Interrupt for CS1237 DOUT (PF7)
 */
void cs1237_exti_init(void)
{
    /* Enable SYSCFG clock */
    rcu_periph_clock_enable(RCU_SYSCFG);

    /* Connect EXTI Line 7 to GPIO Port F */
    syscfg_exti_line_config(CS1237_EXTI_PORT_SOURCE, CS1237_EXTI_PIN_SOURCE);

    /* Configure EXTI Line 7 for Falling Edge */
    exti_init(CS1237_EXTI_LINE, EXTI_INTERRUPT, EXTI_TRIG_FALLING);
    exti_interrupt_flag_clear(CS1237_EXTI_LINE);

    /* Enable NVIC IRQ with FreeRTOS-safe priority (Priority 6) */
    nvic_irq_enable(CS1237_EXTI_IRQn, 6, 0);
}

/**
 * @brief Fast 27-pulse read of 24-bit ADC data (MSB first) + 3 release pulses
 *        Designed for ISR or high-speed polling execution (~15us total)
 * @return 32-bit signed ADC value
 */
int32_t cs1237_read_27pulse_fast(void)
{
    int32_t raw_data = 0;
    int i;

    /* Read 24-bit data */
    for (i = 0; i < 24; i++) {
        CS1237_CLK_H();
        cs1237_delay_us(1);
        raw_data = (raw_data << 1);
        if (CS1237_DOUT_R() == SET) {
            raw_data |= 1;
        }
        CS1237_CLK_L();
        cs1237_delay_us(1);
    }

    /* Send 3 extra clock pulses (pulses 25, 26, 27) to release DOUT to high */
    for (i = 0; i < 3; i++) {
        CS1237_CLK_H();
        cs1237_delay_us(1);
        CS1237_CLK_L();
        cs1237_delay_us(1);
    }

    /* Sign extension from 24-bit to 32-bit */
    if (raw_data & 0x800000) {
        raw_data |= 0xFF000000;
    }

    return raw_data;
}

/**
 * @brief Push raw ADC sample into 16-point ring buffer (Called from ISR or 640Hz poll)
 */
void torque_filter_push_isr(int32_t raw_adc)
{
    if (g_torque_buf.count >= TORQUE_RING_BUF_SIZE) {
        g_torque_buf.sum -= g_torque_buf.buffer[g_torque_buf.head];
    } else {
        g_torque_buf.count++;
    }

    g_torque_buf.buffer[g_torque_buf.head] = raw_adc;
    g_torque_buf.sum += raw_adc;
    g_torque_buf.head = (g_torque_buf.head + 1U) % TORQUE_RING_BUF_SIZE;
}

/**
 * @brief Fetch 20ms oversampled arithmetic average from ring buffer (Called from 50Hz task)
 * @return 32-bit averaged signed ADC raw value
 */
int32_t torque_filter_get_averaged(void)
{
    int32_t avg_val = 0;
    
    exti_interrupt_disable(CS1237_EXTI_LINE);
    if (g_torque_buf.count > 0) {
        avg_val = (int32_t)(g_torque_buf.sum / (int64_t)g_torque_buf.count);
    }
    exti_interrupt_enable(CS1237_EXTI_LINE);

    return avg_val;
}

/**
 * @brief Get filtered physical torque in N.m based on 20ms oversampling
 */
float cs1237_get_filtered_torque_nm(float coeff)
{
    float mv = 0.0f;
    uint8_t success = 0;
    int32_t avg_raw = torque_filter_get_averaged();
    if (avg_raw == 0 && g_torque_buf.count == 0) {
        /* If ring buffer has not received EXTI interrupts yet, do a fast synchronous read */
        avg_raw = cs1237_read_adc_signed(&success);
        if (success) {
            torque_filter_push_isr(avg_raw);
        }
    }
    mv = cs1237_raw_to_voltage_mv(avg_raw, sg_current_pga, 3.3f);
    return mv * coeff;
}

/**
 * @brief EXTI5_9 IRQ Handler subroutine for CS1237 DOUT falling edge
 */
void cs1237_exti_isr(void)
{
    if (RESET != exti_interrupt_flag_get(CS1237_EXTI_LINE)) {
        /* 1. 立即暂时关闭 EXTI 中断，防止在移位读取 24bit 数据时 DOUT 引脚跳变产生 EXTI 嵌套中断风暴 */
        exti_interrupt_disable(CS1237_EXTI_LINE);
        exti_interrupt_flag_clear(CS1237_EXTI_LINE);

        if (CS1237_DOUT_R() == RESET) {
            int32_t raw = cs1237_read_27pulse_fast();
            torque_filter_push_isr(raw);
        }

        /* 2. 发送完 27 个脉冲后 DOUT 已释放为高电平，彻底清除期间产生的所有挂起标志并重新使能 */
        exti_interrupt_flag_clear(CS1237_EXTI_LINE);
        exti_interrupt_enable(CS1237_EXTI_LINE);
    }
}

/**
 * @brief Read raw 24-bit data from CS1237 ADC
 * @param success Pointer to status flag (1 = success, 0 = timeout/error)
 * @return 24-bit unsigned raw data
 */
int32_t cs1237_read_adc_raw(uint8_t *success)
{
    int32_t raw_data = 0;
    int i;

    /* Wait for DOUT/DRDY to go LOW (data ready) up to 20ms */
    if (!cs1237_wait_drdy_low(20U)) {
        if (success) {
            *success = 0U;
        }
        return 0;
    }

    if (success) {
        *success = 1U;
    }

    /* Read 24-bit data */
    for (i = 0; i < 24; i++) {
        CS1237_CLK_H();
        cs1237_delay_us(1);
        raw_data = (raw_data << 1);
        if (CS1237_DOUT_R() == SET) {
            raw_data |= 1;
        }
        CS1237_CLK_L();
        cs1237_delay_us(1);
    }

    /* Send 3 extra clock pulses (pulses 25, 26, 27) */
    for (i = 0; i < 3; i++) {
        CS1237_CLK_H();
        cs1237_delay_us(1);
        CS1237_CLK_L();
        cs1237_delay_us(1);
    }

    return raw_data;
}

/**
 * @brief Read 24-bit signed data from CS1237 ADC (includes sign extension)
 * @param success Pointer to status flag (1 = success, 0 = timeout/error)
 * @return 32-bit signed ADC value
 */
int32_t cs1237_read_adc_signed(uint8_t *success)
{
    int32_t val = cs1237_read_adc_raw(success);
    
    /* Sign extension from 24-bit to 32-bit */
    if (val & 0x800000) {
        val |= 0xFF000000;
    }
    
    return val;
}

/**
 * @brief Write data to CS1237 configuration register
 * @param reg_val New register byte to write
 */
void cs1237_write_reg(uint8_t reg_val)
{
    int i;
    uint8_t cmd = CS1237_CMD_WRITE;

    /* 1. Wait for DOUT/DRDY to go LOW */
    if (!cs1237_wait_drdy_low(20U)) {
        return; /* Timeout occurred */
    }

    /* 2. Clock out 24 dummy/ADC pulses (pulses 1-24) */
    for (i = 0; i < 24; i++) {
        CS1237_CLK_H();
        cs1237_delay_us(1);
        CS1237_CLK_L();
        cs1237_delay_us(1);
    }

    /* 3. Clock 3 dummy pulses (pulses 25, 26, 27) */
    for (i = 0; i < 3; i++) {
        CS1237_CLK_H();
        cs1237_delay_us(1);
        CS1237_CLK_L();
        cs1237_delay_us(1);
    }

    /* 4. Configure DOUT pin as output to write the command */
    cs1237_dout_as_output();

    /* 5. Clock 2 transit pulses (pulses 28, 29), keep DOUT low */
    CS1237_DOUT_L();
    for (i = 0; i < 2; i++) {
        CS1237_CLK_H();
        cs1237_delay_us(1);
        CS1237_CLK_L();
        cs1237_delay_us(1);
    }

    /* 6. Shift out the 7-bit WRITE command (0x65, MSB first, pulses 30-36) */
    for (i = 0; i < 7; i++) {
        CS1237_CLK_H();
        if ((cmd >> (6 - i)) & 0x01) {
            CS1237_DOUT_H();
        } else {
            CS1237_DOUT_L();
        }
        cs1237_delay_us(1);
        CS1237_CLK_L();
        cs1237_delay_us(1);
    }

    /* 7. Clock 37 (transit pulse, DOUT remains output) */
    CS1237_CLK_H();
    cs1237_delay_us(1);
    CS1237_CLK_L();
    cs1237_delay_us(1);

    /* 8. Shift out the 8-bit register data (MSB first, pulses 38-45) */
    for (i = 0; i < 8; i++) {
        CS1237_CLK_H();
        if ((reg_val >> (7 - i)) & 0x01) {
            CS1237_DOUT_H();
        } else {
            CS1237_DOUT_L();
        }
        cs1237_delay_us(1);
        CS1237_CLK_L();
        cs1237_delay_us(1);
    }

    /* 9. Clock 46 (finalize clock) */
    CS1237_CLK_H();
    cs1237_delay_us(1);
    CS1237_CLK_L();
    cs1237_delay_us(1);

    /* 10. Restore DOUT as input for future readings */
    cs1237_dout_as_input();
}

/**
 * @brief Read data from CS1237 configuration register
 * @return The 8-bit register byte read
 */
uint8_t cs1237_read_reg(void)
{
    uint8_t reg_val = 0;
    int i;
    uint8_t cmd = CS1237_CMD_READ;

    /* 1. Wait for DOUT/DRDY to go LOW */
    if (!cs1237_wait_drdy_low(20U)) {
        return 0; /* Timeout occurred */
    }

    /* 2. Clock out 24 dummy/ADC pulses (pulses 1-24) */
    for (i = 0; i < 24; i++) {
        CS1237_CLK_H();
        cs1237_delay_us(1);
        CS1237_CLK_L();
        cs1237_delay_us(1);
    }

    /* 3. Clock 3 dummy pulses (pulses 25, 26, 27) */
    for (i = 0; i < 3; i++) {
        CS1237_CLK_H();
        cs1237_delay_us(1);
        CS1237_CLK_L();
        cs1237_delay_us(1);
    }

    /* 4. Configure DOUT pin as output to write the command */
    cs1237_dout_as_output();

    /* 5. Clock 2 transit pulses (pulses 28, 29), keep DOUT low */
    CS1237_DOUT_L();
    for (i = 0; i < 2; i++) {
        CS1237_CLK_H();
        cs1237_delay_us(1);
        CS1237_CLK_L();
        cs1237_delay_us(1);
    }

    /* 6. Shift out the 7-bit READ command (0x56, MSB first, pulses 30-36) */
    for (i = 0; i < 7; i++) {
        CS1237_CLK_H();
        if ((cmd >> (6 - i)) & 0x01) {
            CS1237_DOUT_H();
        } else {
            CS1237_DOUT_L();
        }
        cs1237_delay_us(1);
        CS1237_CLK_L();
        cs1237_delay_us(1);
    }

    /* 7. Clock 37 (transit pulse, switch DOUT back to input mode) */
    CS1237_CLK_H();
    cs1237_delay_us(1);
    CS1237_CLK_L();
    cs1237_dout_as_input(); /* Switch to input mode to read the response */
    cs1237_delay_us(1);

    /* 8. Shift in the 8-bit register data (MSB first, pulses 38-45) */
    for (i = 0; i < 8; i++) {
        CS1237_CLK_H();
        cs1237_delay_us(1);
        reg_val = (reg_val << 1);
        if (CS1237_DOUT_R() == SET) {
            reg_val |= 1;
        }
        CS1237_CLK_L();
        cs1237_delay_us(1);
    }

    /* 9. Clock 46 (finalize clock) */
    CS1237_CLK_H();
    cs1237_delay_us(1);
    CS1237_CLK_L();
    cs1237_delay_us(1);

    return reg_val;
}

/**
 * @brief Configure CS1237 parameters high-level helper
 * @param gain PGA gain (1x, 2x, 64x, 128x)
 * @param speed Sampling rate (10Hz, 40Hz, 640Hz, 1280Hz)
 * @param ch Input channel (AIN, Temp Sensor, Short)
 * @param vref VREF status (On, Off)
 */
void cs1237_configure(cs1237_pga_t gain, cs1237_speed_t speed, cs1237_ch_t ch, cs1237_vref_t vref)
{
    uint8_t reg_val = 0;
    
    sg_current_pga   = gain;
    sg_current_speed = speed;
    sg_current_ch    = ch;
    sg_current_vref  = vref;

    /* Construct configuration register byte:
       Bit 7: Reserved (0)
       Bit 6: REFO_OFF (vref: 0 = on, 1 = off)
       Bit 5-4: SPEED
       Bit 3-2: PGA
       Bit 1-0: CH_SEL */
    reg_val |= (vref & 0x01) << 6;
    reg_val |= (speed & 0x03) << 4;
    reg_val |= (gain & 0x03) << 2;
    reg_val |= (ch & 0x03);
    
    cs1237_write_reg(reg_val);
}

/**
 * @brief Get numerical multiplier value for PGA enum
 */
uint16_t cs1237_get_pga_multiplier(cs1237_pga_t pga)
{
    switch (pga) {
        case CS1237_PGA_1X:   return 1;
        case CS1237_PGA_2X:   return 2;
        case CS1237_PGA_64X:  return 64;
        case CS1237_PGA_128X: return 128;
        default: return 1;
    }
}

/**
 * @brief Get currently active PGA gain
 */
cs1237_pga_t cs1237_get_current_pga(void)
{
    return sg_current_pga;
}

/**
 * @brief Read CS1237 ADC value with automatic PGA gain range adjustment
 * @param success Pointer to status flag (1 = success, 0 = timeout/error)
 * @param out_pga Pointer to store the active PGA gain used for the returned sample
 * @return 32-bit signed ADC value
 */
int32_t cs1237_read_adc_auto_range(uint8_t *success, cs1237_pga_t *out_pga)
{
    int32_t adc_val = cs1237_read_adc_signed(success);
    int32_t abs_val = 0;
    cs1237_pga_t target_pga = sg_current_pga;
    uint8_t pga_changed = 0;

    if (success && *success == 0) {
        if (out_pga) *out_pga = sg_current_pga;
        return 0;
    }

    abs_val = (adc_val < 0) ? -adc_val : adc_val;

    if (abs_val > 7500000) {
        /* High Signal: Step DOWN gain */
        if (sg_current_pga == CS1237_PGA_128X) {
            target_pga = CS1237_PGA_64X;
            pga_changed = 1;
        } else if (sg_current_pga == CS1237_PGA_64X) {
            target_pga = CS1237_PGA_2X;
            pga_changed = 1;
        } else if (sg_current_pga == CS1237_PGA_2X) {
            target_pga = CS1237_PGA_1X;
            pga_changed = 1;
        }
    } else {
        /* Low Signal: Step UP gain */
        if (sg_current_pga == CS1237_PGA_1X && abs_val < 3500000) {
            target_pga = CS1237_PGA_2X;
            pga_changed = 1;
        } else if (sg_current_pga == CS1237_PGA_2X && abs_val < 100000) {
            target_pga = CS1237_PGA_64X;
            pga_changed = 1;
        } else if (sg_current_pga == CS1237_PGA_64X && abs_val < 3500000) {
            target_pga = CS1237_PGA_128X;
            pga_changed = 1;
        }
    }

    if (pga_changed) {
        /* Reconfigure CS1237 with new PGA setting */
        cs1237_configure(target_pga, sg_current_speed, sg_current_ch, sg_current_vref);

        /* Discard 1 transition frame for PGA gain settling */
        (void)cs1237_read_adc_signed(NULL);

        /* Read fresh measurement with updated gain */
        adc_val = cs1237_read_adc_signed(success);
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
float cs1237_raw_to_voltage_mv(int32_t raw_val, cs1237_pga_t pga, float vref_volts)
{
    uint16_t mult = cs1237_get_pga_multiplier(pga);
    /* Full scale voltage for differential input = (Vref / 2.0) / Gain */
    float full_scale_mv = (vref_volts * 1000.0f / 2.0f) / (float)mult;
    return ((float)raw_val / 8388607.0f) * full_scale_mv;
}
