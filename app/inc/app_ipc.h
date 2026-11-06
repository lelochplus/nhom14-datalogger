/*
 * app_ipc.h - đối tượng IPC dùng chung giữa các task (Giai đoạn 1, Bước 2).
 *
 *   task_sensor --xLogQueue (FIFO)------------> task_comm   (UART + Flash W25Q64)
 *   task_sensor --xDisplayQueue (hộp thư 1)---> task_display (OLED, buzzer, LED)
 *   task_sensor --xSysEvents: EVT_ALARM-------> task_display
 *   task_comm   --xSysEvents: EVT_FLASH_READY-> task_display
 *   task_comm   --app_config_set()------------> task_sensor  (PERIOD/TEMP/HUM)
 *
 * Bus I2C và UART đã được HAL tự khoá bằng mutex.
 */
#ifndef APP_IPC_H
#define APP_IPC_H

#include "FreeRTOS.h"
#include "queue.h"
#include "event_groups.h"
#include "app_logic.h"

#define EVT_ALARM       (1u << 0)   /* mẫu mới nhất có cảnh báo */
#define EVT_FLASH_READY (1u << 1)   /* W25Q đã nhận diện, ghi được */

extern QueueHandle_t      xLogQueue;      /* app_sample_t, LOG_QUEUE_LEN phần tử */
extern QueueHandle_t      xDisplayQueue;  /* app_sample_t, 1 phần tử */
extern EventGroupHandle_t xSysEvents;

/* Bộ đếm thống kê: mỗi biến chỉ 1 task ghi, task khác chỉ đọc (32 bit là nguyên tử) */
extern volatile uint32_t g_log_written;   /* task_comm ghi: số bản ghi đã ghi Flash */
extern volatile uint32_t g_log_dropped;   /* task_sensor ghi: số mẫu rơi vì queue đầy */

/* Gọi 1 lần trong main() trước khi tạo task */
void app_ipc_init(void);

/* Cấu hình runtime, bảo vệ bằng mutex */
void app_config_get(app_config_t *out);
void app_config_set(const app_config_t *in);

#endif /* APP_IPC_H */
