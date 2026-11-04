/* Bước 5: DS3231 (hàng 6 chân: VCC 3.3V, GND, SCL B6, SDA B7).
 * Nếu RTC mất giờ thì tự đặt theo giờ lúc biên dịch firmware.
 * Đạt khi: giây tăng đều 1 s; rút nguồn, cắm lại vẫn đúng giờ (cần pin). */
#include <string.h>
#include <stdlib.h>
#include "hw_common.h"
#include "ds3231.h"

static app_time_t build_time(void)
{
    static const char mon[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
    const char *d = __DATE__, *t = __TIME__;           /* "Nov  4 2026", "13:45:00" */
    app_time_t r;
    r.mon  = (uint8_t)((strstr(mon, (char[]){d[0], d[1], d[2], 0}) - mon) / 3 + 1);
    r.day  = (uint8_t)atoi(d + 4);
    r.year = (uint16_t)atoi(d + 7);
    r.hour = (uint8_t)atoi(t);
    r.min  = (uint8_t)atoi(t + 3);
    r.sec  = (uint8_t)atoi(t + 6);
    return r;
}

void test_main(void)
{
    hw_log("B5 DS3231 init: %s", ds3231_init() == HAL_OK ? "OK" : "LOI");
    app_time_t now;
    if (ds3231_lost_power() || ds3231_get_time(&now) != HAL_OK) {
        app_time_t b = build_time();
        hw_log("RTC mat gio -> dat: %s", ds3231_set_time(&b) == HAL_OK ? "OK" : "LOI");
    }
    for (;;) {
        if (ds3231_get_time(&now) == HAL_OK)
            hw_log("%02u:%02u:%02u %02u/%02u/%04u", now.hour, now.min, now.sec,
                   now.day, now.mon, now.year);
        else
            hw_log("DS3231 LOI (0x68 khong tra loi)");
        hw_sleep(1000);
    }
}
