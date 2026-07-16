#include "bsp_hlw8112.h"
#include <stdio.h>

/* Calibration parameters read back from the chip */
static uint16_t u16_rms_iac = 0;
static uint16_t u16_rms_ibc = 0;
static uint16_t u16_rms_uc = 0;
static uint16_t u16_power_pac = 0;
static uint16_t u16_power_pbc = 0;
static uint16_t u16_power_sc = 0;
static uint16_t u16_energy_ac = 0;
static uint16_t u16_energy_bc = 0;
static uint16_t u16_checksum_reg = 0;
static uint16_t u16_checksum_calc = 0;

/* Microsecond delay helper for GD32F470 at 240MHz */
static void hlw8112_delay_us(uint32_t us)
{
    volatile uint32_t count = us * 120;
    while (count--) {
        __NOP();
    }
}

/* GPIO control macros */
#define CS_H()     gpio_bit_set(HLW8112_GPIO_PORT, HLW8112_CS_PIN)
#define CS_L()     gpio_bit_reset(HLW8112_GPIO_PORT, HLW8112_CS_PIN)

#define SCLK_H()   gpio_bit_set(HLW8112_GPIO_PORT, HLW8112_SCLK_PIN)
#define SCLK_L()   gpio_bit_reset(HLW8112_GPIO_PORT, HLW8112_SCLK_PIN)

#define SDI_H()    gpio_bit_set(HLW8112_GPIO_PORT, HLW8112_SDI_PIN)
#define SDI_L()    gpio_bit_reset(HLW8112_GPIO_PORT, HLW8112_SDI_PIN)

#define SDO_R()    gpio_input_bit_get(HLW8112_GPIO_PORT, HLW8112_SDO_PIN)

#define EN_H()     gpio_bit_set(HLW8112_GPIO_PORT, HLW8112_EN_PIN)
#define EN_L()     gpio_bit_reset(HLW8112_GPIO_PORT, HLW8112_EN_PIN)

/* Software SPI Write Byte */
static void hlw8112_spi_write_byte(uint8_t data)
{
    uint8_t i;
    for (i = 0; i < 8; i++) {
        if (data & (0x80 >> i)) {
            SDI_H();
        } else {
            SDI_L();
        }
        
        SCLK_H();
        hlw8112_delay_us(10);
        SCLK_L();
        hlw8112_delay_us(10);
    }
}

/* Software SPI Read Byte */
static uint8_t hlw8112_spi_read_byte(void)
{
    uint8_t i;
    uint8_t data = 0;
    for (i = 0; i < 8; i++) {
        data <<= 1;
        
        SCLK_H();
        hlw8112_delay_us(10);
        SCLK_L();
        hlw8112_delay_us(10);
        
        if (SDO_R() == SET) {
            data |= 0x01;
        }
    }
    return data;
}

/* Read registers via Software SPI */
static uint32_t hlw8112_read_reg_bytes(uint8_t reg_addr, uint8_t bytes_len)
{
    uint8_t i;
    uint32_t val = 0;
    
    CS_L();
    hlw8112_delay_us(2);
    
    /* Command byte for reading: Bit 7 is 0, bits 6:0 is address */
    hlw8112_spi_write_byte(reg_addr & 0x7F);
    
    /* Read bytes, MSB first */
    for (i = 0; i < bytes_len; i++) {
        uint8_t byte_val = hlw8112_spi_read_byte();
        val = (val << 8) | byte_val;
    }
    
    CS_H();
    hlw8112_delay_us(2);
    
    return val;
}

/* Command helpers for special operations */
static void hlw8112_write_enable(void)
{
    CS_L();
    hlw8112_delay_us(2);
    hlw8112_spi_write_byte(0xEA);
    hlw8112_spi_write_byte(HLW8112_CMD_WRITE_EN);
    CS_H();
    hlw8112_delay_us(2);
}

static void hlw8112_write_disable(void)
{
    CS_L();
    hlw8112_delay_us(2);
    hlw8112_spi_write_byte(0xEA);
    hlw8112_spi_write_byte(HLW8112_CMD_WRITE_DIS);
    CS_H();
    hlw8112_delay_us(2);
}

static void hlw8112_select_channel_a(void)
{
    CS_L();
    hlw8112_delay_us(2);
    hlw8112_spi_write_byte(0xEA);
    hlw8112_spi_write_byte(HLW8112_CMD_SEL_CHA);
    CS_H();
    hlw8112_delay_us(2);
}

/* Initialize HLW8112 GPIO and Registers */
void hlw8112_init(void)
{
    volatile uint32_t delay_cnt;
    uint32_t checksum_sum;

    /* Enable GPIOB clock */
    rcu_periph_clock_enable(HLW8112_GPIO_RCU);
    
    /* Configure output pins: CS, SCLK, SDI, EN as Push-Pull Output */
    gpio_mode_set(HLW8112_GPIO_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, 
                  HLW8112_CS_PIN | HLW8112_SCLK_PIN | HLW8112_SDI_PIN | HLW8112_EN_PIN);
    gpio_output_options_set(HLW8112_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, 
                            HLW8112_CS_PIN | HLW8112_SCLK_PIN | HLW8112_SDI_PIN | HLW8112_EN_PIN);
                            
    /* Configure input pin: SDO as Input with Pull-up */
    gpio_mode_set(HLW8112_GPIO_PORT, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, HLW8112_SDO_PIN);
    
    /* Set EN Pin High to enable the chip */
    EN_H();
    
    /* Set SCLK initially Low, CS High */
    CS_H();
    SCLK_L();
    SDI_L();
    
    /* Delay for chip startup stabilization (approx 100ms) */
    delay_cnt = 2000000;
    while (delay_cnt--) { __NOP(); }
    
    /* Start Initialization Register Configuration */
    hlw8112_write_enable();
    hlw8112_select_channel_a();
    
    /* Write SYSCON Reg: Enables ADCs, PGA Current A = 16, PGA Current B = 16, PGA Voltage = 1 */
    CS_L();
    hlw8112_spi_write_byte(REG_SYSCON_ADDR | 0x80);
    hlw8112_spi_write_byte(0x0F); /* High Byte */
    hlw8112_spi_write_byte(0x04); /* Low Byte */
    CS_H();
    hlw8112_delay_us(2);
    
    /* Write EMUCON Reg: Enables accumulators */
    CS_L();
    hlw8112_spi_write_byte(REG_EMUCON_ADDR | 0x80);
    hlw8112_spi_write_byte(0x10); /* High Byte */
    hlw8112_spi_write_byte(0x03); /* Low Byte */
    CS_H();
    hlw8112_delay_us(2);
    
    /* Write EMUCON2 Reg: Enables zero crossing, waveform sampling */
    CS_L();
    hlw8112_spi_write_byte(REG_EMUCON2_ADDR | 0x80);
    hlw8112_spi_write_byte(0x0F); /* High Byte */
    hlw8112_spi_write_byte(0xFF); /* Low Byte */
    CS_H();
    hlw8112_delay_us(2);
    
    /* Disable interrupts (IE register = 0x0000) */
    CS_L();
    hlw8112_spi_write_byte(REG_IE_ADDR | 0x80);
    hlw8112_spi_write_byte(0x00); /* High Byte */
    hlw8112_spi_write_byte(0x00); /* Low Byte */
    CS_H();
    hlw8112_delay_us(2);
    
    hlw8112_write_disable();
    
    /* Read calibration coefficients */
    u16_rms_iac   = (uint16_t)hlw8112_read_reg_bytes(REG_RMS_IAC_ADDR, 2);
    u16_rms_ibc   = (uint16_t)hlw8112_read_reg_bytes(REG_RMS_IBC_ADDR, 2);
    u16_rms_uc    = (uint16_t)hlw8112_read_reg_bytes(REG_RMS_UC_ADDR, 2);
    u16_power_pac = (uint16_t)hlw8112_read_reg_bytes(REG_POWER_PAC_ADDR, 2);
    u16_power_pbc = (uint16_t)hlw8112_read_reg_bytes(REG_POWER_PBC_ADDR, 2);
    u16_power_sc  = (uint16_t)hlw8112_read_reg_bytes(REG_POWER_SC_ADDR, 2);
    u16_energy_ac = (uint16_t)hlw8112_read_reg_bytes(REG_ENERGY_AC_ADDR, 2);
    u16_energy_bc = (uint16_t)hlw8112_read_reg_bytes(REG_ENERGY_BC_ADDR, 2);
    u16_checksum_reg = (uint16_t)hlw8112_read_reg_bytes(REG_CHECKSUM_ADDR, 2);
    
    /* Calculate parameters checksum */
    checksum_sum = 0xFFFF + u16_rms_iac + u16_rms_ibc + u16_rms_uc + 
                   u16_power_pac + u16_power_pbc + u16_power_sc + 
                   u16_energy_ac + u16_energy_bc;
    u16_checksum_calc = (uint16_t)(~checksum_sum & 0xFFFF);
}

/* Check if checksum matches register checksum */
uint8_t hlw8112_check_calibration(void)
{
    return (u16_checksum_calc == u16_checksum_reg) ? 1 : 0;
}

/* Read and scale all parameters */
void hlw8112_read_data(hlw8112_data_t *data)
{
    uint32_t raw_freq;
    uint32_t raw_u;
    uint32_t raw_ia;
    uint32_t raw_ib;
    uint32_t raw_pa;
    uint32_t raw_pb;
    uint32_t raw_ea;
    uint32_t raw_eb;
    uint32_t raw_pf;
    uint32_t raw_angle;
    float pf_val;
    float angle_val;

    if (data == NULL) return;
    
    /* 1. Frequency (Hz) */
    raw_freq = hlw8112_read_reg_bytes(REG_UFREQ_ADDR, 2);
    if (raw_freq > 0) {
        data->frequency = 3579545.0f / (8.0f * (float)raw_freq);
    } else {
        data->frequency = 0.0f;
    }
    
    /* 2. Voltage RMS (V) */
    raw_u = hlw8112_read_reg_bytes(REG_RMSU_ADDR, 3);
    if ((raw_u & 0x800000) == 0x800000) {
        data->voltage = 0.0f;
    } else {
        /* Formula: U = (raw_u * u16_rms_uc) / (2^22 * 100) */
        data->voltage = ((float)raw_u * (float)u16_rms_uc) / 4194304.0f / 100.0f;
    }
    
    /* 3. Current Channel A RMS (A) */
    raw_ia = hlw8112_read_reg_bytes(REG_RMSIA_ADDR, 3);
    if ((raw_ia & 0x800000) == 0x800000) {
        data->current_a = 0.0f;
    } else {
        /* Formula: I = (raw_ia * u16_rms_iac) / (2^23 * 1000) */
        data->current_a = ((float)raw_ia * (float)u16_rms_iac) / 8388608.0f / 1000.0f;
    }
    
    /* 4. Current Channel B RMS (A) */
    raw_ib = hlw8112_read_reg_bytes(REG_RMSIB_ADDR, 3);
    if ((raw_ib & 0x800000) == 0x800000) {
        data->current_b = 0.0f;
    } else {
        data->current_b = ((float)raw_ib * (float)u16_rms_ibc) / 8388608.0f / 1000.0f;
    }
    
    /* 5. Active Power Channel A (W) */
    raw_pa = hlw8112_read_reg_bytes(REG_POWER_PA_ADDR, 4);
    if (raw_pa > 0x80000000) {
        uint32_t abs_pa = ~raw_pa;
        data->active_power_a = ((float)abs_pa * (float)u16_power_pac) / 2147483648.0f;
    } else {
        data->active_power_a = ((float)raw_pa * (float)u16_power_pac) / 2147483648.0f;
    }
    
    /* 6. Active Power Channel B (W) */
    raw_pb = hlw8112_read_reg_bytes(REG_POWER_PB_ADDR, 4);
    if (raw_pb > 0x80000000) {
        uint32_t abs_pb = ~raw_pb;
        data->active_power_b = ((float)abs_pb * (float)u16_power_pbc) / 2147483648.0f;
    } else {
        data->active_power_b = ((float)raw_pb * (float)u16_power_pbc) / 2147483648.0f;
    }
    
    /* 7. Active Energy Channel A (kWh) */
    raw_ea = hlw8112_read_reg_bytes(REG_ENERGY_PA_ADDR, 3);
    /* Formula: E = (raw_ea * u16_energy_ac) / 2^29 */
    data->active_energy_a = ((float)raw_ea * (float)u16_energy_ac) / 536870912.0f;
    
    /* 8. Active Energy Channel B (kWh) */
    raw_eb = hlw8112_read_reg_bytes(REG_ENERGY_PB_ADDR, 3);
    data->active_energy_b = ((float)raw_eb * (float)u16_energy_bc) / 536870912.0f;
    
    /* 9. Power Factor */
    raw_pf = hlw8112_read_reg_bytes(REG_PF_ADDR, 3);
    if (raw_pf > 0x800000) {
        pf_val = (float)(0xFFFFFF - raw_pf + 1) / 8388607.0f;
    } else {
        pf_val = (float)raw_pf / 8388607.0f;
    }
    if (data->active_power_a < 0.3f) {
        pf_val = 0.0f;
    }
    data->power_factor = pf_val;
    
    /* 10. Phase Angle (Degrees) */
    raw_angle = hlw8112_read_reg_bytes(REG_ANGLE_ADDR, 2);
    if (data->frequency < 55.0f) {
        angle_val = (float)raw_angle * 0.0805f;
    } else {
        angle_val = (float)raw_angle * 0.0965f;
    }
    if (data->active_power_a < 0.5f) {
        angle_val = 0.0f;
    }
    data->phase_angle = angle_val;
}
