/*!
    \file    bsp_lan8720.c
    \brief   LAN8720A / LAN8702 Ethernet PHY Driver Implementation for GD32F470
*/

#include "bsp_lan8720.h"
#include "systick.h"

/* Saved PHY Hardware Address */
static uint16_t sg_phy_addr = LAN8720_DEFAULT_PHY_ADDR;
static uint8_t sg_mac_address[6] = {0x00, 0x80, 0xE1, 0x00, 0x00, 0x01};

/*!
    \brief      Read LAN8720/LAN8702 PHY register via SMI
    \param[in]  phy_addr: PHY address (0..31)
    \param[in]  reg_addr: Register address (0..31)
    \retval     Register value
*/
uint16_t bsp_lan8720_read_phy_reg(uint16_t phy_addr, uint16_t reg_addr)
{
    uint16_t reg_val = 0U;
    enet_phy_write_read(ENET_PHY_READ, phy_addr, reg_addr, &reg_val);
    return reg_val;
}

/*!
    \brief      Write LAN8720/LAN8702 PHY register via SMI
    \param[in]  phy_addr: PHY address (0..31)
    \param[in]  reg_addr: Register address (0..31)
    \param[in]  value: Value to write
*/
void bsp_lan8720_write_phy_reg(uint16_t phy_addr, uint16_t reg_addr, uint16_t value)
{
    uint16_t val = value;
    enet_phy_write_read(ENET_PHY_WRITE, phy_addr, reg_addr, &val);
}

/*!
    \brief      Configure GD32F470 GPIO pins for Ethernet RMII interface
*/
static void lan8720_gpio_config(void)
{
    /* Enable RCU Clocks for GPIOA, GPIOB, GPIOC, and SYSCFG */
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_GPIOB);
    rcu_periph_clock_enable(RCU_GPIOC);
    rcu_periph_clock_enable(RCU_SYSCFG);

    /* Configure SYSCFG to select RMII interface mode */
    syscfg_enet_phy_interface_config(SYSCFG_ENET_PHY_RMII);

    /*
        RMII Standard Pin Assignment for GD32F470:
        PA1  -> ETH_RMII_REF_CLK (Alternate Function AF11)
        PA2  -> ETH_MDIO         (Alternate Function AF11)
        PA7  -> ETH_RMII_CRS_DV  (Alternate Function AF11)
        PC1  -> ETH_MDC          (Alternate Function AF11)
        PC4  -> ETH_RMII_RXD0    (Alternate Function AF11)
        PC5  -> ETH_RMII_RXD1    (Alternate Function AF11)
        PB11 -> ETH_RMII_TX_EN   (Alternate Function AF11)
        PB12 -> ETH_RMII_TXD0    (Alternate Function AF11)
        PB13 -> ETH_RMII_TXD1    (Alternate Function AF11)
    */

    /* PA1: REF_CLK */
    gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_1);
    gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_MAX, GPIO_PIN_1);
    gpio_af_set(GPIOA, GPIO_AF_11, GPIO_PIN_1);

    /* PA2: MDIO */
    gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_2);
    gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_MAX, GPIO_PIN_2);
    gpio_af_set(GPIOA, GPIO_AF_11, GPIO_PIN_2);

    /* PA7: CRS_DV */
    gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_7);
    gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_MAX, GPIO_PIN_7);
    gpio_af_set(GPIOA, GPIO_AF_11, GPIO_PIN_7);

    /* PC1: MDC */
    gpio_mode_set(GPIOC, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_1);
    gpio_output_options_set(GPIOC, GPIO_OTYPE_PP, GPIO_OSPEED_MAX, GPIO_PIN_1);
    gpio_af_set(GPIOC, GPIO_AF_11, GPIO_PIN_1);

    /* PC4: RXD0 */
    gpio_mode_set(GPIOC, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_4);
    gpio_output_options_set(GPIOC, GPIO_OTYPE_PP, GPIO_OSPEED_MAX, GPIO_PIN_4);
    gpio_af_set(GPIOC, GPIO_AF_11, GPIO_PIN_4);

    /* PC5: RXD1 */
    gpio_mode_set(GPIOC, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_5);
    gpio_output_options_set(GPIOC, GPIO_OTYPE_PP, GPIO_OSPEED_MAX, GPIO_PIN_5);
    gpio_af_set(GPIOC, GPIO_AF_11, GPIO_PIN_5);

    /* PB11: TX_EN */
    gpio_mode_set(GPIOB, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_11);
    gpio_output_options_set(GPIOB, GPIO_OTYPE_PP, GPIO_OSPEED_MAX, GPIO_PIN_11);
    gpio_af_set(GPIOB, GPIO_AF_11, GPIO_PIN_11);

    /* PB12: TXD0 */
    gpio_mode_set(GPIOB, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_12);
    gpio_output_options_set(GPIOB, GPIO_OTYPE_PP, GPIO_OSPEED_MAX, GPIO_PIN_12);
    gpio_af_set(GPIOB, GPIO_AF_11, GPIO_PIN_12);

    /* PB13: TXD1 */
    gpio_mode_set(GPIOB, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_13);
    gpio_output_options_set(GPIOB, GPIO_OTYPE_PP, GPIO_OSPEED_MAX, GPIO_PIN_13);
    gpio_af_set(GPIOB, GPIO_AF_11, GPIO_PIN_13);
}

/*!
    \brief      Scan PHY address (0 to 31) to auto-detect LAN8720/LAN8702
    \retval     Detected PHY address, or LAN8720_DEFAULT_PHY_ADDR if not found
*/
static uint16_t lan8720_scan_phy_address(void)
{
    uint16_t addr;
    uint16_t phy_id1, phy_id2;

    for (addr = 0; addr < 32; addr++) {
        phy_id1 = bsp_lan8720_read_phy_reg(addr, PHY_ID1);
        phy_id2 = bsp_lan8720_read_phy_reg(addr, PHY_ID2);

        /* LAN8720 / LAN8702 OUI ID1 is 0x0007 */
        if (phy_id1 == 0x0007U) {
            printf("[LAN8720 Driver] Found PHY at address 0x%02X (PHY_ID1: 0x%04X, PHY_ID2: 0x%04X)\r\n", 
                   addr, phy_id1, phy_id2);
            return addr;
        }
    }

    printf("[LAN8720 Driver] Warning: Could not detect PHY ID 0x0007. Using default PHY address 0x%02X.\r\n", 
           LAN8720_DEFAULT_PHY_ADDR);
    return LAN8720_DEFAULT_PHY_ADDR;
}

/*!
    \brief      Initialize LAN8720 / LAN8702 Ethernet driver
    \retval     LAN8720_OK on success, LAN8720_ERROR on failure
*/
int bsp_lan8720_init(void)
{
    uint32_t timeout = 0U;
    uint16_t reg_val = 0U;
    enet_mediamode_enum media_mode = ENET_AUTO_NEGOTIATION;

    printf("[LAN8720 Driver] Initializing GD32F470 ENET Peripheral & LAN8720/LAN8702 PHY...\r\n");

    /* Enable ENET module clocks */
    rcu_periph_clock_enable(RCU_ENET);
    rcu_periph_clock_enable(RCU_ENETTX);
    rcu_periph_clock_enable(RCU_ENETRX);

    /* Reset ENET peripheral */
    enet_deinit();

    /* Configure RMII GPIO pins */
    lan8720_gpio_config();

    /* ENET Software Reset */
    if (ERROR == enet_software_reset()) {
        printf("[LAN8720 Driver] Error: ENET software reset failed!\r\n");
        return LAN8720_ERROR;
    }

    /* Configure MDC clock divisor based on HCLK frequency */
    if (ERROR == enet_phy_config()) {
        printf("[LAN8720 Driver] Error: ENET PHY SMI clock config failed!\r\n");
        return LAN8720_ERROR;
    }

    /* Scan SMI bus to locate LAN8720 PHY address */
    sg_phy_addr = lan8720_scan_phy_address();

    /* Issue Soft Reset to LAN8720 PHY */
    bsp_lan8720_write_phy_reg(sg_phy_addr, PHY_BCR, PHY_BCR_RESET);
    delay_1ms(10);

    /* Wait until PHY Reset complete (Bit 15 cleared) */
    timeout = 0U;
    do {
        reg_val = bsp_lan8720_read_phy_reg(sg_phy_addr, PHY_BCR);
        delay_1ms(10);
        timeout++;
    } while ((reg_val & PHY_BCR_RESET) && (timeout < 100U));

    if (timeout >= 100U) {
        printf("[LAN8720 Driver] Error: LAN8720 PHY Soft Reset timeout!\r\n");
        return LAN8720_ERROR;
    }

    /* Enable Auto-Negotiation and restart it */
    reg_val = PHY_BCR_AUTONEG_ENABLE | PHY_BCR_RESTART_AUTONEG;
    bsp_lan8720_write_phy_reg(sg_phy_addr, PHY_BCR, reg_val);

    printf("[LAN8720 Driver] Waiting for PHY Auto-Negotiation...\r\n");
    timeout = 0U;
    do {
        reg_val = bsp_lan8720_read_phy_reg(sg_phy_addr, PHY_BSR);
        delay_1ms(20);
        timeout++;
    } while (!(reg_val & PHY_BSR_AUTONEG_COMPLETE) && (timeout < 150U));

    /* Check PHY Special Control/Status Register (0x1F) for speed & duplex */
    reg_val = bsp_lan8720_read_phy_reg(sg_phy_addr, PHY_SCSR);
    switch (reg_val & PHY_SCSR_SPEED_MASK) {
        case PHY_SCSR_100FD:
            media_mode = ENET_100M_FULLDUPLEX;
            printf("[LAN8720 Driver] Network Speed: 100Mbps, Duplex: Full\r\n");
            break;
        case PHY_SCSR_100HD:
            media_mode = ENET_100M_HALFDUPLEX;
            printf("[LAN8720 Driver] Network Speed: 100Mbps, Duplex: Half\r\n");
            break;
        case PHY_SCSR_10FD:
            media_mode = ENET_10M_FULLDUPLEX;
            printf("[LAN8720 Driver] Network Speed: 10Mbps, Duplex: Full\r\n");
            break;
        case PHY_SCSR_10HD:
            media_mode = ENET_10M_HALFDUPLEX;
            printf("[LAN8720 Driver] Network Speed: 10Mbps, Duplex: Half\r\n");
            break;
        default:
            media_mode = ENET_AUTO_NEGOTIATION;
            printf("[LAN8720 Driver] Network Speed/Duplex: Auto-negotiation / Default\r\n");
            break;
    }

    /* Initialize ENET MAC & DMA hardware with Auto-Negotiation enabled */
    if (ERROR == enet_init(ENET_AUTO_NEGOTIATION, ENET_NO_AUTOCHECKSUM, ENET_BROADCAST_FRAMES_PASS)) {
        printf("[LAN8720 Driver] Error: enet_init failed!\r\n");
        return LAN8720_ERROR;
    }

    /* Ensure LAN8720 PHY keeps Auto-Negotiation Enabled (BCR = 0x1000) */
    bsp_lan8720_write_phy_reg(sg_phy_addr, PHY_BCR, PHY_BCR_AUTONEG_ENABLE);

    /* Set MAC Address 0 */
    enet_mac_address_set(ENET_MAC_ADDRESS0, sg_mac_address);
    printf("[LAN8720 Driver] MAC Address set to %02X:%02X:%02X:%02X:%02X:%02X\r\n",
           sg_mac_address[0], sg_mac_address[1], sg_mac_address[2],
           sg_mac_address[3], sg_mac_address[4], sg_mac_address[5]);

    /* Initialize Tx and Rx DMA descriptor chain */
    enet_descriptors_chain_init(ENET_DMA_TX);
    enet_descriptors_chain_init(ENET_DMA_RX);

    /* Enable ENET MAC and DMA transmit & receive */
    enet_enable();

    printf("[LAN8720 Driver] LAN8720/LAN8702 Ethernet Driver Initialized Successfully!\r\n");
    return LAN8720_OK;
}

/*!
    \brief      Get Ethernet Cable Link Status
    \retval     1: Link UP, 0: Link DOWN
*/
uint8_t bsp_lan8720_get_link_status(void)
{
    uint16_t bsr = 0U;

    /* Read BSR twice because link status bit (Bit 2) is latch-low */
    bsr = bsp_lan8720_read_phy_reg(sg_phy_addr, PHY_BSR);
    bsr = bsp_lan8720_read_phy_reg(sg_phy_addr, PHY_BSR);

    if (bsr & PHY_BSR_LINK_STATUS) {
        return 1U;
    } else {
        return 0U;
    }
}

/*!
    \brief      Print all key PHY registers for debugging
*/
void bsp_lan8720_print_phy_regs(void)
{
    uint16_t bcr = bsp_lan8720_read_phy_reg(sg_phy_addr, PHY_BCR);
    uint16_t bsr = bsp_lan8720_read_phy_reg(sg_phy_addr, PHY_BSR);
    uint16_t anar = bsp_lan8720_read_phy_reg(sg_phy_addr, PHY_ANAR);
    uint16_t anlpar = bsp_lan8720_read_phy_reg(sg_phy_addr, PHY_ANLPAR);
    uint16_t scsr = bsp_lan8720_read_phy_reg(sg_phy_addr, PHY_SCSR);

    printf("[PHY Debug] Addr:0x%02X | BCR:0x%04X BSR:0x%04X ANAR:0x%04X ANLPAR:0x%04X SCSR:0x%04X\r\n",
           sg_phy_addr, bcr, bsr, anar, anlpar, scsr);
}

/*!
    \brief      Send an Ethernet frame via GD32 ENET DMA
    \param[in]  p_buf: Pointer to packet payload buffer
    \param[in]  len: Length of packet (bytes)
    \retval     LAN8720_OK on success, LAN8720_ERROR on error
*/
int bsp_lan8720_send_packet(uint8_t *p_buf, uint16_t len)
{
    if ((p_buf == NULL) || (len == 0U)) {
        return LAN8720_ERROR;
    }

    if (SUCCESS == enet_frame_transmit(p_buf, (uint32_t)len)) {
        // printf("[ETH DMA] Transmitted %d bytes\r\n", len);
        return LAN8720_OK;
    } else {
        return LAN8720_ERROR;
    }
}

uint16_t bsp_lan8720_receive_packet(uint8_t *p_buf, uint16_t max_len)
{
    uint32_t frame_len = 0U;

    /* Check if a valid frame has been received */
    frame_len = enet_rxframe_size_get();
    if (frame_len <= 1U) {
        return 0U;
    }

    if (frame_len > max_len) {
        /* Drop frame if buffer is too small */
        enet_rxframe_drop();
        return 0U;
    }

    /* Copy received packet to user buffer */
    if (SUCCESS == enet_frame_receive(p_buf, frame_len)) {
        // printf("[ETH DMA] Received %d bytes\r\n", frame_len);
        return (uint16_t)frame_len;
    }

    return 0U;
}


/*!
    \brief      Get current MAC address
    \param[out] mac: Array of 6 bytes to store MAC address
*/
void bsp_lan8720_get_mac(uint8_t mac[6])
{
    if (mac != NULL) {
        memcpy(mac, sg_mac_address, 6);
    }
}
