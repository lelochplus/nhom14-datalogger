/* ds3231 - RTC DS3231 trên I2C1 (Dev 1: feature/task-sensor) */
#ifndef DS3231_H
#define DS3231_H

#include "hal_common.h"
#include "app_logic.h"

hal_status_t ds3231_init(void);                         /* bật dao động, tắt ngõ SQW */
hal_status_t ds3231_get_time(app_time_t *t);            /* chế độ 24 h */
hal_status_t ds3231_set_time(const app_time_t *t);      /* xoá luôn cờ OSF */
/* true nếu dao động từng dừng (mất pin) -> giờ trong RTC không tin được */
bool ds3231_lost_power(void);

#endif
