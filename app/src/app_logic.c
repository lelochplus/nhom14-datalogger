#include <stdio.h>
#include <stdlib.h>
#include "app_logic.h"

/* ================= SHT31 ================= */

uint8_t app_sht31_crc8(const uint8_t *data, size_t len)
{
    uint8_t crc = 0xFF;                         /* datasheet SHT3x: poly 0x31, init 0xFF */
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++)
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x31) : (uint8_t)(crc << 1);
    }
    return crc;
}

bool app_sht31_decode(const uint8_t raw[6], int16_t *temp_c100, uint16_t *hum_c100)
{
    if (app_sht31_crc8(&raw[0], 2) != raw[2] || app_sht31_crc8(&raw[3], 2) != raw[5])
        return false;

    int32_t st  = ((int32_t)raw[0] << 8) | raw[1];
    int32_t srh = ((int32_t)raw[3] << 8) | raw[4];
    /* T = -45 + 175 * St / 65535 ; RH = 100 * Srh / 65535 (làm tròn) */
    *temp_c100 = (int16_t)(-4500 + (17500 * st + 32767) / 65535);
    *hum_c100  = (uint16_t)((10000 * srh + 32767) / 65535);
    return true;
}

/* ================= LDR ================= */

uint8_t app_light_percent(uint16_t adc_raw, bool invert)
{
    if (adc_raw > 4095) adc_raw = 4095;
    uint32_t pct = ((uint32_t)adc_raw * 100u + 2047u) / 4095u;
    return (uint8_t)(invert ? 100u - pct : pct);
}

/* ================= Ngưỡng ================= */

uint16_t app_eval_thresholds(const app_sample_t *s, const app_config_t *cfg)
{
    uint16_t f = 0;
    if (s->flags & APP_FLAG_SHT_FAIL) return 0;   /* không có số liệu để so */
    if (s->temp_c100 > cfg->t_hi_c100) f |= APP_FLAG_T_HIGH;
    if (s->temp_c100 < cfg->t_lo_c100) f |= APP_FLAG_T_LOW;
    if (s->hum_c100  > cfg->h_hi_c100) f |= APP_FLAG_H_HIGH;
    if (s->hum_c100  < cfg->h_lo_c100) f |= APP_FLAG_H_LOW;
    return f;
}

/* ================= Định dạng ================= */

uint8_t app_bcd2bin(uint8_t v) { return (uint8_t)((v >> 4) * 10u + (v & 0x0Fu)); }
uint8_t app_bin2bcd(uint8_t v) { return (uint8_t)(((v / 10u) << 4) | (v % 10u)); }

bool app_time_valid(const app_time_t *t)
{
    static const uint8_t mdays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (t->year < 2000 || t->year > 2099 || t->mon < 1 || t->mon > 12) return false;
    uint8_t dmax = mdays[t->mon - 1];
    if (t->mon == 2 && (t->year % 4u) == 0) dmax = 29;   /* 2000..2099: chia 4 là đủ */
    return t->day >= 1 && t->day <= dmax && t->hour < 24 && t->min < 60 && t->sec < 60;
}

int app_fmt_c100(char *buf, size_t size, int32_t v)
{
    const char *sign = (v < 0) ? "-" : "";
    if (v < 0) v = -v;
    return snprintf(buf, size, "%s%d.%02d", sign, (int)(v / 100), (int)(v % 100));
}

int app_format_csv(char *buf, size_t size, const app_sample_t *s)
{
    char t[12] = "", h[12] = "";
    if (!(s->flags & APP_FLAG_SHT_FAIL)) {
        app_fmt_c100(t, sizeof t, s->temp_c100);
        app_fmt_c100(h, sizeof h, s->hum_c100);
    }
    return snprintf(buf, size, "%04u-%02u-%02u %02u:%02u:%02u,%s,%s,%u,0x%04X\r\n",
                    s->time.year, s->time.mon, s->time.day,
                    s->time.hour, s->time.min, s->time.sec,
                    t, h, s->light_pct, s->flags);
}

/* ================= Bản ghi Flash ================= */

void app_rec_pack(const app_sample_t *s, uint8_t r[APP_REC_SIZE])
{
    r[0]  = APP_REC_MARK;
    r[1]  = (uint8_t)(s->time.year - 2000);
    r[2]  = s->time.mon;  r[3] = s->time.day;
    r[4]  = s->time.hour; r[5] = s->time.min; r[6] = s->time.sec;
    r[7]  = (uint8_t)s->temp_c100; r[8]  = (uint8_t)((uint16_t)s->temp_c100 >> 8);
    r[9]  = (uint8_t)s->hum_c100;  r[10] = (uint8_t)(s->hum_c100 >> 8);
    r[11] = s->light_pct;
    r[12] = (uint8_t)s->flags;     r[13] = (uint8_t)(s->flags >> 8);
    r[14] = 0xFF;
    r[15] = app_sht31_crc8(r, APP_REC_SIZE - 1);   /* dùng lại CRC-8 poly 0x31 */
}

bool app_rec_unpack(const uint8_t r[APP_REC_SIZE], app_sample_t *s)
{
    if (r[0] != APP_REC_MARK || app_sht31_crc8(r, APP_REC_SIZE - 1) != r[15])
        return false;
    s->seq       = 0;
    s->time      = (app_time_t){ (uint16_t)(2000 + r[1]), r[2], r[3], r[4], r[5], r[6] };
    s->temp_c100 = (int16_t)(r[7] | (r[8] << 8));
    s->hum_c100  = (uint16_t)(r[9] | (r[10] << 8));
    s->light_pct = r[11];
    s->flags     = (uint16_t)(r[12] | (r[13] << 8));
    return true;
}

uint32_t app_log_find_head(uint32_t sector_size, uint32_t n_sectors,
                           bool (*rec_erased)(uint32_t addr))
{
    /* Sector "đã ghi" mà sector kế tiếp còn trống chính là sector đang ghi.
     * Nhờ luôn xoá trước sector kế, chỉ có đúng 1 chỗ chuyển đã ghi -> trống. */
    bool first_written = !rec_erased(0);
    bool cur = first_written;
    for (uint32_t i = 0; i < n_sectors; i++) {
        bool next = (i + 1 < n_sectors) ? !rec_erased((i + 1) * sector_size) : first_written;
        if (cur && !next) {
            /* Trong sector, bản ghi được ghi tuần tự: tìm nhị phân ô trống đầu tiên */
            uint32_t base = i * sector_size, lo = 1, hi = sector_size / APP_REC_SIZE;
            while (lo < hi) {
                uint32_t mid = (lo + hi) / 2;
                if (rec_erased(base + mid * APP_REC_SIZE)) hi = mid;
                else lo = mid + 1;
            }
            return (base + lo * APP_REC_SIZE) % (sector_size * n_sectors);
        }
        cur = next;
    }
    return 0;   /* chip trống hoàn toàn (hoặc toàn rác: cần lệnh ERASE) */
}

uint32_t app_log_find_oldest(uint32_t head, uint32_t sector_size, uint32_t n_sectors,
                             bool (*rec_erased)(uint32_t addr))
{
    /* Ngay sau sector đang ghi luôn có 1 sector đã xoá trước. Sector tiếp theo nữa
     * còn dữ liệu nghĩa là log đã quay vòng và đó là phần cũ nhất. */
    uint32_t s = (head / sector_size + 1) % n_sectors;
    if (rec_erased(s * sector_size)) s = (s + 1) % n_sectors;
    return rec_erased(s * sector_size) ? 0 : s * sector_size;
}

uint32_t app_time_key(const app_time_t *t)
{
    uint32_t d = ((uint32_t)(t->year - 2000) * 12u + (t->mon - 1u)) * 31u + (t->day - 1u);
    return ((d * 24u + t->hour) * 60u + t->min) * 60u + t->sec;
}

/* ================= Cấu hình ================= */

#define CFG_MARK 0xC5u

bool app_cfg_valid(const app_config_t *c)
{
    return c->period_ms >= APP_CFG_PERIOD_MIN && c->period_ms <= APP_CFG_PERIOD_MAX &&
           c->t_lo_c100 >= APP_CFG_T_MIN_C100 && c->t_hi_c100 <= APP_CFG_T_MAX_C100 &&
           c->t_lo_c100 < c->t_hi_c100 &&
           c->h_hi_c100 <= APP_CFG_H_MAX_C100 && c->h_lo_c100 < c->h_hi_c100;
}

void app_cfg_pack(const app_config_t *c, uint8_t r[APP_REC_SIZE])
{
    for (int i = 0; i < (int)APP_REC_SIZE; i++) r[i] = 0xFF;
    r[0] = CFG_MARK;
    r[1] = (uint8_t)c->period_ms;          r[2] = (uint8_t)(c->period_ms >> 8);
    r[3] = (uint8_t)(c->period_ms >> 16);  r[4] = (uint8_t)(c->period_ms >> 24);
    r[5] = (uint8_t)c->t_lo_c100;          r[6] = (uint8_t)((uint16_t)c->t_lo_c100 >> 8);
    r[7] = (uint8_t)c->t_hi_c100;          r[8] = (uint8_t)((uint16_t)c->t_hi_c100 >> 8);
    r[9] = (uint8_t)c->h_lo_c100;          r[10] = (uint8_t)(c->h_lo_c100 >> 8);
    r[11] = (uint8_t)c->h_hi_c100;         r[12] = (uint8_t)(c->h_hi_c100 >> 8);
    r[15] = app_sht31_crc8(r, APP_REC_SIZE - 1);
}

bool app_cfg_unpack(const uint8_t r[APP_REC_SIZE], app_config_t *c)
{
    if (r[0] != CFG_MARK || app_sht31_crc8(r, APP_REC_SIZE - 1) != r[15]) return false;
    app_config_t t = {
        .period_ms = (uint32_t)r[1] | ((uint32_t)r[2] << 8) | ((uint32_t)r[3] << 16) |
                     ((uint32_t)r[4] << 24),
        .t_lo_c100 = (int16_t)(r[5] | (r[6] << 8)),
        .t_hi_c100 = (int16_t)(r[7] | (r[8] << 8)),
        .h_lo_c100 = (uint16_t)(r[9] | (r[10] << 8)),
        .h_hi_c100 = (uint16_t)(r[11] | (r[12] << 8)),
    };
    if (!app_cfg_valid(&t)) return false;
    *c = t;
    return true;
}

static int32_t clamp(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }

void app_cfg_adjust(app_config_t *c, int field, int delta)
{
    switch (field) {
    case 0: c->t_lo_c100 = (int16_t)clamp(c->t_lo_c100 + delta * 100, APP_CFG_T_MIN_C100, c->t_hi_c100 - 100); break;
    case 1: c->t_hi_c100 = (int16_t)clamp(c->t_hi_c100 + delta * 100, c->t_lo_c100 + 100, APP_CFG_T_MAX_C100); break;
    case 2: c->h_lo_c100 = (uint16_t)clamp((int32_t)c->h_lo_c100 + delta * 100, 0, (int32_t)c->h_hi_c100 - 100); break;
    case 3: c->h_hi_c100 = (uint16_t)clamp((int32_t)c->h_hi_c100 + delta * 100, (int32_t)c->h_lo_c100 + 100, APP_CFG_H_MAX_C100); break;
    default: break;
    }
}

/* ================= Nút nhấn ================= */

bool app_btn_update(app_btn_t *b, bool pressed, uint32_t now, bool auto_repeat)
{
    if (pressed != b->raw) { b->raw = pressed; b->raw_since = now; }

    if (b->raw != b->stable && now - b->raw_since >= APP_BTN_DEBOUNCE_MS) {
        b->stable = b->raw;
        if (b->stable) { b->next_repeat = now + APP_BTN_REPEAT_DELAY; return true; }
        return false;
    }
    if (auto_repeat && b->stable && b->raw && (int32_t)(now - b->next_repeat) >= 0) {
        b->next_repeat = now + APP_BTN_REPEAT_MS;
        return true;
    }
    return false;
}

bool app_btn_busy(const app_btn_t *b) { return b->raw || b->stable; }

/* ================= Lệnh UART ================= */

static char lower(char c) { return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c; }

/* So khớp từ khoá ở đầu dòng, trả con trỏ sau từ khoá hoặc NULL */
static const char *match_kw(const char *p, const char *kw)
{
    while (*kw)
        if (lower(*p++) != *kw++) return NULL;
    return (*p == '\0' || *p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') ? p : NULL;
}

/* Đọc tối đa max số nguyên. date_seps: cho phép '-', ':', '/' làm dấu phân cách
 * (chỉ giữa các số, nên số âm vẫn đọc được ở chế độ thường). Trả số lượng, -1 nếu lỗi. */
static int parse_nums(const char *p, int32_t *out, int max, bool date_seps)
{
    int n = 0;
    for (;;) {
        while (*p == ' ' || *p == '\t' ||
               (date_seps && n > 0 && (*p == '-' || *p == ':' || *p == '/')))
            p++;
        if (*p == '\0' || *p == '\r' || *p == '\n') return n;
        if (n == max) return -1;
        char *end;
        long v = strtol(p, &end, 10);
        if (end == p) return -1;
        out[n++] = (int32_t)v;
        p = end;
    }
}

/* v[0..4] = năm, tháng, ngày, giờ, phút -> thời gian hợp lệ? */
static bool make_time(const int32_t *v, uint8_t sec, app_time_t *t)
{
    if (v[0] < 2000 || v[0] > 2099) return false;
    for (int i = 1; i < 5; i++) if (v[i] < 0 || v[i] > 59) return false;
    *t = (app_time_t){ (uint16_t)v[0], (uint8_t)v[1], (uint8_t)v[2],
                       (uint8_t)v[3], (uint8_t)v[4], sec };
    return app_time_valid(t);
}

app_cmd_t app_parse_cmd(const char *line)
{
    app_cmd_t c = { .type = APP_CMD_INVALID };
    int32_t v[10];
    const char *p;
    int n;

    while (*line == ' ' || *line == '\t') line++;
    if (*line == '\0' || *line == '\r' || *line == '\n') { c.type = APP_CMD_NONE; return c; }

    if ((p = match_kw(line, "help")) && parse_nums(p, v, 0, false) == 0) c.type = APP_CMD_HELP;
    else if ((p = match_kw(line, "get"))  && parse_nums(p, v, 0, false) == 0) c.type = APP_CMD_GET;
    else if ((p = match_kw(line, "stat")) && parse_nums(p, v, 0, false) == 0) c.type = APP_CMD_STAT;
    else if ((p = match_kw(line, "dump")) && parse_nums(p, v, 0, false) == 0) c.type = APP_CMD_DUMP;
    else if ((p = match_kw(line, "erase")) && parse_nums(p, v, 0, false) == 0) c.type = APP_CMD_ERASE;
    else if ((p = match_kw(line, "last"))) {
        if (parse_nums(p, v, 1, false) == 1 && v[0] >= 1 && v[0] <= 600000) {
            c.type = APP_CMD_LAST; c.a = v[0];
        }
    } else if ((p = match_kw(line, "log"))) {
        n = parse_nums(p, v, 10, true);
        if (n == 3) {                                   /* cả một ngày */
            int32_t f[5] = { v[0], v[1], v[2], 0, 0 }, t[5] = { v[0], v[1], v[2], 23, 59 };
            if (make_time(f, 0, &c.time) && make_time(t, 59, &c.time_to)) c.type = APP_CMD_LOG;
        } else if (n == 10) {
            if (v[3] <= 23 && v[8] <= 23 &&
                make_time(&v[0], 0, &c.time) && make_time(&v[5], 59, &c.time_to) &&
                app_time_key(&c.time) <= app_time_key(&c.time_to))
                c.type = APP_CMD_LOG;
        }
    } else if ((p = match_kw(line, "period"))) {
        if (parse_nums(p, v, 1, false) == 1 && v[0] >= (int32_t)APP_CFG_PERIOD_MIN &&
            v[0] <= (int32_t)APP_CFG_PERIOD_MAX) {
            c.type = APP_CMD_PERIOD; c.a = v[0];
        }
    } else if ((p = match_kw(line, "temp"))) {
        if (parse_nums(p, v, 2, false) == 2 && v[0] >= -40 && v[1] <= 125 && v[0] < v[1]) {
            c.type = APP_CMD_TEMP; c.a = v[0]; c.b = v[1];
        }
    } else if ((p = match_kw(line, "hum"))) {
        if (parse_nums(p, v, 2, false) == 2 && v[0] >= 0 && v[1] <= 100 && v[0] < v[1]) {
            c.type = APP_CMD_HUM; c.a = v[0]; c.b = v[1];
        }
    } else if ((p = match_kw(line, "time"))) {
        if (parse_nums(p, v, 6, true) == 6 && v[5] >= 0 && v[5] <= 59 && v[3] <= 23 &&
            make_time(v, (uint8_t)v[5], &c.time))
            c.type = APP_CMD_TIME;
    }
    return c;
}
