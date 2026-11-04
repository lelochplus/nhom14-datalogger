#include <string.h>
#include "flash_log.h"
#include "w25q.h"
#include "FreeRTOS.h"
#include "semphr.h"

#define SECTOR    W25Q_SECTOR_SIZE
#define REC       APP_REC_SIZE
#define LOCK_WAIT pdMS_TO_TICKS(2000)     /* xoá sector lâu nhất 400 ms */

static SemaphoreHandle_t s_mutex;
static bool     s_ready;
static uint32_t s_log_size;              /* byte vùng log = (số sector - 1) * SECTOR */
static uint32_t s_cfg_addr;              /* sector cuối */
static uint32_t s_head;                  /* chỗ ghi kế tiếp */
static uint32_t s_oldest;                /* bản ghi cũ nhất */

static bool lock(void)    { return xSemaphoreTake(s_mutex, LOCK_WAIT) == pdTRUE; }
static void unlock(void)  { xSemaphoreGive(s_mutex); }

static bool rec_erased(uint32_t addr)
{
    uint8_t b = 0;
    w25q_read(addr, &b, 1);
    return b == 0xFF;
}

/* Xoá sector nếu còn dữ liệu; nếu đó là sector chứa bản ghi cũ nhất thì dời mốc cũ nhất */
static bool erase_if_used(uint32_t addr)
{
    if (rec_erased(addr)) return true;
    if (w25q_erase_sector(addr) != HAL_OK) return false;
    if (s_oldest / SECTOR == addr / SECTOR) s_oldest = (addr + SECTOR) % s_log_size;
    return true;
}

bool fl_os_init(void)
{
    s_mutex = xSemaphoreCreateMutex();
    return s_mutex != NULL;
}

bool fl_mount(void)
{
    if (!lock()) return false;
    s_ready = false;
    if (w25q_init() == HAL_OK && w25q_capacity() >= 4 * SECTOR) {
        uint32_t n = w25q_capacity() / SECTOR - 1;
        s_log_size = n * SECTOR;
        s_cfg_addr = s_log_size;
        s_head     = app_log_find_head(SECTOR, n, rec_erased);
        s_oldest   = app_log_find_oldest(s_head, SECTOR, n, rec_erased);
        s_ready    = true;
    }
    unlock();
    return s_ready;
}

bool fl_ready(void) { return s_ready; }

bool fl_append(const app_sample_t *s)
{
    uint8_t rec[REC], back[REC];
    app_rec_pack(s, rec);

    if (!s_ready || !lock()) return false;
    bool ok = true;
    if (s_head % SECTOR == 0) {                               /* sang sector mới */
        ok = erase_if_used(s_head) && erase_if_used((s_head + SECTOR) % s_log_size);
    }
    ok = ok && w25q_write_page(s_head, rec, REC) == HAL_OK;
    ok = ok && w25q_read(s_head, back, REC) == HAL_OK && memcmp(rec, back, REC) == 0;
    if (ok) s_head = (s_head + REC) % s_log_size;
    else    s_ready = false;                                  /* task_comm sẽ mount lại */
    unlock();
    return ok;
}

uint32_t fl_count(void)    { return s_ready ? ((s_head + s_log_size - s_oldest) % s_log_size) / REC : 0; }
uint32_t fl_capacity(void) { return s_log_size / REC; }
uint32_t fl_jedec_id(void) { return w25q_jedec_id(); }
uint32_t fl_head(void)     { return s_head; }

uint32_t fl_addr_back(uint32_t n)
{
    uint32_t c = fl_count();
    if (n > c) n = c;
    return (s_head + s_log_size - n * REC) % s_log_size;
}

uint32_t fl_addr_next(uint32_t addr) { return (addr + REC) % s_log_size; }

bool fl_read_at(uint32_t addr, app_sample_t *s)
{
    uint8_t rec[REC];
    if (!s_ready || !lock()) return false;
    bool ok = w25q_read(addr, rec, REC) == HAL_OK;
    unlock();
    return ok && app_rec_unpack(rec, s);
}

bool fl_read_newest(uint32_t k, app_sample_t *s)
{
    if (k >= fl_count()) return false;
    return fl_read_at(fl_addr_back(k + 1), s);
}

bool fl_erase_all(void)
{
    if (!lock()) return false;
    bool ok = w25q_erase_chip() == HAL_OK;
    s_head = s_oldest = 0;
    s_ready = ok;
    unlock();
    return ok;
}

bool fl_cfg_load(app_config_t *c)
{
    uint8_t rec[REC];
    if (!s_ready || !lock()) return false;
    bool ok = w25q_read(s_cfg_addr, rec, REC) == HAL_OK;
    unlock();
    return ok && app_cfg_unpack(rec, c);
}

/* ponytail: xoá + ghi 1 sector mỗi lần lưu (chịu ~100 000 lần, quá đủ khi chỉnh tay).
 * Mất điện đúng lúc lưu thì lần khởi động sau dùng ngưỡng mặc định. */
bool fl_cfg_save(const app_config_t *c)
{
    uint8_t rec[REC];
    app_cfg_pack(c, rec);
    if (!s_ready || !lock()) return false;
    bool ok = w25q_erase_sector(s_cfg_addr) == HAL_OK &&
              w25q_write_page(s_cfg_addr, rec, REC) == HAL_OK;
    unlock();
    return ok;
}
