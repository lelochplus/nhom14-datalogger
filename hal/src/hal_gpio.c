#include "hal_gpio.h"
#include "bsp.h"

static hal_exti_cb_t s_exti_cb[16];

void hal_gpio_init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN |
                    RCC_APB2ENR_IOPCEN | RCC_APB2ENR_AFIOEN;
}

void hal_gpio_config(GPIO_TypeDef *port, uint8_t pin, hal_gpio_mode_t mode)
{
    /* 4 bit / chân: CNF[1:0] MODE[1:0] (RM0008 9.2.1) */
    static const uint8_t cfg[] = {
        [HAL_GPIO_IN_ANALOG]   = 0x0,
        [HAL_GPIO_IN_FLOAT]    = 0x4,
        [HAL_GPIO_IN_PULLUP]   = 0x8,
        [HAL_GPIO_IN_PULLDOWN] = 0x8,
        [HAL_GPIO_OUT_PP]      = 0x2,
        [HAL_GPIO_OUT_OD]      = 0x6,
        [HAL_GPIO_AF_PP]       = 0xB,
        [HAL_GPIO_AF_OD]       = 0xF,
    };
    volatile uint32_t *reg = (pin < 8) ? &port->CRL : &port->CRH;
    uint32_t shift = (pin & 7u) * 4u;

    if (mode == HAL_GPIO_IN_PULLUP)   port->BSRR = 1u << pin;
    if (mode == HAL_GPIO_IN_PULLDOWN) port->BRR  = 1u << pin;
    *reg = (*reg & ~(0xFu << shift)) | ((uint32_t)cfg[mode] << shift);
}

void hal_gpio_write(GPIO_TypeDef *port, uint8_t pin, bool level)
{
    /* BSRR/BRR ghi nguyên tử, an toàn giữa task và ISR */
    if (level) port->BSRR = 1u << pin;
    else       port->BRR  = 1u << pin;
}

void hal_gpio_toggle(GPIO_TypeDef *port, uint8_t pin)
{
    hal_gpio_write(port, pin, !(port->ODR & (1u << pin)));
}

bool hal_gpio_read(GPIO_TypeDef *port, uint8_t pin)
{
    return (port->IDR >> pin) & 1u;
}

static IRQn_Type exti_irq(uint8_t pin)
{
    if (pin <= 4)  return (IRQn_Type)(EXTI0_IRQn + pin);
    if (pin <= 9)  return EXTI9_5_IRQn;
    return EXTI15_10_IRQn;
}

void hal_gpio_exti_enable(GPIO_TypeDef *port, uint8_t pin, hal_exti_edge_t edge, hal_exti_cb_t cb)
{
    uint32_t port_idx = ((uint32_t)port - GPIOA_BASE) / (GPIOB_BASE - GPIOA_BASE);
    uint32_t shift = (pin & 3u) * 4u;

    s_exti_cb[pin] = cb;
    AFIO->EXTICR[pin >> 2] = (AFIO->EXTICR[pin >> 2] & ~(0xFu << shift)) | (port_idx << shift);

    if (edge & HAL_EXTI_RISING)  EXTI->RTSR |= 1u << pin;
    if (edge & HAL_EXTI_FALLING) EXTI->FTSR |= 1u << pin;
    EXTI->PR = 1u << pin;
    EXTI->IMR |= 1u << pin;

    NVIC_SetPriority(exti_irq(pin), BSP_IRQ_PRIO);
    NVIC_EnableIRQ(exti_irq(pin));
}

static void exti_dispatch(uint32_t mask)
{
    uint32_t pending = EXTI->PR & mask;
    EXTI->PR = pending;
    for (uint8_t pin = 0; pending; pin++, pending >>= 1)
        if ((pending & 1u) && s_exti_cb[pin]) s_exti_cb[pin]();
}

void EXTI0_IRQHandler(void)     { exti_dispatch(1u << 0); }
void EXTI1_IRQHandler(void)     { exti_dispatch(1u << 1); }
void EXTI2_IRQHandler(void)     { exti_dispatch(1u << 2); }
void EXTI3_IRQHandler(void)     { exti_dispatch(1u << 3); }
void EXTI4_IRQHandler(void)     { exti_dispatch(1u << 4); }
void EXTI9_5_IRQHandler(void)   { exti_dispatch(0x03E0u); }
void EXTI15_10_IRQHandler(void) { exti_dispatch(0xFC00u); }
