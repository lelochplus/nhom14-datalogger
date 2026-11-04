/*
 * FreeRTOSConfig.h - cấu hình kernel chung của nhóm.
 * CHỈ Leader được sửa file này (quy chuẩn mục 5.1).
 * Thông số task (priority/stack/queue) nằm ở app/inc/app_cfg.h.
 */
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include <stdint.h>
extern uint32_t SystemCoreClock;

#define configUSE_PREEMPTION                    1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 1
#define configCPU_CLOCK_HZ                      (SystemCoreClock)
#define configTICK_RATE_HZ                      ((TickType_t)1000)
#define configMAX_PRIORITIES                    6
#define configMINIMAL_STACK_SIZE                ((uint16_t)128)   /* words */
#define configTOTAL_HEAP_SIZE                   ((size_t)(10 * 1024))
#define configMAX_TASK_NAME_LEN                 12
#define configUSE_16_BIT_TICKS                  0
#define configIDLE_SHOULD_YIELD                 1
#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             0
#define configUSE_COUNTING_SEMAPHORES           0
#define configQUEUE_REGISTRY_SIZE               0
#define configUSE_TASK_NOTIFICATIONS            1
#define configSUPPORT_DYNAMIC_ALLOCATION        1
#define configSUPPORT_STATIC_ALLOCATION         0

/* Hook dùng để kiểm tra khi tích hợp (Giai đoạn 3) */
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0
#define configCHECK_FOR_STACK_OVERFLOW          2
#define configUSE_MALLOC_FAILED_HOOK            1

/* Software timer */
#define configUSE_TIMERS                        0   /* không dùng software timer: tiết kiệm 1 task + stack */
#define configTIMER_TASK_PRIORITY               (configMAX_PRIORITIES - 1)
#define configTIMER_QUEUE_LENGTH                5
#define configTIMER_TASK_STACK_DEPTH            configMINIMAL_STACK_SIZE

/* API được dùng */
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_xTaskDelayUntil                 1
#define INCLUDE_vTaskDelete                     0
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_uxTaskGetStackHighWaterMark     1
#define INCLUDE_xTaskGetHandle                  1
#define INCLUDE_xTaskGetSchedulerState          1

/* Cortex-M3 STM32F1: 4 bit priority */
#define configPRIO_BITS                         4
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY      15
/* ISR gọi API ...FromISR() phải có priority số >= 5 (xem bsp.h: BSP_IRQ_PRIO) */
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5
#define configKERNEL_INTERRUPT_PRIORITY \
    (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))
#define configMAX_SYSCALL_INTERRUPT_PRIORITY \
    (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))

/* Lỗi nghiêm trọng: dừng hệ thống, gắn debugger sẽ thấy dòng lỗi */
#define configASSERT(x) do { if (!(x)) { __asm volatile("cpsid i"); for (;;) {} } } while (0)

/* Gắn handler FreeRTOS vào bảng vector của startup file */
#define vPortSVCHandler     SVC_Handler
#define xPortPendSVHandler  PendSV_Handler
#define xPortSysTickHandler SysTick_Handler

#endif /* FREERTOS_CONFIG_H */
