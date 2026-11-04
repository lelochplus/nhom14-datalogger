#include "hal_adc.h"

#define ADC_LOOP_TIMEOUT 100000u   /* 1 lần chuyển đổi ~21 us, vòng này >> thế */

void hal_adc_init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;

    /* Kênh 0..9 lấy mẫu 239.5 chu kỳ: trở kháng ngõ ra LDR module lớn */
    ADC1->SMPR2 = 0x3FFFFFFFu;
    /* Kích hoạt bằng phần mềm (EXTSEL = 111 SWSTART) */
    ADC1->CR2 = ADC_CR2_EXTSEL | ADC_CR2_EXTTRIG | ADC_CR2_ADON;
    for (volatile int i = 0; i < 1000; i++) {}          /* tSTAB ~1 us */

    ADC1->CR2 |= ADC_CR2_RSTCAL;
    for (uint32_t n = 0; (ADC1->CR2 & ADC_CR2_RSTCAL) && n < ADC_LOOP_TIMEOUT; n++) {}
    ADC1->CR2 |= ADC_CR2_CAL;
    for (uint32_t n = 0; (ADC1->CR2 & ADC_CR2_CAL) && n < ADC_LOOP_TIMEOUT; n++) {}
}

hal_status_t hal_adc_read(uint8_t channel, uint16_t *out)
{
    if (channel > 17 || out == NULL) return HAL_ERROR;

    ADC1->SQR1 = 0;                 /* 1 chuyển đổi */
    ADC1->SQR3 = channel;
    ADC1->SR = 0;
    ADC1->CR2 |= ADC_CR2_SWSTART;

    for (uint32_t n = 0; !(ADC1->SR & ADC_SR_EOC); n++)
        if (n >= ADC_LOOP_TIMEOUT) return HAL_TIMEOUT;

    *out = (uint16_t)ADC1->DR;
    return HAL_OK;
}
