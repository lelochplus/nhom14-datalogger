#include "w25q.h"
#include "hal_spi.h"
#include "FreeRTOS.h"
#include "task.h"

#define CMD_WRITE_ENABLE   0x06
#define CMD_READ_STATUS1   0x05
#define CMD_READ_DATA      0x03
#define CMD_PAGE_PROGRAM   0x02
#define CMD_SECTOR_ERASE   0x20
#define CMD_CHIP_ERASE     0xC7
#define CMD_RELEASE_PD     0xAB
#define CMD_JEDEC_ID       0x9F
#define STATUS_BUSY        0x01

static uint32_t s_jedec;

static void cmd_addr(uint8_t cmd, uint32_t addr)
{
    uint8_t f[4] = { cmd, (uint8_t)(addr >> 16), (uint8_t)(addr >> 8), (uint8_t)addr };
    hal_spi_xfer_buf(f, NULL, sizeof f);
}

static void cmd_only(uint8_t cmd)
{
    hal_spi_cs(true);
    hal_spi_xfer(cmd);
    hal_spi_cs(false);
}

/* Chờ hết BUSY. Thao tác lâu (xoá) thì nhường CPU cho task khác giữa các lần hỏi. */
static hal_status_t wait_ready(uint32_t timeout_ms)
{
    TickType_t t0 = xTaskGetTickCount();
    for (;;) {
        hal_spi_cs(true);
        hal_spi_xfer(CMD_READ_STATUS1);
        uint8_t st = hal_spi_xfer(0xFF);
        hal_spi_cs(false);
        if (!(st & STATUS_BUSY)) return HAL_OK;
        if (xTaskGetTickCount() - t0 > pdMS_TO_TICKS(timeout_ms)) return HAL_TIMEOUT;
        if (timeout_ms > 10) vTaskDelay(1);
    }
}

hal_status_t w25q_init(void)
{
    hal_spi_set_fast(true);           /* W25Q chịu tới 50 MHz với lệnh 0x03 */
    cmd_only(CMD_RELEASE_PD);         /* phòng khi chip đang power-down */
    vTaskDelay(pdMS_TO_TICKS(1));     /* tRES1 3 us */

    uint8_t id[3];
    hal_spi_cs(true);
    hal_spi_xfer(CMD_JEDEC_ID);
    hal_spi_xfer_buf(NULL, id, sizeof id);
    hal_spi_cs(false);
    s_jedec = ((uint32_t)id[0] << 16) | ((uint32_t)id[1] << 8) | id[2];

    /* EF = Winbond; id[2] = log2(dung lượng): 0x16 4 MB, 0x17 8 MB, 0x18 16 MB */
    return (id[0] == 0xEF && id[2] >= 0x14 && id[2] <= 0x18) ? HAL_OK : HAL_ERROR;
}

uint32_t w25q_capacity(void) { return 1UL << (s_jedec & 0xFF); }
uint32_t w25q_jedec_id(void) { return s_jedec; }

hal_status_t w25q_read(uint32_t addr, uint8_t *buf, size_t len)
{
    hal_spi_cs(true);
    cmd_addr(CMD_READ_DATA, addr);
    hal_spi_xfer_buf(NULL, buf, len);
    hal_spi_cs(false);
    return HAL_OK;
}

hal_status_t w25q_write_page(uint32_t addr, const uint8_t *buf, size_t len)
{
    size_t room = W25Q_PAGE_SIZE - (addr % W25Q_PAGE_SIZE);
    if (len == 0 || len > room) return HAL_ERROR;    /* vắt qua page sẽ ghi đè đầu page */

    cmd_only(CMD_WRITE_ENABLE);
    hal_spi_cs(true);
    cmd_addr(CMD_PAGE_PROGRAM, addr);
    hal_spi_xfer_buf(buf, NULL, len);
    hal_spi_cs(false);
    return wait_ready(5);                            /* tPP max 3 ms */
}

hal_status_t w25q_erase_sector(uint32_t addr)
{
    cmd_only(CMD_WRITE_ENABLE);
    hal_spi_cs(true);
    cmd_addr(CMD_SECTOR_ERASE, addr);
    hal_spi_cs(false);
    return wait_ready(500);
}

hal_status_t w25q_erase_chip(void)
{
    cmd_only(CMD_WRITE_ENABLE);
    cmd_only(CMD_CHIP_ERASE);
    return wait_ready(200000);
}
