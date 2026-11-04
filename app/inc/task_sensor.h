#ifndef TASK_SENSOR_H
#define TASK_SENSOR_H

#include "FreeRTOS.h"

/* Tạo task; trả pdPASS nếu thành công. Gọi trong main() trước vTaskStartScheduler() */
BaseType_t task_sensor_create(void);

#endif /* TASK_SENSOR_H */
