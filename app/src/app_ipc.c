#include "app_ipc.h"
#include "app_cfg.h"
#include "semphr.h"

QueueHandle_t      xLogQueue;
QueueHandle_t      xDisplayQueue;
EventGroupHandle_t xSysEvents;

volatile uint32_t g_log_written;
volatile uint32_t g_log_dropped;

static SemaphoreHandle_t s_cfg_mutex;
static app_config_t s_cfg = {
    .period_ms = CFG_DEFAULT_PERIOD_MS,
    .t_lo_c100 = CFG_DEFAULT_T_LO_C100, .t_hi_c100 = CFG_DEFAULT_T_HI_C100,
    .h_lo_c100 = CFG_DEFAULT_H_LO_C100, .h_hi_c100 = CFG_DEFAULT_H_HI_C100,
};

void app_ipc_init(void)
{
    xLogQueue     = xQueueCreate(LOG_QUEUE_LEN, sizeof(app_sample_t));
    xDisplayQueue = xQueueCreate(1, sizeof(app_sample_t));
    xSysEvents    = xEventGroupCreate();
    s_cfg_mutex   = xSemaphoreCreateMutex();
    configASSERT(xLogQueue && xDisplayQueue && xSysEvents && s_cfg_mutex);
}

void app_config_get(app_config_t *out)
{
    BaseType_t ok = xSemaphoreTake(s_cfg_mutex, pdMS_TO_TICKS(100));
    configASSERT(ok == pdTRUE);         /* chỉ giữ mutex vài µs: hết 100 ms là lỗi thiết kế */
    *out = s_cfg;
    xSemaphoreGive(s_cfg_mutex);
}

void app_config_set(const app_config_t *in)
{
    BaseType_t ok = xSemaphoreTake(s_cfg_mutex, pdMS_TO_TICKS(100));
    configASSERT(ok == pdTRUE);
    s_cfg = *in;
    xSemaphoreGive(s_cfg_mutex);
}
