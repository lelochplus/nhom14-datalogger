/*
 * bsp.h - Board support & pin mapping (STM32F103C8T6 "Blue Pill").
 * Mọi chân phần cứng chỉ được định nghĩa ở đây, đúng theo đề cương mục 4.
 */
#ifndef BSP_H
#define BSP_H

#include "stm32f1xx.h"

/* ---- I2C1: SHT31 (0x44), SSD1306 (0x3C), DS3231 (0x68) ---- */
#define BSP_I2C_PORT        GPIOB
#define BSP_I2C_SCL_PIN     6
#define BSP_I2C_SDA_PIN     7
#define BSP_I2C_SPEED_HZ    400000u

#define BSP_SHT31_ADDR      0x44    /* SHT30/SHT31 (module GY-SHT3X), chân AD để trống */
#define BSP_SSD1306_ADDR    0x3C
#define BSP_DS3231_ADDR     0x68

/* ---- SPI1: Flash W25Q64 ---- */
#define BSP_SPI_PORT        GPIOA
#define BSP_SPI_SCK_PIN     5
#define BSP_SPI_MISO_PIN    6
#define BSP_SPI_MOSI_PIN    7
#define BSP_FLASH_CS_PORT   GPIOA
#define BSP_FLASH_CS_PIN    4

/* ---- ADC1: LDR ---- */
#define BSP_LDR_PORT        GPIOA
#define BSP_LDR_PIN         0
#define BSP_LDR_ADC_CH      0

/* ---- USART1: PC qua USB-TTL ---- */
#define BSP_UART_PORT       GPIOA
#define BSP_UART_TX_PIN     9
#define BSP_UART_RX_PIN     10
#define BSP_UART_BAUD       115200u

/* ---- GPIO báo hiệu & nút nhấn ---- */
#define BSP_BUZZER_PORT     GPIOB
#define BSP_BUZZER_PIN      1
/* Mức làm buzzer kêu. Module nhóm dùng in "Low level trigger" (低电平触发) -> 0.
 * Module kích mức cao (transistor NPN / còi nối thẳng) -> 1. */
#define BSP_BUZZER_ON_LEVEL 0
#define BSP_LED_PORT        GPIOC
#define BSP_LED_PIN         13      /* LED onboard, mức 0 = sáng */
/* 4 nút nối xuống GND, dùng điện trở kéo lên nội, ngắt EXTI cạnh xuống */
#define BSP_BTN_PORT        GPIOB
#define BSP_BTN_PAGE_PIN    0       /* EXTI0     : chuyển trang */
#define BSP_BTN_SET_PIN     10      /* EXTI15_10 : chọn / lưu */
#define BSP_BTN_PLUS_PIN    11      /* EXTI15_10 : tăng / cũ hơn */
#define BSP_BTN_MINUS_PIN   12      /* EXTI15_10 : giảm / mới hơn */

/* Priority cho mọi ngắt ngoại vi: phải >= configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY (5)
 * thì mới được gọi API ...FromISR() */
#define BSP_IRQ_PRIO        6

/* Clock 72 MHz từ HSE 8 MHz (rơi về HSI 64 MHz nếu thạch anh không chạy),
 * nhóm priority NVIC cho FreeRTOS. Gọi đầu tiên trong main(). */
void bsp_system_init(void);

#endif /* BSP_H */
