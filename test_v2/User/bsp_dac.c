#include "bsp_dac.h"
#include <stdio.h>

/**
 * @brief Initialize DAC Channel 0 (PA4) and Channel 1 (PA5)
 */
void bsp_dac_init(void)
{
    /* 1. Enable RCU clocks for GPIOA and DAC */
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_DAC);

    /* 2. Configure PA4 (DAC0_OUT0) and PA5 (DAC0_OUT1) as Analog Mode */
    gpio_mode_set(GPIOA, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, GPIO_PIN_4 | GPIO_PIN_5);

    /* 3. Reset DAC0 module */
    dac_deinit(DAC0);

    /* 4. Configure DAC0 Channel 0 (PA4) */
    /* Disable trigger for direct/immediate voltage output */
    dac_trigger_disable(DAC0, DAC_OUT0);
    /* Disable wave generator */
    dac_wave_mode_config(DAC0, DAC_OUT0, DAC_WAVE_DISABLE);
    /* Enable output buffer to enhance drive capability (low impedance output) */
    dac_output_buffer_enable(DAC0, DAC_OUT0);
    /* Enable DAC0 Channel 0 */
    dac_enable(DAC0, DAC_OUT0);

    /* 5. Configure DAC0 Channel 1 (PA5) */
    /* Disable trigger for direct/immediate voltage output */
    dac_trigger_disable(DAC0, DAC_OUT1);
    /* Disable wave generator */
    dac_wave_mode_config(DAC0, DAC_OUT1, DAC_WAVE_DISABLE);
    /* Enable output buffer to enhance drive capability */
    dac_output_buffer_enable(DAC0, DAC_OUT1);
    /* Enable DAC0 Channel 1 */
    dac_enable(DAC0, DAC_OUT1);
}

/**
 * @brief Set DAC output voltage
 * @param channel: 0 for PA4 (DAC_OUT0), 1 for PA5 (DAC_OUT1)
 * @param voltage: Target voltage (0.0f to 3.3f)
 */
void bsp_dac_set_voltage(uint8_t channel, float voltage)
{
    /* Vref is typically 3.3V on the Liangshan Pi board */
    const float vref = 3.3f;
    uint16_t dac_value = 0;
    
    if(voltage < 0.0f) {
        voltage = 0.0f;
    } else if(voltage > vref) {
        voltage = vref;
    }
    
    /* Convert voltage to 12-bit digital value (0 to 4095) */
    dac_value = (uint16_t)((voltage / vref) * 4095.0f);
    
    /* Print debug information to USART */
    printf("DAC Channel %d: Set to %d mV, Register Value = %d\r\n", channel, (int)(voltage * 1000.0f), dac_value);
    
    if(channel == 0) {
        dac_data_set(DAC0, DAC_OUT0, DAC_ALIGN_12B_R, dac_value);
    } else if(channel == 1) {
        dac_data_set(DAC0, DAC_OUT1, DAC_ALIGN_12B_R, dac_value);
    }
}
