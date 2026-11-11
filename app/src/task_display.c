/*
 * task_display - OLED 5 trang, 4 nút nhấn (EXTI), chỉnh ngưỡng bằng nút,
 * xem lịch sử và biểu đồ đọc từ Flash; buzzer + LED cảnh báo do TIM2 định thời.
 * Dev 2: feature/task-display
 *
 *   Trang      | SET                 | +                  | -
 *   1 Số đo    | -                   | -                  | -
 *   2 Ngưỡng   | vào chỉnh / mục kế  | tăng 1 °C / 1 %    | giảm 1 °C / 1 %
 *              | (mục cuối: lưu)     |                    |
 *   3 Lịch sử  | về mẫu mới nhất     | lùi về mẫu cũ hơn  | tiến tới mẫu mới hơn
 *   4 Biểu đồ  | đổi đại lượng T/H/A | thu nhỏ (xem dài)  | phóng to (xem ngắn)
 *   5 Hệ thống | -                   | -                  | -
 * PAGE: chuyển trang (đang chỉnh ngưỡng thì huỷ, không lưu).
 */
#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"
#include "task_display.h"
#include "app_ipc.h"
#include "app_cfg.h"
#include "bsp.h"
#include "hal_gpio.h"
#include "hal_timer.h"
#include "ssd1306.h"
#include "flash_log.h"

enum { PG_MAIN, PG_THRESH, PG_HISTORY, PG_CHART, PG_SYSTEM, PG_COUNT };
enum { BTN_PAGE, BTN_SET, BTN_PLUS, BTN_MINUS, BTN_COUNT };
#define EV(b) (1u << (b))

#define HIST_PER_SCREEN 3
#define CHART_POINTS    120
#define CHART_TOP       12
#define CHART_BOTTOM    63

static const uint8_t s_btn_pin[BTN_COUNT]    = { BSP_BTN_PAGE_PIN, BSP_BTN_SET_PIN,
                                                 BSP_BTN_PLUS_PIN, BSP_BTN_MINUS_PIN };
static const bool    s_btn_repeat[BTN_COUNT] = { false, false, true, true };
static const uint16_t s_chart_step[] = { 1, 10, 60, 600 };     /* số mẫu / 1 điểm ảnh */

static TaskHandle_t xDisplayTaskHandle = NULL;
static volatile bool s_alarm_active;

/* Trạng thái giao diện (chỉ task_display dùng) */
static struct {
    uint8_t      page;
    int8_t       edit;              /* -1: không chỉnh; 0..3: mục đang chỉnh */
    app_config_t edit_cfg;
    TickType_t   saved_at;          /* hiện "DA LUU" vài giây */
    uint32_t     hist_off;          /* lịch sử: số mẫu lùi từ mới nhất */
    uint8_t      chart_qty;         /* 0 T, 1 H, 2 ánh sáng */
    uint8_t      chart_zoom;        /* chỉ số s_chart_step */
} ui = { .edit = -1, .saved_at = 0 };

/* ---------- Ngắt ---------- */

static void button_isr(void)                 /* EXTI0 / EXTI15_10, ngữ cảnh ngắt */
{
    BaseType_t woken = pdFALSE;
    vTaskNotifyGiveFromISR(xDisplayTaskHandle, &woken);
    portYIELD_FROM_ISR(woken);
}

static void blink_isr(void)                  /* TIM2 mỗi ALARM_BLINK_MS */
{
    static bool phase;
    phase = s_alarm_active ? !phase : false;
    hal_gpio_write(BSP_BUZZER_PORT, BSP_BUZZER_PIN, phase == BSP_BUZZER_ON_LEVEL);
    hal_gpio_write(BSP_LED_PORT, BSP_LED_PIN, !phase);   /* LED tích cực mức 0 */
}

/* ---------- Xử lý nút ---------- */

static void handle_buttons(uint32_t ev, TickType_t now)
{
    if (ev & EV(BTN_PAGE)) {
        ui.edit = -1;                                    /* rời trang: huỷ chỉnh dở */
        ui.page = (uint8_t)((ui.page + 1) % PG_COUNT);
        return;
    }

    switch (ui.page) {
    case PG_THRESH:
        if (ev & EV(BTN_SET)) {
            if (ui.edit < 0) {
                app_config_get(&ui.edit_cfg);
                ui.edit = 0;
            } else if (++ui.edit >= APP_CFG_FIELDS) {    /* qua mục cuối: lưu */
                app_config_t cur;
                app_config_get(&cur);
                ui.edit_cfg.period_ms = cur.period_ms;   /* PC có thể vừa đổi chu kỳ */
                app_config_set(&ui.edit_cfg);
                fl_cfg_save(&ui.edit_cfg);
                ui.edit = -1;
                ui.saved_at = now ? now : 1;
            }
        }
        if (ui.edit >= 0) {
            if (ev & EV(BTN_PLUS))  app_cfg_adjust(&ui.edit_cfg, ui.edit, +1);
            if (ev & EV(BTN_MINUS)) app_cfg_adjust(&ui.edit_cfg, ui.edit, -1);
        }
        break;

    case PG_HISTORY: {
        uint32_t cnt = fl_count();
        if (ev & EV(BTN_SET))   ui.hist_off = 0;
        if (ev & EV(BTN_PLUS))  ui.hist_off += HIST_PER_SCREEN;
        if ((ev & EV(BTN_MINUS)) && ui.hist_off >= HIST_PER_SCREEN) ui.hist_off -= HIST_PER_SCREEN;
        if (cnt && ui.hist_off >= cnt) ui.hist_off = (cnt - 1) / HIST_PER_SCREEN * HIST_PER_SCREEN;
        break;
    }

    case PG_CHART:
        if (ev & EV(BTN_SET)) ui.chart_qty = (uint8_t)((ui.chart_qty + 1) % 3);
        if ((ev & EV(BTN_PLUS)) && ui.chart_zoom + 1u < sizeof s_chart_step / sizeof s_chart_step[0])
            ui.chart_zoom++;
        if ((ev & EV(BTN_MINUS)) && ui.chart_zoom > 0) ui.chart_zoom--;
        break;

    default:
        break;
    }
}

/* ---------- Vẽ các trang ---------- */

static void title(const char *name, int page)
{
    char l[32];
    snprintf(l, sizeof l, "%-17s%d/%d", name, page + 1, PG_COUNT);
    ssd1306_text(0, 0, l, 1);
    ssd1306_hline(9);
}

static void alarm_text(char *buf, size_t n, uint16_t f)
{
    if (f & APP_FLAG_SENSOR_MASK)
        snprintf(buf, n, "LOI:%s%s%s", (f & APP_FLAG_SHT_FAIL) ? " SHT30" : "",
                 (f & APP_FLAG_RTC_FAIL) ? " RTC" : "", (f & APP_FLAG_ADC_FAIL) ? " ADC" : "");
    else if (f & APP_FLAG_ALARM_MASK)
        snprintf(buf, n, "CANH BAO:%s%s%s%s", (f & APP_FLAG_T_HIGH) ? " T>" : "",
                 (f & APP_FLAG_T_LOW) ? " T<" : "", (f & APP_FLAG_H_HIGH) ? " H>" : "",
                 (f & APP_FLAG_H_LOW) ? " H<" : "");
    else
        snprintf(buf, n, "Trang thai: OK");
}

static void page_main(const app_sample_t *s, bool flash_ok)
{
    char l[32], a[12];

    snprintf(l, sizeof l, "%02u:%02u:%02u  %02u/%02u/%04u", s->time.hour, s->time.min,
             s->time.sec, s->time.day, s->time.mon, s->time.year);
    ssd1306_text(0, 0, l, 1);
    ssd1306_hline(10);

    if (s->flags & APP_FLAG_SHT_FAIL) {
        ssd1306_text(0, 14, "T --.--C", 2);
        ssd1306_text(0, 31, "H --.--%", 2);
    } else {
        app_fmt_c100(a, sizeof a, s->temp_c100);
        snprintf(l, sizeof l, "T %sC", a);
        ssd1306_text(0, 14, l, 2);
        app_fmt_c100(a, sizeof a, s->hum_c100);
        snprintf(l, sizeof l, "H %s%%", a);
        ssd1306_text(0, 31, l, 2);
    }
    snprintf(l, sizeof l, "Anh sang %3u%%  FL:%s", s->light_pct, flash_ok ? "OK" : "--");
    ssd1306_text(0, 48, l, 1);
    alarm_text(l, sizeof l, s->flags);
    ssd1306_text(0, 56, l, 1);
}

static void page_threshold(TickType_t now)
{
    static const char *const names[APP_CFG_FIELDS] = { "T thap", "T cao ", "H thap", "H cao " };
    char l[32];
    app_config_t c;

    if (ui.edit >= 0) c = ui.edit_cfg;
    else              app_config_get(&c);
    const int v[APP_CFG_FIELDS] = { c.t_lo_c100 / 100, c.t_hi_c100 / 100,
                                    c.h_lo_c100 / 100, c.h_hi_c100 / 100 };

    title(ui.edit >= 0 ? "CHINH NGUONG" : "NGUONG CANH BAO", PG_THRESH);
    for (int i = 0; i < APP_CFG_FIELDS; i++) {
        snprintf(l, sizeof l, "%c %s: %4d %s", ui.edit == i ? '>' : ' ', names[i], v[i],
                 i < 2 ? "C" : "%");
        ssd1306_text(0, 13 + i * 10, l, 1);
    }
    if (ui.saved_at && now - ui.saved_at < pdMS_TO_TICKS(2000))
        ssd1306_text(0, 56, "  DA LUU VAO FLASH!", 1);
    else
        ssd1306_text(0, 56, ui.edit >= 0 ? "SET:tiep  +/-:doi" : "SET: chinh nguong", 1);
}

static void page_history(void)
{
    char l[32], t[12], h[12];
    uint32_t cnt = fl_count();

    snprintf(l, sizeof l, "LICH SU %lu/%lu", (unsigned long)(cnt ? ui.hist_off + 1 : 0),
             (unsigned long)cnt);
    ssd1306_text(0, 0, l, 1);
    ssd1306_hline(9);
    if (!fl_ready() || cnt == 0) {
        ssd1306_text(0, 28, fl_ready() ? "Chua co du lieu" : "Flash chua san sang", 1);
        return;
    }
    for (int i = 0; i < HIST_PER_SCREEN; i++) {
        app_sample_t s;
        int y = 12 + i * 17;
        if (!fl_read_newest(ui.hist_off + (uint32_t)i, &s)) {
            if (ui.hist_off + (uint32_t)i < cnt) ssd1306_text(0, y, "(ban ghi hong)", 1);
            continue;
        }
        snprintf(l, sizeof l, "%02u/%02u %02u:%02u:%02u %s", s.time.day, s.time.mon,
                 s.time.hour, s.time.min, s.time.sec,
                 (s.flags & APP_FLAG_ALARM_MASK) ? "!" : "");
        ssd1306_text(0, y, l, 1);
        if (s.flags & APP_FLAG_SHT_FAIL) {
            snprintf(l, sizeof l, " T--   H--   A%u", s.light_pct);
        } else {
            app_fmt_c100(t, sizeof t, s.temp_c100);
            app_fmt_c100(h, sizeof h, s.hum_c100);
            snprintf(l, sizeof l, " T%s H%s A%u", t, h, s.light_pct);
        }
        ssd1306_text(0, y + 8, l, 1);
    }
}

/* Biểu đồ: trục dọc ghi giá trị lớn nhất/nhỏ nhất (số nguyên), trục ngang là thời gian
 * (trái = cũ nhất, phải = hiện tại), nét chấm ngang là ngưỡng cảnh báo. */
static int32_t floor_div100(int32_t v) { return v >= 0 ? v / 100 : -((-v + 99) / 100); }
static int32_t ceil_div100(int32_t v)  { return -floor_div100(-v); }

static void page_chart(void)
{
    enum { N = 100, X0 = 27, TOP = 12, BOT = 54 };
    static int16_t val[N];
    static bool    has[N];
    static const char *const qname[3] = { "NHIET DO (C)", "DO AM (%)", "ANH SANG (%)" };
    static const int32_t min_span[3] = { 2, 5, 10 };   /* thang tối thiểu: không phóng to nhiễu */
    uint32_t step = s_chart_step[ui.chart_zoom];
    int32_t lo = INT32_MAX, hi = INT32_MIN;
    char l[48], ts[24];

    for (int i = 0; i < N; i++) {
        app_sample_t s;
        uint32_t k = (uint32_t)(N - 1 - i) * step;
        has[i] = fl_read_newest(k, &s) && !(ui.chart_qty < 2 && (s.flags & APP_FLAG_SHT_FAIL));
        if (!has[i]) continue;
        val[i] = ui.chart_qty == 0 ? s.temp_c100 :
                 ui.chart_qty == 1 ? (int16_t)s.hum_c100 : (int16_t)(s.light_pct * 100);
        if (val[i] < lo) lo = val[i];
        if (val[i] > hi) hi = val[i];
    }

    /* Tiêu đề: tên đại lượng + độ dài khoảng thời gian đang xem */
    app_config_t c;
    app_config_get(&c);
    uint32_t span = (uint32_t)((uint64_t)N * step * c.period_ms / 60000u);   /* phút */
    if (span < 120) snprintf(ts, sizeof ts, "%lu phut", (unsigned long)span);
    else            snprintf(ts, sizeof ts, "%lu gio", (unsigned long)(span / 60));
    snprintf(l, sizeof l, "%-13s%8s", qname[ui.chart_qty], ts);
    ssd1306_text(0, 0, l, 1);
    ssd1306_hline(9);
    if (lo > hi) { ssd1306_text(0, 28, "Chua co du lieu", 1); return; }

    /* Thang đo: làm tròn ra số nguyên, rộng ít nhất min_span đơn vị */
    lo = floor_div100(lo);
    hi = ceil_div100(hi);
    int32_t need = min_span[ui.chart_qty] - (hi - lo);
    if (need > 0) { lo -= need / 2; hi += need - need / 2; }
    if (ui.chart_qty > 0) {                              /* % nằm trong 0..100 */
        if (lo < 0)   { hi -= lo; lo = 0; }
        if (hi > 100) { lo -= hi - 100; hi = 100; }
    }
    const int32_t range = (hi - lo) * 100, h = BOT - TOP;

    /* Trục dọc: nhãn max ở trên, min ở dưới */
    snprintf(l, sizeof l, "%4ld", (long)hi); ssd1306_text(0, TOP - 1, l, 1);
    snprintf(l, sizeof l, "%4ld", (long)lo); ssd1306_text(0, BOT - 6, l, 1);
    for (int y = TOP; y <= BOT; y += 2) ssd1306_pixel(X0 - 2, y);

    /* Ngưỡng cảnh báo (nhiệt độ, độ ẩm): nét chấm ngang */
    if (ui.chart_qty < 2) {
        int32_t th[2] = { ui.chart_qty == 0 ? c.t_lo_c100 : c.h_lo_c100,
                          ui.chart_qty == 0 ? c.t_hi_c100 : c.h_hi_c100 };
        for (int j = 0; j < 2; j++) {
            if (th[j] < lo * 100 || th[j] > hi * 100) continue;
            int y = BOT - (int)((th[j] - lo * 100) * h / range);
            for (int x = X0; x < SSD1306_W; x += 4) ssd1306_pixel(x, y);
        }
    }

    /* Đường dữ liệu: nối điểm liên tiếp bằng đoạn thẳng đứng */
    int prev = -1;
    for (int i = 0; i < N; i++) {
        if (!has[i]) { prev = -1; continue; }
        int y = BOT - (int)((val[i] - lo * 100) * h / range);
        int a = prev < 0 ? y : prev;
        for (int yy = (y < a ? y : a); yy <= (y > a ? y : a); yy++) ssd1306_pixel(X0 + i, yy);
        prev = y;
    }

    /* Trục ngang: trái là quá khứ, phải là hiện tại */
    snprintf(l, sizeof l, "-%s", ts);
    ssd1306_text(X0, 57, l, 1);
    ssd1306_text(SSD1306_W - 18, 57, "nay", 1);
}

static void page_system(bool flash_ok)
{
    char l[32];
    uint32_t up = xTaskGetTickCount() / configTICK_RATE_HZ;
    app_config_t c;
    app_config_get(&c);

    title("HE THONG", PG_SYSTEM);
    if (flash_ok) snprintf(l, sizeof l, "Flash: %lu/%luk", (unsigned long)fl_count(),
                           (unsigned long)(fl_capacity() / 1000));
    else          snprintf(l, sizeof l, "Flash: LOI/KHONG THAY");
    ssd1306_text(0, 13, l, 1);
    snprintf(l, sizeof l, "Ghi:%lu Mat:%lu", (unsigned long)g_log_written, (unsigned long)g_log_dropped);
    ssd1306_text(0, 23, l, 1);
    snprintf(l, sizeof l, "Chu ky do: %lu ms", (unsigned long)c.period_ms);
    ssd1306_text(0, 33, l, 1);
    snprintf(l, sizeof l, "Heap trong: %u B", (unsigned)xPortGetFreeHeapSize());
    ssd1306_text(0, 43, l, 1);
    snprintf(l, sizeof l, "Uptime %02lu:%02lu:%02lu", (unsigned long)(up / 3600),
             (unsigned long)(up / 60 % 60), (unsigned long)(up % 60));
    ssd1306_text(0, 53, l, 1);
}

/* ---------- Task ---------- */

static void prvDisplayTask(void *pvParameters)
{
    (void)pvParameters;
    static app_btn_t btn[BTN_COUNT];
    bool oled_ok = false;
    TickType_t last_draw = 0, last_btn = 0;

    /* Ngoại vi riêng của task: buzzer, LED, 4 nút, TIM2 */
    hal_gpio_write(BSP_BUZZER_PORT, BSP_BUZZER_PIN, !BSP_BUZZER_ON_LEVEL);   /* tắt */
    hal_gpio_config(BSP_BUZZER_PORT, BSP_BUZZER_PIN, HAL_GPIO_OUT_PP);
    hal_gpio_write(BSP_LED_PORT, BSP_LED_PIN, 1);
    hal_gpio_config(BSP_LED_PORT, BSP_LED_PIN, HAL_GPIO_OUT_PP);
    for (int i = 0; i < BTN_COUNT; i++) {
        hal_gpio_config(BSP_BTN_PORT, s_btn_pin[i], HAL_GPIO_IN_PULLUP);
        hal_gpio_exti_enable(BSP_BTN_PORT, s_btn_pin[i], HAL_EXTI_FALLING, button_isr);
    }
    hal_timer_start(ALARM_BLINK_MS, blink_isr);

    for (;;) {
        /* Đang có nút bấm thì hỏi nút mỗi 20 ms (chống dội, giữ để lặp);
         * bình thường ngủ tới lần làm tươi kế tiếp hoặc tới khi ngắt nút đánh thức. */
        bool busy = false;
        for (int i = 0; i < BTN_COUNT; i++) busy |= app_btn_busy(&btn[i]);
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(busy ? BUTTON_POLL_MS : DISPLAY_REFRESH_MS));

        TickType_t now = xTaskGetTickCount();
        uint32_t ev = 0;
        for (int i = 0; i < BTN_COUNT; i++) {
            bool pressed = !hal_gpio_read(BSP_BTN_PORT, s_btn_pin[i]);
            busy |= pressed;
            if (app_btn_update(&btn[i], pressed, now * portTICK_PERIOD_MS, s_btn_repeat[i]))
                ev |= EV(i);
        }
        if (ev) { handle_buttons(ev, now); last_btn = now; }
        if (ui.edit >= 0 && now - last_btn > pdMS_TO_TICKS(EDIT_TIMEOUT_MS)) ui.edit = -1;

        EventBits_t sys = xEventGroupGetBits(xSysEvents);
        s_alarm_active = (sys & EVT_ALARM) != 0;
        bool flash_ok = (sys & EVT_FLASH_READY) != 0;

        /* Vẽ khi có nút bấm hoặc tới hạn làm tươi (trang biểu đồ thưa hơn) */
        TickType_t period = pdMS_TO_TICKS(ui.page == PG_CHART ? CHART_REFRESH_MS : DISPLAY_REFRESH_MS);
        if (!ev && now - last_draw < period) continue;
        last_draw = now;

        if (!oled_ok) oled_ok = ssd1306_init() == HAL_OK;   /* cắm lại OLED vẫn chạy */
        if (!oled_ok) continue;

        app_sample_t s;
        ssd1306_clear();
        switch (ui.page) {
        case PG_MAIN:
            if (xQueuePeek(xDisplayQueue, &s, 0) == pdPASS) page_main(&s, flash_ok);
            else ssd1306_text(0, 28, "Dang khoi dong...", 1);
            break;
        case PG_THRESH:  page_threshold(now); break;
        case PG_HISTORY: page_history();      break;
        case PG_CHART:   page_chart();        break;
        default:         page_system(flash_ok); break;
        }
        oled_ok = ssd1306_flush() == HAL_OK;
    }
}

BaseType_t task_display_create(void)
{
    return xTaskCreate(prvDisplayTask, "DISPLAY", TASK_DISPLAY_STACK, NULL,
                       TASK_DISPLAY_PRIO, &xDisplayTaskHandle);
}
