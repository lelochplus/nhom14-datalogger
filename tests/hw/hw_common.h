/*
 * hw_common - khung chung cho các chương trình test phần cứng từng module
 * (tests/hw/bN_*.c). Mỗi bài test chỉ cần viết hàm test_main() chạy trong 1 task.
 *   - hw_log(): in ra UART (115200) VÀ cuộn trên OLED nếu OLED có mặt
 *   - LED PC13 đảo trạng thái mỗi lần hw_log -> nháy = chương trình đang chạy
 */
#ifndef HW_COMMON_H
#define HW_COMMON_H

#include <stdbool.h>
#include "FreeRTOS.h"
#include "task.h"

extern bool hw_oled_ok;            /* true nếu OLED khởi tạo được */

void test_main(void);              /* mỗi file bài test định nghĩa hàm này */
void hw_log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void hw_sleep(uint32_t ms);

#endif
