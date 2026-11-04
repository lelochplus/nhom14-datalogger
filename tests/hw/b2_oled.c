/* Bước 2: thêm OLED (VCC 3.3V, GND, SCL B6, SDA B7).
 * Đạt khi: OLED hiện khung viền, chữ to "OLED OK" và số đếm tăng dần. */
#include <stdio.h>
#include "hw_common.h"
#include "ssd1306.h"

void test_main(void)
{
    if (!hw_oled_ok) {
        for (;;) { hw_log("B2 OLED: KHONG thay OLED o 0x3C -> kiem tra day"); hw_sleep(1000); }
    }
    hw_log("B2 OLED: khoi tao OK");
    for (unsigned n = 0;; n++) {
        char l[16];
        ssd1306_clear();
        for (int x = 0; x < SSD1306_W; x++) { ssd1306_pixel(x, 0); ssd1306_pixel(x, 63); }
        for (int y = 0; y < SSD1306_H; y++) { ssd1306_pixel(0, y); ssd1306_pixel(127, y); }
        ssd1306_text(22, 12, "OLED OK", 2);
        snprintf(l, sizeof l, "dem: %u", n);
        ssd1306_text(34, 40, l, 1);
        if (ssd1306_flush() != HAL_OK) hw_log("Loi gui du lieu OLED");
        hw_sleep(500);
    }
}
