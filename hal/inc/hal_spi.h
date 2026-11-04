/*
 * hal_spi - SPI1 master mode 0 + chân CS (Dev 2: feature/hal-i2c-spi)
 * Chỉ task_comm dùng SPI (Flash W25Q64) nên không cần mutex.
 * ponytail: thêm mutex khi có thiết bị SPI thứ hai ở task khác.
 */
#ifndef HAL_SPI_H
#define HAL_SPI_H

#include "hal_common.h"

void    hal_spi_init(void);                 /* mặc định tốc độ chậm */
void    hal_spi_set_fast(bool fast);        /* chậm ~281 kHz / nhanh 18 MHz */
void    hal_spi_cs(bool active);            /* active = kéo CS xuống 0 */
uint8_t hal_spi_xfer(uint8_t tx);
/* Truyền/nhận khối: tx == NULL -> gửi 0xFF, rx == NULL -> bỏ dữ liệu nhận */
void    hal_spi_xfer_buf(const uint8_t *tx, uint8_t *rx, size_t len);

#endif /* HAL_SPI_H */
