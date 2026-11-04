/*
 * main.c - Nhóm 14: Trạm đo môi trường và ghi log, STM32F103 + FreeRTOS.
 * Chỉ sửa ở nhánh integration/... (Giai đoạn 3).
 */
#include "FreeRTOS.h"
#include "task.h"
#include "bsp.h"
#include "hal_gpio.h"
#include "hal_uart.h"
#include "hal_i2c.h"
#include "app_ipc.h"
#include "flash_log.h"
#include "task_sensor.h"
#include "task_display.h"
#include "task_comm.h"

static void fatal(void)
{
    /* LED PC13 sáng cố định + dừng: dấu hiệu lỗi khởi tạo / tràn stack / hết heap */
    __disable_irq();
    hal_gpio_write(BSP_LED_PORT, BSP_LED_PIN, 0);
    hal_gpio_config(BSP_LED_PORT, BSP_LED_PIN, HAL_GPIO_OUT_PP);
    for (;;) {}
}

int main(void)
{
    /* 1. Hardware & HAL init (ngoại vi dùng chung) */
    bsp_system_init();
    hal_gpio_init();
    hal_uart_init(BSP_UART_BAUD);
    hal_i2c_init();

    /* 2. FreeRTOS IPC Object Creation */
    app_ipc_init();
    if (!fl_os_init()) fatal();

    /* 3. Create Tasks */
    if (task_sensor_create()  != pdPASS) fatal();
    if (task_display_create() != pdPASS) fatal();
    if (task_comm_create()    != pdPASS) fatal();

    /* 4. Start FreeRTOS Scheduler */
    vTaskStartScheduler();

    /* Chỉ tới đây nếu không đủ heap tạo Idle task */
    fatal();
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask; (void)pcTaskName;   /* gắn debugger: xem pcTaskName để biết task nào */
    fatal();
}

void vApplicationMallocFailedHook(void)
{
    fatal();
}
