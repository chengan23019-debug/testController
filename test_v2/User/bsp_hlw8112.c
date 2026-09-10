/*!
    \file    bsp_hlw8112.c
    \brief   HLW8112 AC Energy Metering Driver for GD32F470 (FreeRTOS 50Hz Fast SPI)
*/

#include "bsp_hlw8112.h"
#include <stdio.h>
#include <string.h>

/* 芯片出厂校准参数（在初始化时自动从 HLW8112 读出） */
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

/* 坏帧拦截保持缓存 (Sanity Guard Cache) */
static hlw8112_data_t s_last_valid_hlw_data = {0};
static uint8_t        s_hlw_data_valid = 0;

/* GD32F470 240MHz 软件 SPI 微秒级延时助手 (优化至 1us 级别) */
static void hlw8112_delay_us(uint32_t us)
{
    volatile uint32_t count = us * 30; /* ~1us at 240MHz */
    while (count--) {
        __NOP();
    }
}

/* GPIO 引脚操作宏 */
#define CS_H()     gpio_bit_set(HLW8112_GPIO_PORT, HLW8112_CS_PIN)
#define CS_L()     gpio_bit_reset(HLW8112_GPIO_PORT, HLW8112_CS_PIN)

#define SCLK_H()   gpio_bit_set(HLW8112_GPIO_PORT, HLW8112_SCLK_PIN)
#define SCLK_L()   gpio_bit_reset(HLW8112_GPIO_PORT, HLW8112_SCLK_PIN)

#define SDI_H()    gpio_bit_set(HLW8112_GPIO_PORT, HLW8112_SDI_PIN)
#define SDI_L()    gpio_bit_reset(HLW8112_GPIO_PORT, HLW8112_SDI_PIN)

#define SDO_R()    gpio_input_bit_get(HLW8112_GPIO_PORT, HLW8112_SDO_PIN)

#define EN_H()     gpio_bit_set(HLW8112_GPIO_PORT, HLW8112_EN_PIN)
#define EN_L()     gpio_bit_reset(HLW8112_GPIO_PORT, HLW8112_EN_PIN)

/**
 * @brief  软件 SPI 发送单字节 (MSB 高位在前, 1us 时钟脉冲)
 */
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
        hlw8112_delay_us(1);
        SCLK_L();
        hlw8112_delay_us(1);
    }
}

/**
 * @brief  软件 SPI 接收单字节 (MSB 高位在前, 1us 时钟脉冲)
 */
static uint8_t hlw8112_spi_read_byte(void)
{
    uint8_t i;
    uint8_t data = 0;
    for (i = 0; i < 8; i++) {
        data <<= 1;
        
        SCLK_H();
        hlw8112_delay_us(1);
        SCLK_L();
        hlw8112_delay_us(1);
        
        if (SDO_R() == SET) {
            data |= 0x01;
        }
    }
    return data;
}

/**
 * @brief  向 HLW8112 读取指定字节数的寄存器
 * @param  reg_addr: 寄存器地址
 * @param  bytes_len: 需读取的字节数 (2, 3, 或 4)
 * @retval 拼接后的寄存器原始数值 (raw value)
 */
static uint32_t hlw8112_read_reg_bytes(uint8_t reg_addr, uint8_t bytes_len)
{
    uint8_t i;
    uint32_t val = 0;
    
    CS_L();
    hlw8112_delay_us(1);
    
    /* 读寄存器命令：Bit 7 为 0，Bits 6:0 为寄存器地址 */
    hlw8112_spi_write_byte(reg_addr & 0x7F);
    
    /* 连续按 MSB 顺序读取字节 */
    for (i = 0; i < bytes_len; i++) {
        uint8_t byte_val = hlw8112_spi_read_byte();
        val = (val << 8) | byte_val;
    }
    
    CS_H();
    hlw8112_delay_us(1);
    
    return val;
}

/**
 * @brief  解锁 HLW8112 寄存器写入保护
 */
static void hlw8112_write_enable(void)
{
    CS_L();
    hlw8112_delay_us(1);
    hlw8112_spi_write_byte(0xEA);
    hlw8112_spi_write_byte(HLW8112_CMD_WRITE_EN);
    CS_H();
    hlw8112_delay_us(1);
}

/**
 * @brief  锁定 HLW8112 寄存器写入保护
 */
static void hlw8112_write_disable(void)
{
    CS_L();
    hlw8112_delay_us(1);
    hlw8112_spi_write_byte(0xEA);
    hlw8112_spi_write_byte(HLW8112_CMD_WRITE_DIS);
    CS_H();
    hlw8112_delay_us(1);
}

/**
 * @brief  选择 A 通道进行有功功率计算
 */
static void hlw8112_select_channel_a(void)
{
    CS_L();
    hlw8112_delay_us(1);
    hlw8112_spi_write_byte(0xEA);
    hlw8112_spi_write_byte(HLW8112_CMD_SEL_CHA);
    CS_H();
    hlw8112_delay_us(1);
}

/**
 * @brief  HLW8112 外设硬件与底层寄存器初始化
 */
void hlw8112_init(void)
{
    volatile uint32_t delay_cnt;
    uint32_t checksum_sum;

    /* 使能 GPIOB 时钟 */
    rcu_periph_clock_enable(HLW8112_GPIO_RCU);
    
    /* 配置输出引脚：CS, SCLK, SDI, EN 为推挽输出 50MHz */
    gpio_mode_set(HLW8112_GPIO_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, 
                  HLW8112_CS_PIN | HLW8112_SCLK_PIN | HLW8112_SDI_PIN | HLW8112_EN_PIN);
    gpio_output_options_set(HLW8112_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, 
                            HLW8112_CS_PIN | HLW8112_SCLK_PIN | HLW8112_SDI_PIN | HLW8112_EN_PIN);
                            
    /* 配置输入引脚：SDO 为带上拉输入 */
    gpio_mode_set(HLW8112_GPIO_PORT, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, HLW8112_SDO_PIN);
    
    /* EN 置高使能芯片, SCLK 置低, CS 置高 */
    EN_H();
    CS_H();
    SCLK_L();
    SDI_L();
    
    /* 上电延时稳定 (~50ms) */
    delay_cnt = 1000000;
    while (delay_cnt--) { __NOP(); }
    
    /* 开始写入初始化控制寄存器配置 */
    hlw8112_write_enable();
    hlw8112_select_channel_a();
    
    /* 写入 SYSCON 寄存器: 使能 ADC, A通道 PGA=16, B通道 PGA=16, 电压通道 PGA=1 */
    CS_L();
    hlw8112_spi_write_byte(REG_SYSCON_ADDR | 0x80);
    hlw8112_spi_write_byte(0x0F); /* 高字节 */
    hlw8112_spi_write_byte(0x04); /* 低字节 */
    CS_H();
    hlw8112_delay_us(1);
    
    /* 写入 EMUCON 寄存器: 开启能量累加 */
    CS_L();
    hlw8112_spi_write_byte(REG_EMUCON_ADDR | 0x80);
    hlw8112_spi_write_byte(0x10); /* 高字节 */
    hlw8112_spi_write_byte(0x03); /* 低字节 */
    CS_H();
    hlw8112_delay_us(1);
    
    /* 写入 EMUCON2 寄存器: 开启过零检测与波形采样 */
    CS_L();
    hlw8112_spi_write_byte(REG_EMUCON2_ADDR | 0x80);
    hlw8112_spi_write_byte(0x0F); /* 高字节 */
    hlw8112_spi_write_byte(0xFF); /* 低字节 */
    CS_H();
    hlw8112_delay_us(1);
    
    /* 禁用中断 (IE 寄存器 = 0x0000) */
    CS_L();
    hlw8112_spi_write_byte(REG_IE_ADDR | 0x80);
    hlw8112_spi_write_byte(0x00);
    hlw8112_spi_write_byte(0x00);
    CS_H();
    hlw8112_delay_us(1);
    
    hlw8112_write_disable();
    
    /* 读取芯片内部出厂标定参数 (0x70 ~ 0x77 及 校验和 0x6F) */
    u16_rms_iac   = (uint16_t)hlw8112_read_reg_bytes(REG_RMS_IAC_ADDR, 2);
    u16_rms_ibc   = (uint16_t)hlw8112_read_reg_bytes(REG_RMS_IBC_ADDR, 2);
    u16_rms_uc    = (uint16_t)hlw8112_read_reg_bytes(REG_RMS_UC_ADDR, 2);
    u16_power_pac = (uint16_t)hlw8112_read_reg_bytes(REG_POWER_PAC_ADDR, 2);
    u16_power_pbc = (uint16_t)hlw8112_read_reg_bytes(REG_POWER_PBC_ADDR, 2);
    u16_power_sc  = (uint16_t)hlw8112_read_reg_bytes(REG_POWER_SC_ADDR, 2);
    u16_energy_ac = (uint16_t)hlw8112_read_reg_bytes(REG_ENERGY_AC_ADDR, 2);
    u16_energy_bc = (uint16_t)hlw8112_read_reg_bytes(REG_ENERGY_BC_ADDR, 2);
    u16_checksum_reg = (uint16_t)hlw8112_read_reg_bytes(REG_CHECKSUM_ADDR, 2);
    
    /* 验证芯片标定参数校验和 */
    checksum_sum = 0xFFFF + u16_rms_iac + u16_rms_ibc + u16_rms_uc + 
                   u16_power_pac + u16_power_pbc + u16_power_sc + 
                   u16_energy_ac + u16_energy_bc;
    u16_checksum_calc = (uint16_t)(~checksum_sum & 0xFFFF);
}

/**
 * @brief  检查芯片出厂标定系数校验和是否一致
 * @retval 1 = 校准正常, 0 = 校验失败/未连接
 */
uint8_t hlw8112_check_calibration(void)
{
    return (u16_checksum_calc == u16_checksum_reg && u16_checksum_reg != 0) ? 1 : 0;
}

/**
 * @brief  读取并计算全量交流电测量数据 (含微秒级快速SPI与Sanity Guard坏帧拦截)
 * @param  data: 输出数据结构体指针
 */
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
    hlw8112_data_t cur;

    if (data == NULL) return;

    memset(&cur, 0, sizeof(hlw8112_data_t));
    cur.calib_ok = hlw8112_check_calibration() ? true : false;
    
    /* 1. 电网频率 Frequency (Hz) */
    raw_freq = hlw8112_read_reg_bytes(REG_UFREQ_ADDR, 2);
    if (raw_freq > 0) {
        cur.frequency = 3579545.0f / (8.0f * (float)raw_freq);
    } else {
        cur.frequency = 0.0f;
    }
    
    /* 2. 交流电压有效值 Voltage RMS (V) */
    raw_u = hlw8112_read_reg_bytes(REG_RMSU_ADDR, 3);
    if ((raw_u & 0x800000) == 0x800000) {
        cur.voltage = 0.0f;
    } else {
        cur.voltage = ((float)raw_u * (float)u16_rms_uc) / 4194304.0f / 100.0f;
    }
    
    /* 3. 通道 A 电流有效值 Current Channel A RMS (A) */
    raw_ia = hlw8112_read_reg_bytes(REG_RMSIA_ADDR, 3);
    if ((raw_ia & 0x800000) == 0x800000) {
        cur.current_a = 0.0f;
    } else {
        cur.current_a = ((float)raw_ia * (float)u16_rms_iac) / 8388608.0f / 1000.0f;
    }
    cur.current_a_ma = cur.current_a * 1000.0f;
    
    /* 4. 通道 B 电流有效值 Current Channel B RMS (A) */
    raw_ib = hlw8112_read_reg_bytes(REG_RMSIB_ADDR, 3);
    if ((raw_ib & 0x800000) == 0x800000) {
        cur.current_b = 0.0f;
    } else {
        cur.current_b = ((float)raw_ib * (float)u16_rms_ibc) / 8388608.0f / 1000.0f;
    }
    
    /* 5. 通道 A 有功功率 Active Power Channel A (W) */
    raw_pa = hlw8112_read_reg_bytes(REG_POWER_PA_ADDR, 4);
    if (raw_pa > 0x80000000) {
        uint32_t abs_pa = ~raw_pa;
        cur.active_power_a = ((float)abs_pa * (float)u16_power_pac) / 2147483648.0f;
    } else {
        cur.active_power_a = ((float)raw_pa * (float)u16_power_pac) / 2147483648.0f;
    }
    
    /* 6. 通道 B 有功功率 Active Power Channel B (W) */
    raw_pb = hlw8112_read_reg_bytes(REG_POWER_PB_ADDR, 4);
    if (raw_pb > 0x80000000) {
        uint32_t abs_pb = ~raw_pb;
        cur.active_power_b = ((float)abs_pb * (float)u16_power_pbc) / 2147483648.0f;
    } else {
        cur.active_power_b = ((float)raw_pb * (float)u16_power_pbc) / 2147483648.0f;
    }
    
    /* 7. 通道 A 有功电能 Active Energy Channel A (kWh) */
    raw_ea = hlw8112_read_reg_bytes(REG_ENERGY_PA_ADDR, 3);
    cur.active_energy_a = ((float)raw_ea * (float)u16_energy_ac) / 536870912.0f;
    
    /* 8. 通道 B 有功电能 Active Energy Channel B (kWh) */
    raw_eb = hlw8112_read_reg_bytes(REG_ENERGY_PB_ADDR, 3);
    cur.active_energy_b = ((float)raw_eb * (float)u16_energy_bc) / 536870912.0f;
    
    /* 9. 功率因数 Power Factor (0.000 ~ 1.000) */
    raw_pf = hlw8112_read_reg_bytes(REG_PF_ADDR, 3);
    if (raw_pf > 0x800000) {
        pf_val = (float)(0xFFFFFF - raw_pf + 1) / 8388607.0f;
    } else {
        pf_val = (float)raw_pf / 8388607.0f;
    }
    if (cur.active_power_a < 0.3f) {
        pf_val = 0.0f;
    }
    cur.power_factor = pf_val;
    
    /* 10. 相位角 Phase Angle (Degrees °) */
    raw_angle = hlw8112_read_reg_bytes(REG_ANGLE_ADDR, 2);
    if (cur.frequency < 55.0f) {
        angle_val = (float)raw_angle * 0.0805f;
    } else {
        angle_val = (float)raw_angle * 0.0965f;
    }
    if (cur.active_power_a < 0.5f) {
        angle_val = 0.0f;
    }
    cur.phase_angle = angle_val;

    /* Sanity Guard: 坏帧校验与拦截 (电压 > 500V 或 负异常值拦截) */
    if (cur.voltage > 500.0f || cur.current_a > 150.0f || cur.active_power_a > 75000.0f) {
        if (s_hlw_data_valid) {
            /* 坏帧保持上一周期有效值 */
            memcpy(data, &s_last_valid_hlw_data, sizeof(hlw8112_data_t));
            return;
        }
    }

    /* 更新有效测量值缓存 */
    memcpy(&s_last_valid_hlw_data, &cur, sizeof(hlw8112_data_t));
    s_hlw_data_valid = 1;
    memcpy(data, &cur, sizeof(hlw8112_data_t));
}
