#include "hal_spi.h"
#include "hal_gpio.h"
#include "bsp.h"

#define SPI_BR_SLOW (7u << SPI_CR1_BR_Pos)   /* 72 MHz / 256 = 281 kHz */
#define SPI_BR_FAST (1u << SPI_CR1_BR_Pos)   /* 72 MHz / 4   = 18 MHz  */

void hal_spi_init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;
    hal_gpio_config(BSP_SPI_PORT, BSP_SPI_SCK_PIN,  HAL_GPIO_AF_PP);
    hal_gpio_config(BSP_SPI_PORT, BSP_SPI_MOSI_PIN, HAL_GPIO_AF_PP);
    hal_gpio_config(BSP_SPI_PORT, BSP_SPI_MISO_PIN, HAL_GPIO_IN_PULLUP);
    hal_gpio_write(BSP_FLASH_CS_PORT, BSP_FLASH_CS_PIN, 1);
    hal_gpio_config(BSP_FLASH_CS_PORT, BSP_FLASH_CS_PIN, HAL_GPIO_OUT_PP);

    /* Master, mode 0, MSB first, NSS phần mềm */
    SPI1->CR1 = SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI | SPI_BR_SLOW | SPI_CR1_SPE;
}

void hal_spi_set_fast(bool fast)
{
    while (SPI1->SR & SPI_SR_BSY) {}
    SPI1->CR1 &= ~SPI_CR1_SPE;
    SPI1->CR1 = (SPI1->CR1 & ~SPI_CR1_BR) | (fast ? SPI_BR_FAST : SPI_BR_SLOW);
    SPI1->CR1 |= SPI_CR1_SPE;
}

void hal_spi_cs(bool active)
{
    hal_gpio_write(BSP_FLASH_CS_PORT, BSP_FLASH_CS_PIN, !active);
}

uint8_t hal_spi_xfer(uint8_t tx)
{
    /* SPI master luôn tự hoàn tất sau 8 xung clock nên không cần timeout */
    while (!(SPI1->SR & SPI_SR_TXE)) {}
    *(volatile uint8_t *)&SPI1->DR = tx;
    while (!(SPI1->SR & SPI_SR_RXNE)) {}
    return *(volatile uint8_t *)&SPI1->DR;
}

void hal_spi_xfer_buf(const uint8_t *tx, uint8_t *rx, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        uint8_t r = hal_spi_xfer(tx ? tx[i] : 0xFF);
        if (rx) rx[i] = r;
    }
}
