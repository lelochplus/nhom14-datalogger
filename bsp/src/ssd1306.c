#include <string.h>
#include "ssd1306.h"
#include "hal_i2c.h"
#include "bsp.h"

extern const uint8_t font5x7[95][5];

static uint8_t s_fb[SSD1306_W * SSD1306_H / 8];   /* 1 byte = 8 pixel dọc */

static hal_status_t send_cmds(const uint8_t *cmds, size_t n)
{
    return hal_i2c_mem_write(BSP_SSD1306_ADDR, 0x00, cmds, n);   /* control byte 0x00: lệnh */
}

hal_status_t ssd1306_init(void)
{
    static const uint8_t init[] = {
        0xAE,             /* tắt màn hình */
        0xD5, 0x80,       /* clock */
        0xA8, 0x3F,       /* multiplex 64 */
        0xD3, 0x00,       /* offset */
        0x40,             /* start line 0 */
        0x8D, 0x14,       /* bật charge pump (module 3.3 V) */
        0x20, 0x00,       /* horizontal addressing */
        0xA1, 0xC8,       /* lật ngang + dọc: chân header ở trên */
        0xDA, 0x12,       /* COM pins */
        0x81, 0xCF,       /* contrast */
        0xD9, 0xF1,       /* pre-charge */
        0xDB, 0x40,       /* VCOMH */
        0xA4, 0xA6,       /* hiển thị RAM, không đảo màu */
        0xAF,             /* bật màn hình */
    };
    ssd1306_clear();
    hal_status_t st = send_cmds(init, sizeof init);
    return st == HAL_OK ? ssd1306_flush() : st;
}

void ssd1306_clear(void)
{
    memset(s_fb, 0, sizeof s_fb);
}

void ssd1306_pixel(int x, int y)
{
    if (x >= 0 && x < SSD1306_W && y >= 0 && y < SSD1306_H)
        s_fb[(y / 8) * SSD1306_W + x] |= (uint8_t)(1u << (y & 7));
}

void ssd1306_text(int x, int y, const char *s, int scale)
{
    for (; *s; s++, x += 6 * scale) {
        uint8_t c = (uint8_t)*s;
        if (c < 32 || c > 126) c = '?';
        for (int col = 0; col < 5; col++) {
            uint8_t bits = font5x7[c - 32][col];
            for (int row = 0; row < 8; row++)
                if (bits & (1u << row))
                    for (int dx = 0; dx < scale; dx++)
                        for (int dy = 0; dy < scale; dy++)
                            ssd1306_pixel(x + col * scale + dx, y + row * scale + dy);
        }
    }
}

void ssd1306_hline(int y)
{
    for (int x = 0; x < SSD1306_W; x++) ssd1306_pixel(x, y);
}

hal_status_t ssd1306_flush(void)
{
    static const uint8_t window[] = {0x21, 0, SSD1306_W - 1, 0x22, 0, SSD1306_H / 8 - 1};
    static const uint8_t data_ctrl = 0x40;     /* control byte 0x40: dữ liệu */
    hal_status_t st = send_cmds(window, sizeof window);
    if (st != HAL_OK) return st;
    return hal_i2c_transfer(BSP_SSD1306_ADDR, &data_ctrl, 1, s_fb, sizeof s_fb, NULL, 0);
}
