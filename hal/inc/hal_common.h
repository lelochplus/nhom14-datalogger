#ifndef HAL_COMMON_H
#define HAL_COMMON_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "stm32f1xx.h"

/* Mã trả về chung cho mọi hàm HAL */
typedef enum {
    HAL_OK      = 0,
    HAL_ERROR   = -1,   /* NACK, lỗi bus, tham số sai */
    HAL_TIMEOUT = -2,   /* hết thời gian chờ (không bao giờ chờ vô hạn) */
    HAL_BUSY    = -3,   /* không lấy được mutex trong thời gian cho phép */
} hal_status_t;

#endif /* HAL_COMMON_H */
