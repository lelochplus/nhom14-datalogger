/* sht31 - cảm biến nhiệt độ/độ ẩm SHT31 trên I2C1 (Dev 1: feature/task-sensor) */
#ifndef SHT31_H
#define SHT31_H

#include "hal_common.h"

hal_status_t sht31_init(void);   /* soft reset */
/* Đo 1 lần (high repeatability, ~15 ms, task ngủ trong lúc chờ). */
hal_status_t sht31_measure(int16_t *temp_c100, uint16_t *hum_c100);

#endif
