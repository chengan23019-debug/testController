/*!
    \file    gd32f4xx_it.c
    \brief   interrupt service routines
    \version 2026-02-05, V3.3.3, firmware for GD32F4xx
*/

#include "gd32f4xx_it.h"
#include "main.h"
#include "systick.h"
#include "bsp_cs1237.h"
#include "bsp_timer.h"

#include "FreeRTOS.h"
#include "task.h"

/*!
    \brief      this function handles NMI exception
    \param[in]  none
    \param[out] none
    \retval     none
*/
void NMI_Handler(void)
{
    while(1) {
    }
}

/*!
    \brief      this function handles HardFault exception
    \param[in]  none
    \param[out] none
    \retval     none
*/
void HardFault_Handler(void)
{
    while(1) {
    }
}

/*!
    \brief      this function handles MemManage exception
    \param[in]  none
    \param[out] none
    \retval     none
*/
void MemManage_Handler(void)
{
    while(1) {
    }
}

/*!
    \brief      this function handles BusFault exception
    \param[in]  none
    \param[out] none
    \retval     none
*/
void BusFault_Handler(void)
{
    while(1) {
    }
}

/*!
    \brief      this function handles UsageFault exception
    \param[in]  none
    \param[out] none
    \retval     none
*/
void UsageFault_Handler(void)
{
    while(1) {
    }
}

/*!
    \brief      this function handles DebugMon exception
    \param[in]  none
    \param[out] none
    \retval     none
*/
void DebugMon_Handler(void)
{
    while(1) {
    }
}

/*!
    \brief    this function handles SysTick exception
    \param[in]  none
    \param[out] none
    \retval     none
*/
void SysTick_Handler(void)
{
    delay_decrement();

    if(xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
    {
        vTaskIncrementTick();
        portYIELD();
    }
}

/*!
    \brief      this function handles EXTI5 to EXTI9 interrupt for CS1237 DOUT (PF7)
    \param[in]  none
    \param[out] none
    \retval     none
*/
void EXTI5_9_IRQHandler(void)
{
    cs1237_exti_isr();
}

/*!
    \brief      this function handles TIMER1 interrupt for Speed Measurement Input Capture
    \param[in]  none
    \param[out] none
    \retval     none
*/
void TIMER1_IRQHandler(void)
{
    bsp_timer_irq_handler();
}
