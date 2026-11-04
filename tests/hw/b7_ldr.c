/* Bước 7: module LDR (VCC 3.3V, GND, AO -> A0; DO để trống).
 * Đạt khi: che tay -> % giảm, rọi đèn điện thoại -> % tăng.
 * Nếu ngược thì đổi APP_LDR_INVERT trong app/inc/app_cfg.h. */
#include "hw_common.h"
#include "hal_adc.h"
#include "bsp.h"
#include "app_cfg.h"
#include "app_logic.h"

void test_main(void)
{
    hal_adc_init();
    for (;;) {
        uint16_t raw;
        if (hal_adc_read(BSP_LDR_ADC_CH, &raw) == HAL_OK)
            hw_log("ADC=%4u  anh sang=%3u %%", raw, app_light_percent(raw, APP_LDR_INVERT));
        else
            hw_log("ADC LOI");
        hw_sleep(500);
    }
}
