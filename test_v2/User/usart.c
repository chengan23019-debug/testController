#include "usart.h"

/* 
 * If not using MicroLib, we need to disable semihosting.
 * The compiler defines __MICROLIB when MicroLib is used.
 */
#if !defined(__MICROLIB)
#pragma import(__use_no_semihosting)

struct __FILE {
    int handle;
};

FILE __stdout;

void _sys_exit(int x)
{
    (void)x;
}
#endif

/**
 * @brief Initialize USART0 for debugging print (PA9:TX, PA10:RX)
 * @param baudrate: Communication speed (e.g. 115200)
 */
void usart_debug_init(uint32_t baudrate)
{
    /* Enable GPIOA and USART0 clocks */
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_USART0);

    /* Set GPIO multiplexer function to AF7 (USART0) */
    gpio_af_set(GPIOA, GPIO_AF_7, GPIO_PIN_9 | GPIO_PIN_10);
    
    /* Configure TX and RX pins: Multiplexed, Pull-up */
    gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO_PIN_9 | GPIO_PIN_10);
    /* Configure TX and RX pin output options: Push-pull, Max speed 50MHz */
    gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_9 | GPIO_PIN_10);

    /* Deinitialize USART0 to reset settings */
    usart_deinit(USART0);
    
    /* Configure USART0 parameters */
    usart_baudrate_set(USART0, baudrate);
    usart_word_length_set(USART0, USART_WL_8BIT);
    usart_stop_bit_set(USART0, USART_STB_1BIT);
    usart_parity_config(USART0, USART_PM_NONE);
    usart_hardware_flow_coherence_config(USART0, USART_HCM_NONE);
    
    /* Enable USART0 Transmit & Receive */
    usart_transmit_config(USART0, USART_TRANSMIT_ENABLE);
    usart_receive_config(USART0, USART_RECEIVE_ENABLE);
    
    /* Enable USART0 */
    usart_enable(USART0);
}

/**
 * @brief Redirect fputc (used by printf) to USART0
 */
int fputc(int ch, FILE *f)
{
    (void)f;
    usart_data_transmit(USART0, (uint8_t)ch);
    while(RESET == usart_flag_get(USART0, USART_FLAG_TBE));
    return ch;
}
