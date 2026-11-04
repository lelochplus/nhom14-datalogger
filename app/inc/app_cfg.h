/*
 * app_cfg.h - bảng tài nguyên RTOS nhóm đã thống nhất (Giai đoạn 2, Bước 1).
 * Đổi số trong file này phải qua PR và cả nhóm review.
 */
#ifndef APP_CFG_H
#define APP_CFG_H

#include "FreeRTOS.h"

/* ---- Độ ưu tiên: SENSOR > COMM > DISPLAY ----
 * Sensor cao nhất để chu kỳ lấy mẫu đúng; Comm (ghi Flash + UART) kế tiếp để
 * không mất log; Display thấp nhất vì trễ vài chục ms người nhìn không thấy. */
#define TASK_SENSOR_PRIO    (tskIDLE_PRIORITY + 3)
#define TASK_COMM_PRIO      (tskIDLE_PRIORITY + 2)
#define TASK_DISPLAY_PRIO   (tskIDLE_PRIORITY + 1)

/* ---- Stack (đơn vị word = 4 byte). Đo lại bằng lệnh STAT khi tích hợp ---- */
#define TASK_SENSOR_STACK   256
#define TASK_DISPLAY_STACK  320     /* vẽ biểu đồ, đọc lịch sử từ Flash */
#define TASK_COMM_STACK     384     /* snprintf + bộ đệm dòng */

/* ---- Queue ---- */
#define LOG_QUEUE_LEN       8       /* đệm ~8 s khi đang xoá sector / DUMP */
/* Display dùng hộp thư 1 phần tử (xQueueOverwrite / xQueuePeek) */

/* ---- Giá trị mặc định khi khởi động ---- */
#define CFG_DEFAULT_PERIOD_MS   1000
#define CFG_DEFAULT_T_LO_C100   1000    /* 10.00 °C */
#define CFG_DEFAULT_T_HI_C100   3500    /* 35.00 °C */
#define CFG_DEFAULT_H_LO_C100   3000    /* 30.00 %RH */
#define CFG_DEFAULT_H_HI_C100   8000    /* 80.00 %RH */

/* ---- Hiệu chỉnh phần cứng ----
 * Module LDR phổ biến (LM393): sáng hơn -> điện áp A0 thấp hơn -> đảo.
 * Nếu module của nhóm ngược lại thì đặt 0. */
#define APP_LDR_INVERT          1

#define ALARM_BLINK_MS          250     /* chu kỳ nháy LED / kêu buzzer */
#define DISPLAY_REFRESH_MS      200
#define CHART_REFRESH_MS        1000    /* trang biểu đồ đọc 120 mẫu từ Flash: làm tươi thưa hơn */
#define BUTTON_POLL_MS          20      /* hỏi nút dày khi đang bấm (chống dội, giữ để lặp) */
#define EDIT_TIMEOUT_MS         20000   /* không bấm gì 20 s thì huỷ chỉnh ngưỡng */
#define FLASH_RETRY_MS          5000    /* thử nhận diện lại W25Q sau lỗi */
#define EXPORT_CHUNK_RECORDS    32      /* số bản ghi đọc mỗi vòng lặp khi xuất dữ liệu */

#endif /* APP_CFG_H */
