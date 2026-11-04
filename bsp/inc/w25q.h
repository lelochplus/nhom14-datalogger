/* w25q - SPI NOR Flash Winbond W25Q64 (W25Q32/128 cũng chạy) trên SPI1
 * (Dev 3: feature/task-comm). Chỉ gọi từ task_comm (sau khi scheduler chạy). */
#ifndef W25Q_H
#define W25Q_H

#include "hal_common.h"

#define W25Q_PAGE_SIZE    256u
#define W25Q_SECTOR_SIZE  4096u

/* Đọc JEDEC ID; trả HAL_ERROR nếu không thấy chip Winbond */
hal_status_t w25q_init(void);
uint32_t     w25q_capacity(void);   /* byte, theo JEDEC ID (W25Q64: 8 MB) */
uint32_t     w25q_jedec_id(void);

hal_status_t w25q_read(uint32_t addr, uint8_t *buf, size_t len);
/* Ghi tối đa tới hết page chứa addr (vùng đích phải đã xoá) */
hal_status_t w25q_write_page(uint32_t addr, const uint8_t *buf, size_t len);
hal_status_t w25q_erase_sector(uint32_t addr);   /* 4 KB, ~45 ms (max 400 ms) */
hal_status_t w25q_erase_chip(void);              /* W25Q64 ~20 s (max 100 s) */

#endif
