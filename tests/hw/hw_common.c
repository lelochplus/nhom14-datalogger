#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "hw_common.h"
#include "bsp.h"
#include "hal_gpio.h"
#include "hal_uart.h"
#include "hal_i2c.h"
#include "ssd1306.h"

/* weak: bài b1 build được ngay cả khi chưa viết driver I2C / OLED (bước 2 hướng dẫn) */
#pragma weak hal_i2c_init
#pragma weak ssd1306_init
#pragma weak ssd1306_clear
#pragma weak ssd1306_text
#pragma weak ssd1306_flush

#define LINES 8
#define COLS  21                    /* 128 px / 6 px mỗi ký tự */

bool hw_oled_ok;
static char s_lines[LINES][COLS + 1];

void hw_sleep(uint32_t ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }

void hw_log(const char *fmt, ...)
{
    char buf[96];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);

    hal_uart_puts(buf, portMAX_DELAY);
    hal_uart_puts("\r\n", portMAX_DELAY);
    hal_gpio_toggle(BSP_LED_PORT, BSP_LED_PIN);

    if (!hw_oled_ok && !(ssd1306_init && ssd1306_init() == HAL_OK)) return;   /* OLED rớt thì thử khởi tạo lại */
    hw_oled_ok = true;
    memmove(s_lines[0], s_lines[1], sizeof s_lines - sizeof s_lines[0]);
    strncpy(s_lines[LINES - 1], buf, COLS);          /* cắt còn 21 ký tự */
    s_lines[LINES - 1][COLS] = 0;
    ssd1306_clear();
    for (int i = 0; i < LINES; i++) ssd1306_text(0, i * 8, s_lines[i], 1);
    if (ssd1306_flush() != HAL_OK) hw_oled_ok = false;
}

static void runner(void *arg)
{
    (void)arg;
    hw_oled_ok = ssd1306_init && ssd1306_init() == HAL_OK;
    hal_uart_puts("\r\n\r\n", portMAX_DELAY);
    test_main();
    for (;;) hw_sleep(1000);
}

int main(void)
{
    bsp_system_init();
    hal_gpio_init();
    hal_uart_init(BSP_UART_BAUD);
    if (hal_i2c_init) hal_i2c_init();
    hal_gpio_write(BSP_LED_PORT, BSP_LED_PIN, 1);
    hal_gpio_config(BSP_LED_PORT, BSP_LED_PIN, HAL_GPIO_OUT_PP);
    xTaskCreate(runner, "test", 512, NULL, tskIDLE_PRIORITY + 1, NULL);
    vTaskStartScheduler();
    for (;;) {}
}

static void halt(void)
{
    __disable_irq();
    hal_gpio_write(BSP_LED_PORT, BSP_LED_PIN, 0);   /* LED sáng đứng = lỗi */
    for (;;) {}
}
void vApplicationStackOverflowHook(TaskHandle_t t, char *n) { (void)t; (void)n; halt(); }
void vApplicationMallocFailedHook(void) { halt(); }
