/* Bước 6: W25Q64 (VCC 3.3V, GND, CS A4, CLK A5, DO A6, DI A7, WP/HOLD lên 3.3V).
 * Đọc JEDEC ID, xoá sector cuối, ghi 256 byte mẫu rồi đọc lại so sánh.
 * Đạt khi: ID EF4017 (W25Q64), "GHI/DOC OK". Sector cuối là nơi firmware chính
 * lưu ngưỡng -> sau bài test ngưỡng về mặc định, không sao. */
#include "hw_common.h"
#include "hal_spi.h"
#include "w25q.h"

void test_main(void)
{
    static uint8_t wr[256], rd[256];
    hal_spi_init();
    for (;;) {
        if (w25q_init() != HAL_OK) {
            hw_log("Flash LOI: ID %06lX", (unsigned long)w25q_jedec_id());
            hw_log("000000/FFFFFF = mat day/nguon");
            hw_sleep(2000);
            continue;
        }
        uint32_t cap = w25q_capacity(), addr = cap - W25Q_SECTOR_SIZE;
        hw_log("ID %06lX, %lu KB", (unsigned long)w25q_jedec_id(), (unsigned long)(cap / 1024));
        for (int i = 0; i < 256; i++) wr[i] = (uint8_t)(i * 7 + 3);
        bool ok = w25q_erase_sector(addr) == HAL_OK &&
                  w25q_write_page(addr, wr, sizeof wr) == HAL_OK &&
                  w25q_read(addr, rd, sizeof rd) == HAL_OK;
        int bad = 0;
        for (int i = 0; ok && i < 256; i++) bad += rd[i] != wr[i];
        if (ok && !bad) hw_log("GHI/DOC OK");
        else            hw_log("GHI/DOC LOI (%d byte sai)", bad);
        hw_sleep(5000);
    }
}
