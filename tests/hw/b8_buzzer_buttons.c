/* Bước 8: còi (I/O -> B1) và 4 nút (B0 PAGE, B10 SET, B11 +, B12 -; chân kia xuống GND).
 * Lúc khởi động còi kêu 3 tiếng ngắn. Bấm nút nào -> in tên nút + còi kêu "bíp".
 * Nếu còi kêu liên tục khi KHÔNG bấm gì -> sai mức kích, đổi BSP_BUZZER_ON_LEVEL. */
#include "hw_common.h"
#include "hal_gpio.h"
#include "bsp.h"

static void buzz(bool on) { hal_gpio_write(BSP_BUZZER_PORT, BSP_BUZZER_PIN, on == BSP_BUZZER_ON_LEVEL); }

static void beep(uint32_t ms) { buzz(true); hw_sleep(ms); buzz(false); }

void test_main(void)
{
    static const struct { uint8_t pin; const char *name; } btn[] = {
        {BSP_BTN_PAGE_PIN, "PAGE (B0)"}, {BSP_BTN_SET_PIN, "SET (B10)"},
        {BSP_BTN_PLUS_PIN, "+ (B11)"},   {BSP_BTN_MINUS_PIN, "- (B12)"},
    };
    bool prev[4] = {false};

    buzz(false);
    hal_gpio_config(BSP_BUZZER_PORT, BSP_BUZZER_PIN, HAL_GPIO_OUT_PP);
    for (int i = 0; i < 4; i++) hal_gpio_config(BSP_BTN_PORT, btn[i].pin, HAL_GPIO_IN_PULLUP);

    hw_log("B8: coi keu 3 tieng...");
    for (int i = 0; i < 3; i++) { beep(150); hw_sleep(150); }
    hw_log("Bam tung nut de thu");

    for (;;) {
        for (int i = 0; i < 4; i++) {
            bool down = !hal_gpio_read(BSP_BTN_PORT, btn[i].pin);   /* nối GND = 0 */
            if (down && !prev[i]) { hw_log("Nut %s", btn[i].name); beep(60); }
            prev[i] = down;
        }
        hw_sleep(20);
    }
}
