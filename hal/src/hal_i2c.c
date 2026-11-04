/*
 * I2C1 master polling cho STM32F1, theo trình tự AN2824 (đọc 1, 2, >2 byte)
 * và errata ES096 (đoạn xoá ADDR/đặt STOP phải không bị ngắt chen vào).
 */
#include "hal_i2c.h"
#include "hal_gpio.h"
#include "bsp.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#define I2C_ERR_FLAGS (I2C_SR1_AF | I2C_SR1_BERR | I2C_SR1_ARLO)

static SemaphoreHandle_t s_mutex;

static void short_delay(void) { for (volatile int i = 0; i < 60; i++) {} }  /* ~5 us */

/* Slave bị reset giữa chừng có thể giữ SDA = 0: phát 9 xung SCL rồi STOP */
static void bus_recover(void)
{
    I2C1->CR1 &= ~I2C_CR1_PE;
    hal_gpio_write(BSP_I2C_PORT, BSP_I2C_SCL_PIN, 1);
    hal_gpio_write(BSP_I2C_PORT, BSP_I2C_SDA_PIN, 1);
    hal_gpio_config(BSP_I2C_PORT, BSP_I2C_SCL_PIN, HAL_GPIO_OUT_OD);
    hal_gpio_config(BSP_I2C_PORT, BSP_I2C_SDA_PIN, HAL_GPIO_OUT_OD);
    short_delay();

    for (int i = 0; i < 9 && !hal_gpio_read(BSP_I2C_PORT, BSP_I2C_SDA_PIN); i++) {
        hal_gpio_write(BSP_I2C_PORT, BSP_I2C_SCL_PIN, 0); short_delay();
        hal_gpio_write(BSP_I2C_PORT, BSP_I2C_SCL_PIN, 1); short_delay();
    }
    hal_gpio_write(BSP_I2C_PORT, BSP_I2C_SCL_PIN, 0); short_delay();
    hal_gpio_write(BSP_I2C_PORT, BSP_I2C_SDA_PIN, 0); short_delay();
    hal_gpio_write(BSP_I2C_PORT, BSP_I2C_SCL_PIN, 1); short_delay();
    hal_gpio_write(BSP_I2C_PORT, BSP_I2C_SDA_PIN, 1); short_delay();

    hal_gpio_config(BSP_I2C_PORT, BSP_I2C_SCL_PIN, HAL_GPIO_AF_OD);
    hal_gpio_config(BSP_I2C_PORT, BSP_I2C_SDA_PIN, HAL_GPIO_AF_OD);
}

static void periph_setup(void)
{
    uint32_t pclk1 = SystemCoreClock / 2u;      /* APB1 = HCLK/2 (bsp.c) */
    uint32_t mhz = pclk1 / 1000000u;

    I2C1->CR1 = I2C_CR1_SWRST;
    I2C1->CR1 = 0;
    I2C1->CR2 = mhz;
    /* Fast mode, duty 2:1 -> f = pclk1 / (3 * CCR) */
    I2C1->CCR = I2C_CCR_FS | (pclk1 / (3u * BSP_I2C_SPEED_HZ));
    I2C1->TRISE = mhz * 300u / 1000u + 1u;      /* tr(max) 300 ns */
    I2C1->CR1 = I2C_CR1_PE;
}

void hal_i2c_init(void)
{
    s_mutex = xSemaphoreCreateMutex();
    configASSERT(s_mutex);
    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;
    bus_recover();
    periph_setup();
}

static hal_status_t wait_set(volatile uint32_t *reg, uint32_t mask)
{
    TickType_t t0 = xTaskGetTickCount();
    while (!(*reg & mask)) {
        if (I2C1->SR1 & I2C_ERR_FLAGS) return HAL_ERROR;
        if (xTaskGetTickCount() - t0 > pdMS_TO_TICKS(HAL_I2C_TIMEOUT_MS)) return HAL_TIMEOUT;
    }
    return HAL_OK;
}

static hal_status_t wait_clear(volatile uint32_t *reg, uint32_t mask)
{
    TickType_t t0 = xTaskGetTickCount();
    while (*reg & mask)
        if (xTaskGetTickCount() - t0 > pdMS_TO_TICKS(HAL_I2C_TIMEOUT_MS)) return HAL_TIMEOUT;
    return HAL_OK;
}

#define TRY(x) do { hal_status_t _s = (x); if (_s != HAL_OK) return _s; } while (0)

static void clear_addr(void) { (void)I2C1->SR1; (void)I2C1->SR2; }

static hal_status_t start_addr(uint8_t addr_byte)
{
    I2C1->CR1 |= I2C_CR1_START;
    TRY(wait_set(&I2C1->SR1, I2C_SR1_SB));
    I2C1->DR = addr_byte;
    return wait_set(&I2C1->SR1, I2C_SR1_ADDR);   /* AF -> slave không trả lời */
}

static hal_status_t send(const uint8_t *p, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        TRY(wait_set(&I2C1->SR1, I2C_SR1_TXE));
        I2C1->DR = p[i];
    }
    return HAL_OK;
}

static hal_status_t receive(uint8_t *rx, size_t n)
{
    if (n == 1) {
        I2C1->CR1 &= ~I2C_CR1_ACK;
        taskENTER_CRITICAL();
        clear_addr();
        I2C1->CR1 |= I2C_CR1_STOP;
        taskEXIT_CRITICAL();
        TRY(wait_set(&I2C1->SR1, I2C_SR1_RXNE));
        rx[0] = (uint8_t)I2C1->DR;
        return HAL_OK;
    }
    if (n == 2) {
        I2C1->CR1 |= I2C_CR1_POS;
        taskENTER_CRITICAL();
        clear_addr();
        I2C1->CR1 &= ~I2C_CR1_ACK;
        taskEXIT_CRITICAL();
        TRY(wait_set(&I2C1->SR1, I2C_SR1_BTF));
        taskENTER_CRITICAL();
        I2C1->CR1 |= I2C_CR1_STOP;
        rx[0] = (uint8_t)I2C1->DR;
        taskEXIT_CRITICAL();
        rx[1] = (uint8_t)I2C1->DR;
        I2C1->CR1 &= ~I2C_CR1_POS;
        return HAL_OK;
    }

    size_t i = 0;
    clear_addr();
    while (n - i > 3) {
        TRY(wait_set(&I2C1->SR1, I2C_SR1_RXNE));
        rx[i++] = (uint8_t)I2C1->DR;
    }
    /* Còn 3 byte: N-2 trong DR, N-1 trong shift register */
    TRY(wait_set(&I2C1->SR1, I2C_SR1_BTF));
    I2C1->CR1 &= ~I2C_CR1_ACK;
    taskENTER_CRITICAL();
    rx[i++] = (uint8_t)I2C1->DR;
    I2C1->CR1 |= I2C_CR1_STOP;
    taskEXIT_CRITICAL();
    rx[i++] = (uint8_t)I2C1->DR;
    TRY(wait_set(&I2C1->SR1, I2C_SR1_RXNE));
    rx[i] = (uint8_t)I2C1->DR;
    return HAL_OK;
}

static hal_status_t do_transfer(uint8_t addr7, const uint8_t *tx1, size_t n1,
                                const uint8_t *tx2, size_t n2, uint8_t *rx, size_t nrx)
{
    I2C1->SR1 = 0;                                  /* xoá cờ lỗi cũ */
    TRY(wait_clear(&I2C1->SR2, I2C_SR2_BUSY));

    if (n1 + n2 > 0) {
        TRY(start_addr((uint8_t)(addr7 << 1)));
        clear_addr();
        TRY(send(tx1, n1));
        TRY(send(tx2, n2));
        TRY(wait_set(&I2C1->SR1, I2C_SR1_BTF));
    }
    if (nrx == 0) {
        I2C1->CR1 |= I2C_CR1_STOP;
    } else {
        I2C1->CR1 |= I2C_CR1_ACK;
        TRY(start_addr((uint8_t)(addr7 << 1) | 1u));   /* repeated START */
        TRY(receive(rx, nrx));
    }
    return wait_clear(&I2C1->CR1, I2C_CR1_STOP);
}

hal_status_t hal_i2c_transfer(uint8_t addr7, const uint8_t *tx1, size_t tx1_len,
                              const uint8_t *tx2, size_t tx2_len, uint8_t *rx, size_t rx_len)
{
    if (rx_len > 0 && rx == NULL) return HAL_ERROR;
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(100)) != pdTRUE) return HAL_BUSY;

    hal_status_t st = do_transfer(addr7, tx1, tx1_len, tx2, tx2_len, rx, rx_len);
    if (st != HAL_OK) {
        I2C1->CR1 |= I2C_CR1_STOP;
        I2C1->SR1 = 0;
        if (st == HAL_TIMEOUT || wait_clear(&I2C1->SR2, I2C_SR2_BUSY) != HAL_OK) {
            bus_recover();                          /* bus kẹt: khởi động lại I2C */
            periph_setup();
        }
    }

    xSemaphoreGive(s_mutex);
    return st;
}

hal_status_t hal_i2c_write(uint8_t addr7, const uint8_t *data, size_t len)
{
    return hal_i2c_transfer(addr7, data, len, NULL, 0, NULL, 0);
}

hal_status_t hal_i2c_read(uint8_t addr7, uint8_t *data, size_t len)
{
    return hal_i2c_transfer(addr7, NULL, 0, NULL, 0, data, len);
}

hal_status_t hal_i2c_mem_write(uint8_t addr7, uint8_t reg, const uint8_t *data, size_t len)
{
    return hal_i2c_transfer(addr7, &reg, 1, data, len, NULL, 0);
}

hal_status_t hal_i2c_mem_read(uint8_t addr7, uint8_t reg, uint8_t *data, size_t len)
{
    return hal_i2c_transfer(addr7, &reg, 1, NULL, 0, data, len);
}
