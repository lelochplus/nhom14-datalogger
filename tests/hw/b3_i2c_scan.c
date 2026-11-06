/* Bước 3: quét bus I2C (B6/B7) mỗi 2 giây, in địa chỉ tìm thấy.
 * Cắm đủ cả 3 module thì phải thấy: 0x3C OLED, 0x44 SHT30, 0x68 DS3231
 * (DS3231 ZS-042 có thêm 0x57 = EEPROM AT24C32 trên module, bình thường). */
#include "hw_common.h"
#include "hal_i2c.h"

static const char *name(uint8_t a)
{
    switch (a) {
    case 0x3C: case 0x3D: return "OLED";
    case 0x44: return "SHT30";
    case 0x45: return "SHT30 (AD=VCC!)";
    case 0x68: return "DS3231";
    case 0x57: return "EEPROM ZS-042";
    default:   return "?";
    }
}

void test_main(void)
{
    for (;;) {
        int found = 0;
        hw_log("--- Quet I2C ---");
        for (uint8_t a = 0x08; a < 0x78; a++) {
            static const uint8_t zero = 0;
            if (hal_i2c_write(a, &zero, 1) == HAL_OK) {   /* thiết bị ACK địa chỉ */
                hw_log("0x%02X %s", a, name(a));
                found++;
            }
        }
        hw_log("Tim thay %d thiet bi", found);
        hw_sleep(2000);
    }
}
