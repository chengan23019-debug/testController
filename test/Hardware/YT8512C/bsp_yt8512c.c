#include "bsp_yt8512c.h"
#include "systick.h"
#include <stdio.h>

/**
 * @brief  配置 YT8512C 的 RMII 接口引脚
 */
static void rmii_gpio_config(void)
{
    /* 1. 使能 GPIO 时钟 */
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_GPIOC);
    rcu_periph_clock_enable(RCU_GPIOG); 

    /* 2. 配置 PA1(REF_CLK), PA7(CRS_DV) */
    gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_1 | GPIO_PIN_7);
    gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_MAX, GPIO_PIN_1 | GPIO_PIN_7);
    gpio_af_set(GPIOA, GPIO_AF_11, GPIO_PIN_1 | GPIO_PIN_7);

    /* 配置 PA2(MDIO) - 开启内部上拉 */
    gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO_PIN_2);
    gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_MAX, GPIO_PIN_2);
    gpio_af_set(GPIOA, GPIO_AF_11, GPIO_PIN_2);

    /* 3. 配置 PC1(MDC), PC4(RXD0), PC5(RXD1) */
    gpio_mode_set(GPIOC, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_1 | GPIO_PIN_4 | GPIO_PIN_5);
    gpio_output_options_set(GPIOC, GPIO_OTYPE_PP, GPIO_OSPEED_MAX, GPIO_PIN_1 | GPIO_PIN_4 | GPIO_PIN_5);
    gpio_af_set(GPIOC, GPIO_AF_11, GPIO_PIN_1 | GPIO_PIN_4 | GPIO_PIN_5);

    /* 4. 配置 PG11(TX_EN), PG13(TXD0), PG14(TXD1) - 避开 PB11~PB13 */
    gpio_mode_set(GPIOG, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_11 | GPIO_PIN_13 | GPIO_PIN_14);
    gpio_output_options_set(GPIOG, GPIO_OTYPE_PP, GPIO_OSPEED_MAX, GPIO_PIN_11 | GPIO_PIN_13 | GPIO_PIN_14);
    gpio_af_set(GPIOG, GPIO_AF_11, GPIO_PIN_11 | GPIO_PIN_13 | GPIO_PIN_14);
}

/**
 * @brief  硬件复位 YT8512C 芯片
 */
static void yt8512c_hw_reset(void)
{
    /* 配置复位引脚为推挽输出 */
    rcu_periph_clock_enable(YT8512C_RST_RCU);
    gpio_mode_set(YT8512C_RST_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, YT8512C_RST_PIN);
    gpio_output_options_set(YT8512C_RST_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, YT8512C_RST_PIN);

    /* 拉低复位引脚，保持至少 20ms */
    gpio_bit_reset(YT8512C_RST_PORT, YT8512C_RST_PIN);
    delay_1ms(20);
    
    /* 释放复位引脚，拉高 */
    gpio_bit_set(YT8512C_RST_PORT, YT8512C_RST_PIN);
    
    /* 等待 PHY 芯片初始化完成，提供 100ms 稳定时间 */
    delay_1ms(100); 
}

/**
 * @brief  初始化 YT8512C 及 ENET MAC 外设
 */
void bsp_yt8512c_init(void)
{
    ErrStatus enet_init_status;

    /* 1. 复位硬件 PHY 芯片 */
    yt8512c_hw_reset();

    /* 2. 配置 RMII 相关的 GPIO 引脚 */
    rmii_gpio_config();

    /* 3. 开启 SYSCFG 和以太网外设时钟 */
    rcu_periph_clock_enable(RCU_SYSCFG);
    rcu_periph_clock_enable(RCU_ENET);
    rcu_periph_clock_enable(RCU_ENETTX);
    rcu_periph_clock_enable(RCU_ENETRX);

    /* 4. 选择 RMII 接口模式 (必须在 ENET 软复位之前配置) */
    syscfg_enet_phy_interface_config(SYSCFG_ENET_PHY_RMII);

    /* 5. 软复位 ENET 外设 */
    enet_deinit();
    
    /* 触发 DMA 软件复位，并校验返回值确保 50MHz 参考时钟正常 */
    if (ERROR == enet_software_reset()) {
        printf("[ETH] DMA Software Reset Timeout! Please check RMII REF_CLK on PA1.\r\n");
    } else {
        printf("[ETH] DMA Software Reset Success.\r\n");
    }

    /* 6. 调用官方库初始化函数 */
    /* 开启自协商，不开启硬件校验(由LwIP处理)，接收所有广播包 */
    enet_init_status = enet_init(ENET_AUTO_NEGOTIATION, ENET_NO_AUTOCHECKSUM, ENET_BROADCAST_FRAMES_PASS);
    
    if (SUCCESS == enet_init_status) {
        printf("YT8512C Ethernet PHY Init Success!\r\n");
    } else {
        printf("YT8512C Ethernet PHY Init Failed!\r\n");
    }
}