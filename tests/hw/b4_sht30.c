/* Bước 4: SHT30 (VIN 3.3V, GND, SCL B6, SDA B7, AD và AL để trống).
 * Đạt khi: in nhiệt độ ~25-35 C, độ ẩm ~40-90 %; thổi hơi vào cảm biến -> độ ẩm tăng. */
#include "hw_common.h"
#include "sht31.h"

void test_main(void)
{
    hw_log("B4 SHT30 init: %s", sht31_init() == HAL_OK ? "OK" : "LOI");
    for (;;) {
        int16_t t; uint16_t h;
        hal_status_t st = sht31_measure(&t, &h);
        if (st == HAL_OK)
            hw_log("T=%d.%02d C  H=%u.%02u %%", t / 100, (t < 0 ? -t : t) % 100, h / 100, h % 100);
        else
            hw_log("SHT30 LOI %d (0x44 khong tra loi)", st);
        hw_sleep(1000);
    }
}
