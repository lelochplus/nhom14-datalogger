#include "bsp.h"

#define HSE_STARTUP_LOOPS 100000u

void bsp_system_init(void)
{
    uint32_t n = 0;

    /* Flash 2 wait state cho 48..72 MHz, bật prefetch */
    FLASH->ACR = FLASH_ACR_PRFTBE | FLASH_ACR_LATENCY_2;

    RCC->CR |= RCC_CR_HSEON;
    while (!(RCC->CR & RCC_CR_HSERDY) && ++n < HSE_STARTUP_LOOPS) {}

    /* AHB /1, APB2 /1 (72 MHz), APB1 /2 (36 MHz, max), ADC /6 (12 MHz <= 14 MHz) */
    uint32_t cfgr = RCC_CFGR_HPRE_DIV1 | RCC_CFGR_PPRE2_DIV1 |
                    RCC_CFGR_PPRE1_DIV2 | RCC_CFGR_ADCPRE_DIV6;
    if (RCC->CR & RCC_CR_HSERDY)
        cfgr |= RCC_CFGR_PLLSRC | RCC_CFGR_PLLMULL9;         /* 8 MHz x 9 = 72 MHz */
    else
        cfgr |= RCC_CFGR_PLLMULL16;                           /* HSI/2 x 16 = 64 MHz */
    RCC->CFGR = cfgr;

    RCC->CR |= RCC_CR_PLLON;
    while (!(RCC->CR & RCC_CR_PLLRDY)) {}
    RCC->CFGR |= RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) {}

    SystemCoreClockUpdate();

    /* Toàn bộ 4 bit là preempt priority (FreeRTOS yêu cầu) */
    NVIC_SetPriorityGrouping(3);
}
