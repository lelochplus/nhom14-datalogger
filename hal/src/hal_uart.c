#include <string.h>
#include "hal_uart.h"
#include "hal_gpio.h"
#include "bsp.h"
#include "task.h"
#include "semphr.h"
#include "stream_buffer.h"

#define RX_BUF_SIZE 128

static SemaphoreHandle_t s_tx_mutex;
static StreamBufferHandle_t s_rx_stream;

void hal_uart_init(uint32_t baud)
{
    s_tx_mutex  = xSemaphoreCreateMutex();
    s_rx_stream = xStreamBufferCreate(RX_BUF_SIZE, 1);
    configASSERT(s_tx_mutex && s_rx_stream);

    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
    hal_gpio_config(BSP_UART_PORT, BSP_UART_TX_PIN, HAL_GPIO_AF_PP);
    hal_gpio_config(BSP_UART_PORT, BSP_UART_RX_PIN, HAL_GPIO_IN_PULLUP);

    USART1->BRR = (SystemCoreClock + baud / 2u) / baud;   /* USART1 trên APB2 = HCLK */
    USART1->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE | USART_CR1_UE;

    NVIC_SetPriority(USART1_IRQn, BSP_IRQ_PRIO);
    NVIC_EnableIRQ(USART1_IRQn);
}

hal_status_t hal_uart_write(const void *data, size_t len, TickType_t timeout)
{
    const uint8_t *p = data;

    if (xSemaphoreTake(s_tx_mutex, timeout) != pdTRUE)
        return HAL_BUSY;

    /* ponytail: TX polling (~87 us/byte @115200) chặn CPU trong lúc gửi;
     * chuyển sang DMA1 Channel4 nếu dòng log dài làm trễ task khác. */
    TickType_t start = xTaskGetTickCount();
    hal_status_t st = HAL_OK;
    for (size_t i = 0; i < len && st == HAL_OK; i++) {
        while (!(USART1->SR & USART_SR_TXE))
            if (xTaskGetTickCount() - start > timeout) { st = HAL_TIMEOUT; break; }
        if (st == HAL_OK) USART1->DR = p[i];
    }

    xSemaphoreGive(s_tx_mutex);
    return st;
}

hal_status_t hal_uart_puts(const char *s, TickType_t timeout)
{
    return hal_uart_write(s, strlen(s), timeout);
}

size_t hal_uart_read(void *buf, size_t len, TickType_t timeout)
{
    return xStreamBufferReceive(s_rx_stream, buf, len, timeout);
}

void USART1_IRQHandler(void)
{
    BaseType_t woken = pdFALSE;
    uint32_t sr = USART1->SR;

    if (sr & (USART_SR_RXNE | USART_SR_ORE)) {
        uint8_t c = (uint8_t)USART1->DR;   /* đọc DR xoá luôn cờ ORE */
        if (sr & USART_SR_RXNE)
            xStreamBufferSendFromISR(s_rx_stream, &c, 1, &woken);   /* đầy thì bỏ byte */
    }
    portYIELD_FROM_ISR(woken);
}
