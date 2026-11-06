/*
 * _sbrk cho newlib-nano: snprintf có thể xin vài byte malloc.
 * Vùng heap C nằm giữa `end` (cuối .bss) và đáy stack MSP (dành cho ngắt).
 * Task FreeRTOS KHÔNG dùng heap này mà dùng heap_4 (configTOTAL_HEAP_SIZE).
 */
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

extern char end;               /* linker script */
extern char _estack;
extern char _Min_Stack_Size;   /* là ký hiệu, giá trị nằm ở địa chỉ */

void *_sbrk(ptrdiff_t incr)
{
    static char *brk = &end;
    char *limit = &_estack - (size_t)&_Min_Stack_Size;

    if (brk + incr > limit) {
        errno = ENOMEM;
        return (void *)-1;
    }
    char *prev = brk;
    brk += incr;
    return prev;
}
