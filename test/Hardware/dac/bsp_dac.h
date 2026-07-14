#ifndef __BSP_DAC_H
#define __BSP_DAC_H

#include "gd32f4xx.h"

void bsp_dac_init(void);
void bsp_dac_set_voltage(uint8_t channel, float voltage);

#endif /* __BSP_DAC_H */
