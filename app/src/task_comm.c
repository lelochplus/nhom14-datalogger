/*
 * task_comm - ghi log vào Flash W25Q64, gửi dữ liệu lên PC qua UART, nhận lệnh
 * cấu hình, xuất đoạn dữ liệu theo yêu cầu (DUMP / LAST / LOG). Dev 3: feature/task-comm
 *
 * Xuất dữ liệu chạy dần từng mẩu trong vòng lặp nên vẫn ghi log bình thường.
 * Khung xuất để máy tính (tools/datalogger.py) nhận biết:
 *   # BEGIN <lệnh>
 *   time,temp_C,hum_pct,light_pct,flags
 *   ...các dòng CSV...
 *   # END <số dòng>
 */
#include <stdio.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "task_comm.h"
#include "app_ipc.h"
#include "app_cfg.h"
#include "hal_uart.h"
#include "hal_spi.h"
#include "ds3231.h"
#include "flash_log.h"

#define UART_TIMEOUT pdMS_TO_TICKS(100)

static TaskHandle_t xCommTaskHandle = NULL;
static TickType_t   s_flash_retry_at;

/* Việc xuất dữ liệu đang chạy */
static struct {
    bool     active, filter;
    uint32_t cur, end;              /* địa chỉ tuyệt đối trên Flash */
    uint32_t key_from, key_to;      /* lọc theo thời gian (LOG) */
    uint32_t sent;
} s_exp;

static void say(const char *s) { hal_uart_puts(s, UART_TIMEOUT); }

/* ================= Flash ================= */

static void flash_try_mount(void)
{
    if (fl_ready() || (int32_t)(xTaskGetTickCount() - s_flash_retry_at) < 0) return;

    if (!fl_mount()) {
        xEventGroupClearBits(xSysEvents, EVT_FLASH_READY);
        s_flash_retry_at = xTaskGetTickCount() + pdMS_TO_TICKS(FLASH_RETRY_MS);
        say("# Flash: khong thay W25Q, thu lai sau\r\n");
        return;
    }
    char l[80];
    snprintf(l, sizeof l, "# Flash: ID %06lX, dang luu %lu/%lu mau\r\n",
             (unsigned long)fl_jedec_id(), (unsigned long)fl_count(), (unsigned long)fl_capacity());
    say(l);

    app_config_t cfg;                       /* ngưỡng đã lưu từ lần trước */
    if (fl_cfg_load(&cfg)) {
        app_config_set(&cfg);
        say("# Da nap nguong tu Flash\r\n");
    }
    xEventGroupSetBits(xSysEvents, EVT_FLASH_READY);
}

static void flash_append(const app_sample_t *s)
{
    flash_try_mount();
    if (!fl_ready()) return;
    if (fl_append(s)) {
        g_log_written++;
    } else {
        say("# Flash: loi ghi, khoi dong lai Flash sau\r\n");
        xEventGroupClearBits(xSysEvents, EVT_FLASH_READY);
        s_flash_retry_at = xTaskGetTickCount() + pdMS_TO_TICKS(FLASH_RETRY_MS);
    }
}

static void save_config(const app_config_t *cfg)
{
    app_config_set(cfg);
    if (!fl_cfg_save(cfg)) say("# Canh bao: chua luu duoc nguong vao Flash\r\n");
}

/* ================= Xuất dữ liệu ================= */

static void export_start(const char *what, uint32_t n_newest, const app_cmd_t *range)
{
    if (!fl_ready()) { say("ERR flash\r\n"); return; }
    s_exp.cur    = fl_addr_back(n_newest);
    s_exp.end    = fl_head();
    s_exp.filter = range != NULL;
    if (range) {
        s_exp.key_from = app_time_key(&range->time);
        s_exp.key_to   = app_time_key(&range->time_to);
    }
    s_exp.sent   = 0;
    s_exp.active = true;
    say("# BEGIN ");
    say(what);
    say("\r\n" APP_CSV_HEADER);
}

/* ponytail: LOG quét tuyến tính cả vùng log (~10 s khi đầy chip); tìm nhị phân theo
 * thời gian nếu cần nhanh hơn, nhưng phải xử lý trường hợp từng chỉnh lùi đồng hồ. */
static void export_step(void)
{
    char line[64];
    for (int i = 0; i < EXPORT_CHUNK_RECORDS && s_exp.cur != s_exp.end; i++) {
        app_sample_t s;
        bool ok = fl_read_at(s_exp.cur, &s);          /* trống/hỏng/đã bị ghi đè: bỏ qua */
        s_exp.cur = fl_addr_next(s_exp.cur);
        if (!ok) continue;
        if (s_exp.filter) {
            uint32_t k = app_time_key(&s.time);
            if (k < s_exp.key_from || k > s_exp.key_to) continue;
        }
        int len = app_format_csv(line, sizeof line, &s);
        if (len > 0 && len < (int)sizeof line) hal_uart_write(line, (size_t)len, UART_TIMEOUT);
        s_exp.sent++;
    }
    if (s_exp.cur == s_exp.end || !fl_ready()) {
        snprintf(line, sizeof line, "# END %lu\r\n", (unsigned long)s_exp.sent);
        say(line);
        s_exp.active = false;
    }
}

static void erase_all(void)
{
    if (!fl_ready()) { say("ERR flash\r\n"); return; }
    say("# Dang xoa toan bo Flash (W25Q64 ~20-100 s), trong luc nay khong ghi log...\r\n");
    s_exp.active = false;
    if (fl_erase_all()) {
        app_config_t cfg;
        app_config_get(&cfg);
        fl_cfg_save(&cfg);                        /* giữ lại ngưỡng đang dùng */
        say("OK\r\n");
    } else {
        say("ERR xoa\r\n");
        xEventGroupClearBits(xSysEvents, EVT_FLASH_READY);
    }
}

/* ================= Lệnh UART ================= */

static void print_config(void)
{
    char l[96], tl[12], th[12], hl[12], hh[12];
    app_config_t c;
    app_config_get(&c);
    app_fmt_c100(tl, sizeof tl, c.t_lo_c100); app_fmt_c100(th, sizeof th, c.t_hi_c100);
    app_fmt_c100(hl, sizeof hl, c.h_lo_c100); app_fmt_c100(hh, sizeof hh, c.h_hi_c100);
    snprintf(l, sizeof l, "# period=%lu ms, temp=%s..%s C, hum=%s..%s %%\r\n",
             (unsigned long)c.period_ms, tl, th, hl, hh);
    say(l);
}

static void print_stat(void)
{
    static const char *const names[] = {"SENSOR", "DISPLAY", "COMM", "IDLE"};
    char l[64];

    snprintf(l, sizeof l, "# heap trong=%u B, it nhat tung con=%u B\r\n",
             (unsigned)xPortGetFreeHeapSize(), (unsigned)xPortGetMinimumEverFreeHeapSize());
    say(l);
    for (unsigned i = 0; i < sizeof names / sizeof names[0]; i++) {
        TaskHandle_t h = xTaskGetHandle(names[i]);
        if (!h) continue;
        snprintf(l, sizeof l, "# stack %-8s con trong it nhat %u word\r\n",
                 names[i], (unsigned)uxTaskGetStackHighWaterMark(h));
        say(l);
    }
    snprintf(l, sizeof l, "# log da ghi=%lu, mat mau=%lu\r\n",
             (unsigned long)g_log_written, (unsigned long)g_log_dropped);
    say(l);
    if (fl_ready())
        snprintf(l, sizeof l, "# Flash: dang luu %lu/%lu mau\r\n",
                 (unsigned long)fl_count(), (unsigned long)fl_capacity());
    else
        snprintf(l, sizeof l, "# Flash: loi / khong co\r\n");
    say(l);
}

static void run_cmd(const char *line)
{
    app_cmd_t c = app_parse_cmd(line);
    app_config_t cfg;
    app_config_get(&cfg);

    switch (c.type) {
    case APP_CMD_NONE:
        return;
    case APP_CMD_HELP:
        say("# Lenh: GET | STAT | PERIOD <500..60000 ms> | TEMP <lo> <hi> | HUM <lo> <hi>\r\n"
            "#       TIME YYYY-MM-DD HH:MM:SS | ERASE\r\n"
            "#       DUMP | LAST <n> | LOG YYYY-MM-DD [HH:MM YYYY-MM-DD HH:MM]\r\n");
        return;
    case APP_CMD_DUMP:
        export_start("DUMP", UINT32_MAX, NULL);
        return;
    case APP_CMD_LAST:
        export_start("LAST", (uint32_t)c.a, NULL);
        return;
    case APP_CMD_LOG:
        export_start("LOG", UINT32_MAX, &c);
        return;
    case APP_CMD_ERASE:
        erase_all();
        return;
    case APP_CMD_GET:
        print_config();
        return;
    case APP_CMD_STAT:
        print_stat();
        return;
    case APP_CMD_PERIOD:
        cfg.period_ms = (uint32_t)c.a;
        break;
    case APP_CMD_TEMP:
        cfg.t_lo_c100 = (int16_t)(c.a * 100); cfg.t_hi_c100 = (int16_t)(c.b * 100);
        break;
    case APP_CMD_HUM:
        cfg.h_lo_c100 = (uint16_t)(c.a * 100); cfg.h_hi_c100 = (uint16_t)(c.b * 100);
        break;
    case APP_CMD_TIME:
        say(ds3231_set_time(&c.time) == HAL_OK ? "OK\r\n" : "ERR rtc\r\n");
        return;
    default:
        say("ERR lenh sai, go HELP\r\n");
        return;
    }
    save_config(&cfg);
    say("OK\r\n");
}

/* Gom ký tự UART thành dòng, xử lý khi gặp CR/LF */
static void poll_uart_rx(void)
{
    static char line[48];
    static size_t len;
    static bool overflow;
    char c;

    while (hal_uart_read(&c, 1, 0) == 1) {
        if (c == '\r' || c == '\n') {
            line[len] = '\0';
            if (overflow) say("ERR dong qua dai\r\n");
            else          run_cmd(line);
            len = 0;
            overflow = false;
        } else if (len < sizeof line - 1) {
            line[len++] = c;
        } else {
            overflow = true;
        }
    }
}

/* ================= Task ================= */

static void prvCommTask(void *pvParameters)
{
    (void)pvParameters;
    char line[64];

    hal_spi_init();
    say("\r\n# Nhom 14 - Data Logger STM32F103 + FreeRTOS. Go HELP de xem lenh.\r\n");
    if (ds3231_lost_power())
        say("# Canh bao: RTC tung mat nguon, hay dat gio bang lenh TIME\r\n");
    flash_try_mount();
    say(APP_CSV_HEADER);

    for (;;) {
        app_sample_t s;
        /* Chờ mẫu có timeout để vẫn xử lý lệnh UART đều đặn */
        if (xQueueReceive(xLogQueue, &s, pdMS_TO_TICKS(50)) == pdPASS) {
            flash_append(&s);
            int n = app_format_csv(line, sizeof line, &s);
            if (!s_exp.active && n > 0 && n < (int)sizeof line)   /* đang xuất thì không chen dòng */
                hal_uart_write(line, (size_t)n, UART_TIMEOUT);
        }
        if (s_exp.active) export_step();
        poll_uart_rx();
    }
}

BaseType_t task_comm_create(void)
{
    return xTaskCreate(prvCommTask, "COMM", TASK_COMM_STACK, NULL,
                       TASK_COMM_PRIO, &xCommTaskHandle);
}
