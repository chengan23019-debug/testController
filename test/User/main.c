/*!
    \file    main.c
    \brief   led spark with systick

    \version 2026-02-05, V3.3.3, firmware for GD32F4xx
*/

/*
    Copyright (c) 2026, GigaDevice Semiconductor Inc.

    Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright notice, this
       list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright notice,
       this list of conditions and the following disclaimer in the documentation
       and/or other materials provided with the distribution.
    3. Neither the name of the copyright holder nor the names of its contributors
       may be used to endorse or promote products derived from this software without
       specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
OF SUCH DAMAGE.
*/

#include "gd32f4xx.h"
#include "systick.h"
#include <stdio.h>
#include "main.h"
#include "gd32f4xx_rcu.h"
#include "usart.h"
#include "bsp_dac.h"
#include "bsp_cs1237.h"

#define CS1237_VREF_VALUE    2.5f  /* Internal REFOUT reference voltage is 2.5V */
#define CS1237_ZERO_OFFSET   0.0f  /* Reset offset to 0.0V since short-circuit is near 0 LSB */

/* Kalman Filter Structure (C89 Compliant) */
typedef struct {
    float x;      /* State estimate (Estimated Voltage in Volts) */
    float p;      /* Estimate covariance */
    float q;      /* Process noise covariance */
    float r;      /* Measurement noise covariance */
} kalman_filter_t;

/* Initialize Kalman filter parameters */
static void kalman_init(kalman_filter_t *kf, float init_x, float init_p, float q, float r)
{
    kf->x = init_x;
    kf->p = init_p;
    kf->q = q;
    kf->r = r;
}

/* Update Kalman filter state with new measurement */
static float kalman_update(kalman_filter_t *kf, float measurement)
{
    float k_gain;
    
    /* Prediction update */
    kf->p = kf->p + kf->q;
    
    /* Measurement update */
    k_gain = kf->p / (kf->p + kf->r);
    kf->x = kf->x + k_gain * (measurement - kf->x);
    kf->p = (1.0f - k_gain) * kf->p;
    
    return kf->x;
}

/* Convert PGA enum to float multiplier */
static float get_gain_multiplier(cs1237_pga_t pga)
{
    switch(pga) {
        case CS1237_PGA_1X:   return 1.0f;
        case CS1237_PGA_2X:   return 2.0f;
        case CS1237_PGA_64X:  return 64.0f;
        case CS1237_PGA_128X: return 128.0f;
        default:              return 1.0f;
    }
}

/*!
    \brief    main function
    \param[in]  none
    \param[out] none
    \retval     none
*/
int main(void)
{
    uint8_t cs_config = 0;
    uint8_t success = 0;
    int32_t adc_val = 0;
    int32_t adc_val_abs = 0;
    
    /* Auto-ranging state variables */
    cs1237_pga_t current_pga = CS1237_PGA_1X;
    float gain_multiplier = 1.0f;
    uint8_t pga_changed = 0;
    
    /* Voltages for filtering */
    float measured_voltage = 0.0f;
    float filtered_voltage = 0.0f;
    
    /* Kalman Filter instance */
    kalman_filter_t kf;

    systick_config();
    usart_debug_init(115200);
    
    /* Initialize DAC (PA4 for Channel 0, PA5 for Channel 1) */
    /* bsp_dac_init(); */
    
    /* Set initial test voltages */
    /* bsp_dac_set_voltage(0, 1.65f); */
    /* bsp_dac_set_voltage(1, 2.50f); */

    rcu_periph_clock_enable(RCU_GPIOD);
    gpio_mode_set(GPIOD, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_7);//PD7-LED2 ???H
    gpio_output_options_set(GPIOD, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_7);//PD7-LED2

    /* Initialize Kalman filter: process noise q=0.0001, measurement noise r=0.1 (balanced smoothing and response) */
    kalman_init(&kf, 0.0f, 1.0f, 0.0001f, 0.1f);

    /* Initialize CS1237 ADC (PC1 as CLK, PC2 as DOUT) */
    printf("Initializing CS1237...\r\n");
    cs1237_init();
    delay_1ms(200); /* Wait for chip startup stabilization */
    
    /* Configure CS1237 starting at 1X gain (VREF output enabled, speed 10Hz) */
    current_pga = CS1237_PGA_1X;
    gain_multiplier = 1.0f;
    cs1237_configure(current_pga, CS1237_SPEED_10HZ, CS1237_CH_A, CS1237_VREF_ON);
    delay_1ms(200); /* Wait for new conversion after configuration change to complete */
    
    /* Read back config to check communication */
    cs_config = cs1237_read_reg();
    printf("CS1237 Config readback: 0x%02X\r\n", cs_config);

    while(1) {
        // gpio_bit_set(GPIOD, GPIO_PIN_7);
        // delay_1ms(500);
        // gpio_bit_reset(GPIOD, GPIO_PIN_7);
        // delay_1ms(500);
        
        success = 0;
        adc_val = cs1237_read_adc_signed(&success);
        if (success) {
            /* Compute absolute value for ranging check */
            if (adc_val < 0) {
                adc_val_abs = -adc_val;
            } else {
                adc_val_abs = adc_val;
            }
            
            /* Auto-ranging state machine */
            pga_changed = 0;
            
            /* 1. Down-ranging (reduce gain to prevent ADC saturation)
               Note: CS1237 PGA analog output typically limits/clips around 80%-85% of full range (~7,000,000).
               We set the down-shift threshold to 6,800,000 to guarantee it switches down before analog clipping occurs. */
            if (adc_val_abs > 6800000) {
                if (current_pga == CS1237_PGA_128X) {
                    current_pga = CS1237_PGA_64X;
                    pga_changed = 1;
                } else if (current_pga == CS1237_PGA_64X) {
                    current_pga = CS1237_PGA_2X;
                    pga_changed = 1;
                } else if (current_pga == CS1237_PGA_2X) {
                    current_pga = CS1237_PGA_1X;
                    pga_changed = 1;
                }
            }
            /* 2. Up-ranging (increase gain to improve low-signal resolution)
               Note: To prevent oscillation, the up-shift threshold T_up must satisfy:
               T_up * (Gain_new / Gain_current) < T_down (6,800,000).
               For 1X->2X and 64X->128X (ratio of 2), T_up must be < 3,400,000. We set it to 3,000,000.
               For 2X->64X (ratio of 32), T_up must be < 212,500. We set it to 100,000. */
            else {
                if (current_pga == CS1237_PGA_1X && adc_val_abs < 3000000) {
                    current_pga = CS1237_PGA_2X;
                    pga_changed = 1;
                } else if (current_pga == CS1237_PGA_2X && adc_val_abs < 100000) {
                    current_pga = CS1237_PGA_64X;
                    pga_changed = 1;
                } else if (current_pga == CS1237_PGA_64X && adc_val_abs < 3000000) {
                    current_pga = CS1237_PGA_128X;
                    pga_changed = 1;
                }
            }
            
            /* Apply gear change if needed */
            if (pga_changed) {
                gain_multiplier = get_gain_multiplier(current_pga);
                printf("[AUTO-RANGE] Gear Switch -> New Gain: %d x\r\n", (int)gain_multiplier);
                cs1237_configure(current_pga, CS1237_SPEED_10HZ, CS1237_CH_A, CS1237_VREF_ON);
                delay_1ms(200); /* Wait for new conversion to settle */
                continue; /* Skip this print since it was transient */
            }
            
            /* Calculate measured voltage: V = (ADC / 8388607) * (VREF / Gain)
               Note: CS1237 actual differential full-scale input range is +/- VREF / Gain. */
            measured_voltage = (((float)adc_val / 8388607.0f) * (CS1237_VREF_VALUE / gain_multiplier)) - CS1237_ZERO_OFFSET;
            
            /* Fast step-response check: If the input changes by more than 5mV,
               instantly reset the Kalman filter state to the new measurement to prevent lag. */
            if (measured_voltage - filtered_voltage > 0.005f || 
                measured_voltage - filtered_voltage < -0.005f) {
                kf.x = measured_voltage;
            }
            
            /* Update Kalman filter state */
            filtered_voltage = kalman_update(&kf, measured_voltage);
            
            /* Print raw values and filtered results */
            printf("Gain: %3dx | Raw: %8d | Meas: %9.6f V | Filtered: %9.6f V\r\n", 
                   (int)gain_multiplier, adc_val, measured_voltage, filtered_voltage);
        } else {
            printf("CS1237 Read Timeout!\r\n");
        }
    }
}
