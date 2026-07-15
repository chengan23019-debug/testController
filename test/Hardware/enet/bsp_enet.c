#include "bsp_enet.h"
#include "gd32f4xx_enet.h"
#include "gd32f4xx_syscfg.h"
#include "systick.h"
#include <stdio.h>

/**
 * @brief Initialize the RMII interface, GPIO pins, reset the PHY chip, and initialize the MAC.
 */
void bsp_enet_init(void)
{
    ErrStatus status = ERROR;
    uint32_t ahbclk = 0;
    uint32_t reg_val = 0;
    int scan_idx = 0;
    uint8_t mac_addr[6];
    uint32_t device_id[3];

    /* 1. Enable RCU clocks for GPIO ports and SYSCFG */
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_GPIOB);
    rcu_periph_clock_enable(RCU_GPIOC);
    rcu_periph_clock_enable(RCU_GPIOD);
    rcu_periph_clock_enable(RCU_SYSCFG);

    /* 2. Enable RCU clocks for Ethernet MAC peripheral */
    rcu_periph_clock_enable(RCU_ENET);
    rcu_periph_clock_enable(RCU_ENETTX);
    rcu_periph_clock_enable(RCU_ENETRX);

    /* 3. Configure the media interface mode to RMII */
    syscfg_enet_phy_interface_config(SYSCFG_ENET_PHY_RMII);

    /* 4. Configure GPIO pins alternate function AF11 (Ethernet) */
    /* PA1: RMII_REF_CLK, PA2: RMII_MDIO, PA7: RMII_CRS_DV */
    gpio_af_set(GPIOA, GPIO_AF_11, GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_7);
    gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_7);
    gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_MAX, GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_7);

    /* PB11: RMII_TX_EN, PB12: RMII_TXD0, PB13: RMII_TXD1 */
    gpio_af_set(GPIOB, GPIO_AF_11, GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13);
    gpio_mode_set(GPIOB, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13);
    gpio_output_options_set(GPIOB, GPIO_OTYPE_PP, GPIO_OSPEED_MAX, GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13);

    /* PC1: RMII_MDC, PC4: RMII_RXD0, PC5: RMII_RXD1 */
    gpio_af_set(GPIOC, GPIO_AF_11, GPIO_PIN_1 | GPIO_PIN_4 | GPIO_PIN_5);
    gpio_mode_set(GPIOC, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_1 | GPIO_PIN_4 | GPIO_PIN_5);
    gpio_output_options_set(GPIOC, GPIO_OTYPE_PP, GPIO_OSPEED_MAX, GPIO_PIN_1 | GPIO_PIN_4 | GPIO_PIN_5);

    /* 5. Configure hardware PHY reset pin (PD1) */
    gpio_mode_set(ENET_RST_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, ENET_RST_PIN);
    gpio_output_options_set(ENET_RST_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, ENET_RST_PIN);

    /* Perform hardware reset on YT8512C PHY */
    gpio_bit_reset(ENET_RST_PORT, ENET_RST_PIN);
    delay_1ms(50); /* Hold reset low for 50ms */
    gpio_bit_set(ENET_RST_PORT, ENET_RST_PIN);
    delay_1ms(100); /* Wait 100ms for PHY stabilization */

    /* Configure MDC clock divider for communication during scanning */
    ahbclk = rcu_clock_freq_get(CK_AHB);
    reg_val = ENET_MAC_PHY_CTL;
    reg_val &= ~ENET_MAC_PHY_CTL_CLR;
    if(ENET_RANGE(ahbclk, 20000000U, 35000000U)) {
        reg_val |= ENET_MDC_HCLK_DIV16;
    } else if(ENET_RANGE(ahbclk, 35000000U, 60000000U)) {
        reg_val |= ENET_MDC_HCLK_DIV26;
    } else if(ENET_RANGE(ahbclk, 60000000U, 100000000U)) {
        reg_val |= ENET_MDC_HCLK_DIV42;
    } else if(ENET_RANGE(ahbclk, 100000000U, 150000000U)) {
        reg_val |= ENET_MDC_HCLK_DIV62;
    } else if((ENET_RANGE(ahbclk, 150000000U, 240000000U)) || (240000000U == ahbclk)) {
        reg_val |= ENET_MDC_HCLK_DIV102;
    }
    ENET_MAC_PHY_CTL = reg_val;

    /* Scan PHY addresses from 0 to 31 to verify MDIO/MDC communication and find PHY address */
    printf("Scanning PHY address (0-31)...\r\n");
    for(scan_idx = 0; scan_idx < 32; scan_idx++) {
        uint16_t val = 0;
        ErrStatus res = enet_phy_write_read(ENET_PHY_READ, (uint16_t)scan_idx, PHY_REG_BSR, &val);
        if(res == SUCCESS && val != 0xFFFF && val != 0x0000) {
            printf(" -> Found PHY at address: %d, BSR: 0x%04X\r\n", scan_idx, val);
        }
    }

    /* 6. Initialize ENET MAC peripheral */
    printf("Initializing Ethernet MAC (YT8512C)...\r\n");
    enet_deinit();

    printf("Testing enet_phy_config()...\r\n");
    if(enet_phy_config() == SUCCESS) {
        printf(" -> enet_phy_config() SUCCESS!\r\n");
    } else {
        printf(" -> enet_phy_config() FAILED!\r\n");
    }
    
    status = enet_init(ENET_AUTO_NEGOTIATION, ENET_NO_AUTOCHECKSUM, ENET_BROADCAST_FRAMES_PASS);
    if(status == SUCCESS) {
        printf("Ethernet MAC initialization SUCCESS!\r\n");

        /* Generate unique MAC address from MCU Unique Device ID (UDID) */
        device_id[0] = *(volatile uint32_t *)(0x1FFF7A10);
        device_id[1] = *(volatile uint32_t *)(0x1FFF7A10 + 4);
        device_id[2] = *(volatile uint32_t *)(0x1FFF7A10 + 8);

        mac_addr[0] = 0x02; /* Locally administered unicast address */
        mac_addr[1] = 0x1A;
        mac_addr[2] = (uint8_t)((device_id[0] >> 24) & 0xFF);
        mac_addr[3] = (uint8_t)((device_id[0] >> 16) & 0xFF);
        mac_addr[4] = (uint8_t)((device_id[0] >> 8) & 0xFF);
        mac_addr[5] = (uint8_t)(device_id[0] & 0xFF);

        /* Configure MAC Address 0 */
        enet_mac_address_set(ENET_MAC_ADDRESS0, mac_addr);

        /* Print MAC and IP status */
        printf(" -> MAC Address: %02X:%02X:%02X:%02X:%02X:%02X\r\n", 
               mac_addr[0], mac_addr[1], mac_addr[2], 
               mac_addr[3], mac_addr[4], mac_addr[5]);

        /* Enable ENET global interrupts in NVIC */
        nvic_irq_enable(ENET_IRQn, 10, 0);

        /* Enable ENET DMA Receive and Normal Interrupts */
        enet_interrupt_enable(ENET_DMA_INT_NIE);
        enet_interrupt_enable(ENET_DMA_INT_RIE);
    } else {
        printf("Ethernet MAC initialization FAILED! PHY communication error.\r\n");
    }
}

/**
 * @brief Read the PHY BSR register for diagnostics.
 */
uint16_t bsp_enet_phy_bsr_read(void)
{
    uint16_t val = 0;
    enet_phy_write_read(ENET_PHY_READ, PHY_ADDRESS, PHY_REG_BSR, &val);
    return val;
}

/**
 * @brief Read the PHY BCR register for diagnostics.
 */
uint16_t bsp_enet_phy_bcr_read(void)
{
    uint16_t val = 0;
    enet_phy_write_read(ENET_PHY_READ, PHY_ADDRESS, PHY_REG_BCR, &val);
    return val;
}

/**
 * @brief Read any PHY register for diagnostics.
 */
uint16_t bsp_enet_phy_reg_read(uint16_t reg_addr)
{
    uint16_t val = 0;
    enet_phy_write_read(ENET_PHY_READ, PHY_ADDRESS, reg_addr, &val);
    return val;
}
