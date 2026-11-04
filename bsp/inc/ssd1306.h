/* ssd1306 - OLED 128x64 trên I2C1, vẽ vào framebuffer RAM rồi flush
 * (Dev 2: feature/task-display) */
#ifndef SSD1306_DRV_H
#define SSD1306_DRV_H

#include "hal_common.h"

#define SSD1306_W 128
#define SSD1306_H 64

hal_status_t ssd1306_init(void);
void ssd1306_clear(void);
/* Vẽ chuỗi ASCII tại toạ độ pixel (x, y); scale 1 = 6x8 px/ký tự, 2 = 12x16 */
void ssd1306_text(int x, int y, const char *s, int scale);
void ssd1306_hline(int y);
void ssd1306_pixel(int x, int y);
hal_status_t ssd1306_flush(void);   /* gửi 1024 byte ~25 ms @400 kHz */

#endif
