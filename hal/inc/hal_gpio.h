/*
 * hal_gpio - GPIO và ngắt ngoài EXTI (Dev 1: feature/hal-gpio-uart)
 */
#ifndef HAL_GPIO_H
#define HAL_GPIO_H

#include "hal_common.h"

typedef enum {
    HAL_GPIO_IN_ANALOG,
    HAL_GPIO_IN_FLOAT,
    HAL_GPIO_IN_PULLUP,
    HAL_GPIO_IN_PULLDOWN,
    HAL_GPIO_OUT_PP,        /* output push-pull 2 MHz */
    HAL_GPIO_OUT_OD,
    HAL_GPIO_AF_PP,         /* alternate function, 50 MHz */
    HAL_GPIO_AF_OD,
} hal_gpio_mode_t;

typedef enum { HAL_EXTI_RISING = 1, HAL_EXTI_FALLING = 2, HAL_EXTI_BOTH = 3 } hal_exti_edge_t;

/* Callback chạy trong ngữ cảnh ngắt: chỉ được dùng API ...FromISR() */
typedef void (*hal_exti_cb_t)(void);

void hal_gpio_init(void);   /* bật clock GPIOA/B/C + AFIO */
void hal_gpio_config(GPIO_TypeDef *port, uint8_t pin, hal_gpio_mode_t mode);
void hal_gpio_write(GPIO_TypeDef *port, uint8_t pin, bool level);
void hal_gpio_toggle(GPIO_TypeDef *port, uint8_t pin);
bool hal_gpio_read(GPIO_TypeDef *port, uint8_t pin);

/* Gắn ngắt ngoài cho 1 chân (mỗi số pin 0..15 chỉ dùng được cho 1 port) */
void hal_gpio_exti_enable(GPIO_TypeDef *port, uint8_t pin, hal_exti_edge_t edge, hal_exti_cb_t cb);

#endif /* HAL_GPIO_H */
