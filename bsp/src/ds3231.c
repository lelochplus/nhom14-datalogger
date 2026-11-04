#include "ds3231.h"
#include "hal_i2c.h"
#include "bsp.h"

#define REG_TIME     0x00
#define REG_CONTROL  0x0E
#define REG_STATUS   0x0F
#define STATUS_OSF   0x80

hal_status_t ds3231_init(void)
{
    uint8_t ctrl = 0x1C;            /* EOSC = 0 (chạy cả khi dùng pin), INTCN = 1 */
    return hal_i2c_mem_write(BSP_DS3231_ADDR, REG_CONTROL, &ctrl, 1);
}

hal_status_t ds3231_get_time(app_time_t *t)
{
    uint8_t r[7];
    hal_status_t st = hal_i2c_mem_read(BSP_DS3231_ADDR, REG_TIME, r, sizeof r);
    if (st != HAL_OK) return st;

    t->sec  = app_bcd2bin(r[0] & 0x7F);
    t->min  = app_bcd2bin(r[1] & 0x7F);
    if (r[2] & 0x40) {                          /* chế độ 12 h (do thiết bị khác cài) */
        uint8_t h = app_bcd2bin(r[2] & 0x1F) % 12;
        t->hour = (r[2] & 0x20) ? h + 12 : h;
    } else {
        t->hour = app_bcd2bin(r[2] & 0x3F);
    }
    t->day  = app_bcd2bin(r[4] & 0x3F);
    t->mon  = app_bcd2bin(r[5] & 0x1F);
    t->year = 2000 + app_bcd2bin(r[6]);
    return app_time_valid(t) ? HAL_OK : HAL_ERROR;
}

hal_status_t ds3231_set_time(const app_time_t *t)
{
    if (!app_time_valid(t)) return HAL_ERROR;
    uint8_t r[7] = {
        app_bin2bcd(t->sec), app_bin2bcd(t->min), app_bin2bcd(t->hour), /* bit6 = 0: 24 h */
        1,                                                              /* thứ: không dùng */
        app_bin2bcd(t->day), app_bin2bcd(t->mon), app_bin2bcd((uint8_t)(t->year - 2000)),
    };
    hal_status_t st = hal_i2c_mem_write(BSP_DS3231_ADDR, REG_TIME, r, sizeof r);
    if (st != HAL_OK) return st;

    uint8_t status = 0;                         /* xoá OSF: giờ đã hợp lệ */
    return hal_i2c_mem_write(BSP_DS3231_ADDR, REG_STATUS, &status, 1);
}

bool ds3231_lost_power(void)
{
    uint8_t status = 0;
    return hal_i2c_mem_read(BSP_DS3231_ADDR, REG_STATUS, &status, 1) == HAL_OK &&
           (status & STATUS_OSF);
}
