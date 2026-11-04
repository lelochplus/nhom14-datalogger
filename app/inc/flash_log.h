/*
 * flash_log - log vòng + lưu cấu hình trên Flash W25Q64 (Dev 3).
 *
 * Bố trí chip: sector 0 .. N-1 là log vòng (bản ghi 16 byte), sector cuối lưu cấu hình.
 * Mọi hàm tự khoá mutex nên task_comm (ghi, xuất log) và task_display (xem lịch sử,
 * vẽ biểu đồ) dùng chung được. Chỉ gọi sau khi scheduler chạy, trừ fl_os_init().
 *
 * Địa chỉ trả về là địa chỉ tuyệt đối trên chip: con trỏ đọc không bị lệch khi log
 * quay vòng giữa chừng (bản ghi đã bị xoá đè thì fl_read_at trả false, bỏ qua).
 */
#ifndef FLASH_LOG_H
#define FLASH_LOG_H

#include "app_logic.h"

bool     fl_os_init(void);                 /* tạo mutex, gọi 1 lần trong main() */
bool     fl_mount(void);                   /* nhận chip, tìm chỗ ghi; false nếu không thấy chip */
bool     fl_ready(void);

bool     fl_append(const app_sample_t *s); /* false: lỗi ghi -> đã tự unmount */
uint32_t fl_count(void);                   /* số bản ghi đang lưu */
uint32_t fl_capacity(void);                /* số bản ghi tối đa của vùng log */
uint32_t fl_jedec_id(void);

/* Duyệt log: bản ghi thứ n tính ngược từ mới nhất (n = 1 là mới nhất, n >= count là cũ nhất) */
uint32_t fl_addr_back(uint32_t n);
uint32_t fl_addr_next(uint32_t addr);
uint32_t fl_head(void);                    /* địa chỉ sẽ ghi bản ghi kế tiếp (= điểm dừng) */
bool     fl_read_at(uint32_t addr, app_sample_t *s);
bool     fl_read_newest(uint32_t k, app_sample_t *s);   /* k = 0 là mới nhất */

bool     fl_erase_all(void);               /* xoá chip, mất cả cấu hình; 20..100 s */
bool     fl_cfg_load(app_config_t *c);
bool     fl_cfg_save(const app_config_t *c);

#endif /* FLASH_LOG_H */
