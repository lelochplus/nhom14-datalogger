#ifndef TASK_COMM_H
#define TASK_COMM_H

#include "FreeRTOS.h"

/* Tạo task; trả pdPASS nếu thành công. Gọi trong main() trước vTaskStartScheduler() */
BaseType_t task_comm_create(void);

#endif /* TASK_COMM_H */
