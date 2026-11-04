/*
 * task_sensor - đọc SHT31, LDR, DS3231 theo chu kỳ, so ngưỡng, phát dữ liệu.
 * Dev 1: feature/task-sensor
 */
#include "FreeRTOS.h"
#include "task.h"
#include "task_sensor.h"
#include "app_ipc.h"
#include "app_cfg.h"
#include "bsp.h"
#include "hal_gpio.h"
#include "hal_adc.h"
#include "sht31.h"
#include "ds3231.h"

static TaskHandle_t xSensorTaskHandle = NULL;

static void read_sample(app_sample_t *s, const app_config_t *cfg)
{
    uint16_t raw;

    if (ds3231_get_time(&s->time) != HAL_OK)
        s->flags |= APP_FLAG_RTC_FAIL;
    if (sht31_measure(&s->temp_c100, &s->hum_c100) != HAL_OK)
        s->flags |= APP_FLAG_SHT_FAIL;
    if (hal_adc_read(BSP_LDR_ADC_CH, &raw) == HAL_OK)
        s->light_pct = app_light_percent(raw, APP_LDR_INVERT);
    else
        s->flags |= APP_FLAG_ADC_FAIL;

    s->flags |= app_eval_thresholds(s, cfg);
}

static void prvSensorTask(void *pvParameters)
{
    (void)pvParameters;
    uint32_t seq = 0;

    /* Ngoại vi riêng của task */
    hal_gpio_config(BSP_LDR_PORT, BSP_LDR_PIN, HAL_GPIO_IN_ANALOG);
    hal_adc_init();
    sht31_init();       /* lỗi ở đây không sao: mỗi lần đo sẽ báo cờ SHT_FAIL */
    ds3231_init();

    TickType_t xLastWakeTime = xTaskGetTickCount();
    for (;;) {
        app_config_t cfg;
        app_config_get(&cfg);

        app_sample_t s = { .seq = ++seq };
        read_sample(&s, &cfg);

        if (s.flags & APP_FLAG_ALARM_MASK) xEventGroupSetBits(xSysEvents, EVT_ALARM);
        else                               xEventGroupClearBits(xSysEvents, EVT_ALARM);

        xQueueOverwrite(xDisplayQueue, &s);              /* màn hình chỉ cần mẫu mới nhất */
        if (xQueueSend(xLogQueue, &s, 0) != pdPASS)      /* không chờ: không làm lệch chu kỳ */
            g_log_dropped++;

        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(cfg.period_ms));
    }
}

BaseType_t task_sensor_create(void)
{
    return xTaskCreate(prvSensorTask, "SENSOR", TASK_SENSOR_STACK, NULL,
                       TASK_SENSOR_PRIO, &xSensorTaskHandle);
}
