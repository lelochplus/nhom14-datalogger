/*
 * hal_timer - TIM2 ngắt định kỳ (Dev 3: feature/hal-timer-adc)
 * Dùng để nháy LED / bật-tắt buzzer chính xác mà không block task.
 */
#ifndef HAL_TIMER_H
#define HAL_TIMER_H

#include "hal_common.h"

/* Callback chạy trong ngắt TIM2: ngắn gọn, chỉ dùng API ...FromISR() */
typedef void (*hal_timer_cb_t)(void);

void hal_timer_start(uint32_t period_ms, hal_timer_cb_t cb);   /* 1..6553 ms */
void hal_timer_stop(void);

#endif /* HAL_TIMER_H */
