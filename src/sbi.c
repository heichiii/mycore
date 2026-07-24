#include "sbi.h"
#include <stdint.h>
#include <stddef.h>

#define SBI_LEGACY_PUTCHAR    0x01
#define SBI_EXT_SRST          0x53525354
#define SBI_EXT_TIME          0x54494d45
#define SBI_TIME_SET_TIMER    0
#define SBI_SRST_RESET        0x0
#define SBI_SRST_RESET_TYPE_SHUTDOWN 0x0
#define SBI_SRST_RESET_REASON_NONE   0x0

struct sbiret sbi_ecall(int ext, int fid, unsigned long arg0,
                        unsigned long arg1, unsigned long arg2,
                        unsigned long arg3, unsigned long arg4,
                        unsigned long arg5) {
    struct sbiret ret;
    register unsigned long a0 asm ("a0") = arg0;
    register unsigned long a1 asm ("a1") = arg1;
    register unsigned long a2 asm ("a2") = arg2;
    register unsigned long a3 asm ("a3") = arg3;
    register unsigned long a4 asm ("a4") = arg4;
    register unsigned long a5 asm ("a5") = arg5;
    register unsigned long a6 asm ("a6") = fid;
    register unsigned long a7 asm ("a7") = ext;

    asm volatile ("ecall"
                  : "+r" (a0), "+r" (a1)
                  : "r" (a2), "r" (a3), "r" (a4), "r" (a5), "r" (a6), "r" (a7)
                  : "memory");
    ret.error = a0;
    ret.value = a1;
    return ret;
}

void sbi_putchar(char c)
{
    sbi_ecall(SBI_LEGACY_PUTCHAR, 0, (unsigned long)c, 0, 0, 0, 0, 0);
}

void sbi_puts(const char *str)
{
    while (*str) {
        sbi_putchar(*str++);
    }
}

void sbi_shutdown()
{
    sbi_ecall(SBI_EXT_SRST, SBI_SRST_RESET,
              SBI_SRST_RESET_TYPE_SHUTDOWN, SBI_SRST_RESET_REASON_NONE,
              0, 0, 0, 0);
}

void sbi_set_timer(uint64_t time)
{
    sbi_ecall(SBI_EXT_TIME, SBI_TIME_SET_TIMER,
              time, 0, 0, 0, 0, 0);
}
