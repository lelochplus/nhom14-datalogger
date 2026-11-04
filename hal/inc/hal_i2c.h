/*
 * hal_i2c - I2C1 master, 400 kHz (Dev 2: feature/hal-i2c-spi)
 *
 * Bus dùng chung cho SHT31, SSD1306, DS3231 nên MỌI giao dịch đều đi qua
 * mutex bên trong HAL: task gọi không cần tự khoá bus.
 * Chỉ gọi sau khi scheduler đã chạy (timeout tính theo tick).
 */
#ifndef HAL_I2C_H
#define HAL_I2C_H

#include "hal_common.h"

#define HAL_I2C_TIMEOUT_MS 20u

void hal_i2c_init(void);

/* Giao dịch tổng quát: START, ghi tx1 rồi tx2 (có thể NULL/0),
 * nếu rx_len > 0 thì REPEATED START và đọc rx_len byte, cuối cùng STOP. */
hal_status_t hal_i2c_transfer(uint8_t addr7,
                              const uint8_t *tx1, size_t tx1_len,
                              const uint8_t *tx2, size_t tx2_len,
                              uint8_t *rx, size_t rx_len);

/* Tiện ích thường dùng */
hal_status_t hal_i2c_write(uint8_t addr7, const uint8_t *data, size_t len);
hal_status_t hal_i2c_read(uint8_t addr7, uint8_t *data, size_t len);
hal_status_t hal_i2c_mem_write(uint8_t addr7, uint8_t reg, const uint8_t *data, size_t len);
hal_status_t hal_i2c_mem_read(uint8_t addr7, uint8_t reg, uint8_t *data, size_t len);

#endif /* HAL_I2C_H */
