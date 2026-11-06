/*
 * app_logic - phần xử lý thuần (không đụng phần cứng, không đụng RTOS):
 * CRC/quy đổi SHT31, so ngưỡng, định dạng log CSV, phân tích lệnh UART.
 * Biên dịch được trên PC: `make test` chạy tests/test_app_logic.c
 */
#ifndef APP_LOGIC_H
#define APP_LOGIC_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    uint16_t year;
    uint8_t  mon, day, hour, min, sec;
} app_time_t;

/* Cờ trạng thái của 1 mẫu đo */
#define APP_FLAG_SHT_FAIL   (1u << 0)   /* SHT31 không trả lời / sai CRC */
#define APP_FLAG_RTC_FAIL   (1u << 1)   /* DS3231 không trả lời */
#define APP_FLAG_ADC_FAIL   (1u << 2)
#define APP_FLAG_T_HIGH     (1u << 3)
#define APP_FLAG_T_LOW      (1u << 4)
#define APP_FLAG_H_HIGH     (1u << 5)
#define APP_FLAG_H_LOW      (1u << 6)
#define APP_FLAG_SENSOR_MASK (APP_FLAG_SHT_FAIL | APP_FLAG_RTC_FAIL | APP_FLAG_ADC_FAIL)
#define APP_FLAG_ALARM_MASK  0x7Fu      /* mọi cờ trên đều phải cảnh báo */

/* Gói dữ liệu gửi qua queue (đơn vị cố định, không dùng float) */
typedef struct {
    uint32_t   seq;
    app_time_t time;
    int16_t    temp_c100;   /* nhiệt độ x100 (°C): 2531 = 25.31 °C */
    uint16_t   hum_c100;    /* độ ẩm x100 (%RH) */
    uint8_t    light_pct;   /* 0..100 % */
    uint16_t   flags;       /* APP_FLAG_* */
} app_sample_t;

/* Cấu hình chỉnh được lúc chạy qua UART */
typedef struct {
    uint32_t period_ms;
    int16_t  t_lo_c100, t_hi_c100;
    uint16_t h_lo_c100, h_hi_c100;
} app_config_t;

/* ---- SHT31 ---- */
uint8_t app_sht31_crc8(const uint8_t *data, size_t len);
/* Kiểm CRC 6 byte đọc về rồi quy đổi. Trả false nếu sai CRC. */
bool    app_sht31_decode(const uint8_t raw[6], int16_t *temp_c100, uint16_t *hum_c100);

/* ---- LDR ---- */
uint8_t app_light_percent(uint16_t adc_raw, bool invert);

/* ---- Ngưỡng: trả về các cờ APP_FLAG_T_* / APP_FLAG_H_* ---- */
uint16_t app_eval_thresholds(const app_sample_t *s, const app_config_t *cfg);

/* ---- Định dạng ---- */
uint8_t app_bcd2bin(uint8_t v);
uint8_t app_bin2bcd(uint8_t v);
bool    app_time_valid(const app_time_t *t);
/* "25.31", "-3.05" */
int     app_fmt_c100(char *buf, size_t size, int32_t v);
/* "2026-10-29 19:40:05,25.31,60.12,45,0x0000\r\n" -> trả độ dài */
int     app_format_csv(char *buf, size_t size, const app_sample_t *s);
#define APP_CSV_HEADER "time,temp_C,hum_pct,light_pct,flags\r\n"

/* ---- Bản ghi log trên Flash (16 byte, không bao giờ vắt qua ranh giới page 256 B) ----
 *  [0] 0xA5  [1..6] năm-2000,tháng,ngày,giờ,phút,giây  [7..8] temp  [9..10] hum
 *  [11] light  [12..13] flags  [14] 0xFF  [15] CRC-8 của byte 0..14 (LE cho số 16 bit) */
#define APP_REC_SIZE  16u
#define APP_REC_MARK  0xA5u
void app_rec_pack(const app_sample_t *s, uint8_t rec[APP_REC_SIZE]);
/* false nếu ô trống (0xFF) hoặc hỏng (mất điện giữa lúc ghi) */
bool app_rec_unpack(const uint8_t rec[APP_REC_SIZE], app_sample_t *s);

/* Log vòng: luôn giữ >= 1 sector trống ngay sau sector đang ghi.
 * Tìm địa chỉ ghi tiếp theo. rec_erased(addr) trả true nếu byte đầu bản ghi tại addr = 0xFF.
 * Gọi rec_erased khoảng n_sectors + log2(sector_size/16) lần. */
uint32_t app_log_find_head(uint32_t sector_size, uint32_t n_sectors,
                           bool (*rec_erased)(uint32_t addr));

/* Tìm địa chỉ bản ghi cũ nhất (luôn là đầu một sector). head: kết quả app_log_find_head.
 * Log chưa quay vòng thì trả 0. */
uint32_t app_log_find_oldest(uint32_t head, uint32_t sector_size, uint32_t n_sectors,
                             bool (*rec_erased)(uint32_t addr));

/* Khoá so sánh thời gian (giây kể từ 2000, coi mỗi tháng 31 ngày: chỉ để so sánh) */
uint32_t app_time_key(const app_time_t *t);

/* ---- Cấu hình ngưỡng ---- */
#define APP_CFG_PERIOD_MIN   500u
#define APP_CFG_PERIOD_MAX   60000u
#define APP_CFG_T_MIN_C100   (-4000)
#define APP_CFG_T_MAX_C100   12500
#define APP_CFG_H_MAX_C100   10000u
bool app_cfg_valid(const app_config_t *c);
/* Lưu cấu hình vào Flash: 16 byte có dấu + CRC */
void app_cfg_pack(const app_config_t *c, uint8_t rec[APP_REC_SIZE]);
bool app_cfg_unpack(const uint8_t rec[APP_REC_SIZE], app_config_t *c);
/* Chỉnh ngưỡng bằng nút: field 0 = T thấp, 1 = T cao, 2 = H thấp, 3 = H cao.
 * delta theo bước 1 °C / 1 %; luôn giữ thấp < cao và trong dải đo. */
#define APP_CFG_FIELDS 4
void app_cfg_adjust(app_config_t *c, int field, int delta);

/* ---- Chống dội nút + tự lặp khi giữ ----
 * Gọi định kỳ (≤ 20 ms) với mức đọc được; trả true khi có 1 lần "bấm". */
#define APP_BTN_DEBOUNCE_MS  30u
#define APP_BTN_REPEAT_DELAY 500u
#define APP_BTN_REPEAT_MS    120u
typedef struct {
    bool     stable, raw;
    uint32_t raw_since, next_repeat;
} app_btn_t;
bool app_btn_update(app_btn_t *b, bool pressed, uint32_t now_ms, bool auto_repeat);
/* true nếu nút đang bấm hoặc chưa ổn định (task cần hỏi nút dày hơn) */
bool app_btn_busy(const app_btn_t *b);

/* ---- Lệnh UART (không phân biệt hoa thường) ----
 *   HELP | GET | STAT
 *   DUMP                   xuất toàn bộ log trong Flash dạng CSV
 *   LAST <n>               xuất n mẫu gần nhất (1..600000)
 *   LOG YYYY-MM-DD                         xuất cả ngày
 *   LOG YYYY-MM-DD HH:MM YYYY-MM-DD HH:MM  xuất đoạn từ .. đến (tính cả phút cuối)
 *   ERASE                  xoá toàn bộ Flash
 *   PERIOD <ms>            500..60000
 *   TEMP <lo> <hi>         °C nguyên, -40..125, lo < hi
 *   HUM <lo> <hi>          %RH nguyên, 0..100, lo < hi
 *   TIME YYYY-MM-DD HH:MM:SS
 */
typedef enum {
    APP_CMD_NONE, APP_CMD_HELP, APP_CMD_GET, APP_CMD_STAT, APP_CMD_DUMP, APP_CMD_ERASE,
    APP_CMD_LAST, APP_CMD_LOG,
    APP_CMD_PERIOD, APP_CMD_TEMP, APP_CMD_HUM, APP_CMD_TIME, APP_CMD_INVALID,
} app_cmd_type_t;

typedef struct {
    app_cmd_type_t type;
    int32_t        a, b;       /* PERIOD: a; TEMP/HUM: a = lo, b = hi; LAST: a = n */
    app_time_t     time;       /* TIME; LOG: thời điểm bắt đầu */
    app_time_t     time_to;    /* LOG: thời điểm kết thúc (đã gồm giây 59) */
} app_cmd_t;

app_cmd_t app_parse_cmd(const char *line);

#endif /* APP_LOGIC_H */
