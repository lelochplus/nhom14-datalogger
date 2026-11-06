#ifndef TASK_DISPLAY_H
#define TASK_DISPLAY_H

#include "FreeRTOS.h"

/* Tạo task; trả pdPASS nếu thành công. Gọi trong main() trước vTaskStartScheduler() */
BaseType_t task_display_create(void);

#endif /* TASK_DISPLAY_H */
