/* Bước 1: chỉ cần Blue Pill + ST-Link + USB-TTL.
 * Đạt khi: LED PC13 nháy 1 lần/giây và terminal hiện "Hello n" mỗi giây;
 * gõ phím nào thì board in lại phím đó. */
#include "hw_common.h"
#include "hal_uart.h"

void test_main(void)
{
    hw_log("B1 UART + LED: OK neu thay dong nay");
    for (unsigned n = 1;; n++) {
        char c[16] = {0};
        size_t k = hal_uart_read(c, sizeof c - 1, pdMS_TO_TICKS(1000));
        if (k) hw_log("Nhan duoc: %s", c);
        else   hw_log("Hello %u - go thu 1 phim", n);
    }
}
