/*
 * hal_adc - ADC1 chuyển đổi đơn (Dev 3: feature/hal-timer-adc)
 */
#ifndef HAL_ADC_H
#define HAL_ADC_H

#include "hal_common.h"

void hal_adc_init(void);                                  /* bật ADC1 + hiệu chuẩn */
hal_status_t hal_adc_read(uint8_t channel, uint16_t *out); /* 12 bit, 0..4095 */

#endif /* HAL_ADC_H */
