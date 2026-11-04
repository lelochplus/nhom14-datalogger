#include "sht31.h"
#include "hal_i2c.h"
#include "bsp.h"
#include "app_logic.h"
#include "FreeRTOS.h"
#include "task.h"

hal_status_t sht31_init(void)
{
    static const uint8_t soft_reset[] = {0x30, 0xA2};
    hal_status_t st = hal_i2c_write(BSP_SHT31_ADDR, soft_reset, sizeof soft_reset);
    vTaskDelay(pdMS_TO_TICKS(2));
    return st;
}

hal_status_t sht31_measure(int16_t *temp_c100, uint16_t *hum_c100)
{
    static const uint8_t single_shot_high[] = {0x24, 0x00};   /* không clock stretching */
    uint8_t raw[6];

    hal_status_t st = hal_i2c_write(BSP_SHT31_ADDR, single_shot_high, sizeof single_shot_high);
    if (st != HAL_OK) return st;

    vTaskDelay(pdMS_TO_TICKS(16));          /* tMEAS high repeatability max 15 ms */

    st = hal_i2c_read(BSP_SHT31_ADDR, raw, sizeof raw);
    if (st != HAL_OK) return st;
    return app_sht31_decode(raw, temp_c100, hum_c100) ? HAL_OK : HAL_ERROR;
}
