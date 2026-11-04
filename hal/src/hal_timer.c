#include "hal_timer.h"
#include "bsp.h"

static hal_timer_cb_t s_cb;

void hal_timer_start(uint32_t period_ms, hal_timer_cb_t cb)
{
    if (period_ms == 0 || period_ms > 6553 || cb == NULL) return;

    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
    s_cb = cb;

    /* APB1 chia 2 nên clock timer = 2 x PCLK1 = HCLK. Đếm 10 kHz. */
    TIM2->CR1 = 0;
    TIM2->PSC = SystemCoreClock / 10000u - 1u;
    TIM2->ARR = period_ms * 10u - 1u;
    TIM2->EGR = TIM_EGR_UG;          /* nạp PSC/ARR ngay */
    TIM2->SR = 0;
    TIM2->DIER = TIM_DIER_UIE;

    NVIC_SetPriority(TIM2_IRQn, BSP_IRQ_PRIO);
    NVIC_EnableIRQ(TIM2_IRQn);
    TIM2->CR1 = TIM_CR1_CEN;
}

void hal_timer_stop(void)
{
    TIM2->CR1 = 0;
    TIM2->DIER = 0;
    NVIC_DisableIRQ(TIM2_IRQn);
}

void TIM2_IRQHandler(void)
{
    if (TIM2->SR & TIM_SR_UIF) {
        TIM2->SR = ~TIM_SR_UIF;
        if (s_cb) s_cb();
    }
}
