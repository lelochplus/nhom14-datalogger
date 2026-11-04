/*
 * hal_uart - USART1 (Dev 1: feature/hal-gpio-uart)
 * TX: thread-safe bằng mutex. RX: ngắt -> stream buffer (xStreamBufferSendFromISR).
 */
#ifndef HAL_UART_H
#define HAL_UART_H

#include "hal_common.h"
#include "FreeRTOS.h"

void hal_uart_init(uint32_t baud);

/* Gửi len byte. Trả HAL_BUSY nếu task khác giữ UART quá timeout. */
hal_status_t hal_uart_write(const void *data, size_t len, TickType_t timeout);
hal_status_t hal_uart_puts(const char *s, TickType_t timeout);

/* Đọc tối đa len byte đã nhận, chờ tối đa timeout. Trả số byte đọc được. */
size_t hal_uart_read(void *buf, size_t len, TickType_t timeout);

#endif /* HAL_UART_H */
