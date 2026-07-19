#include <stdint.h>

// Legacy SBI console
#define SBI_LEGACY_PUTCHAR 0x01

void sbi_putchar(char c) {
    register uint64_t a0 asm("a0") = (uint64_t)c;
    register uint64_t a7 asm("a7") = SBI_LEGACY_PUTCHAR;
    
    asm volatile(
        "ecall"
        : "+r"(a0)
        : "r"(a7)
        : "memory"
    );
}

void sbi_puts(const char *str) {
    while (*str) {
        sbi_putchar(*str++);
    }
}

// 定义 SBI SRST (System Reset) 扩展的宏
#define SBI_EXT_SRST 0x53525354
#define SBI_EXT_SRST_RESET 0x0
#define SBI_SRST_RESET_TYPE_SHUTDOWN 0x0
#define SBI_SRST_RESET_REASON_NONE 0x0

static inline void sbi_shutdown(void) {
    // 使用 __attribute__((used)) 防止优化
    register unsigned long a0 asm("a0") = SBI_SRST_RESET_TYPE_SHUTDOWN;
    register unsigned long a1 asm("a1") = SBI_SRST_RESET_REASON_NONE;
    register unsigned long a6 asm("a6") = SBI_EXT_SRST_RESET;
    register unsigned long a7 asm("a7") = SBI_EXT_SRST;
    
    // 使用 volatile 并显式列出所有被修改的寄存器
    asm volatile (
        "ecall"
        : 
        : "r" (a0), "r" (a1), "r" (a6), "r" (a7)
        : "memory"
    );
}
void main() {
    sbi_puts("Hello from kernel!\n");
    sbi_puts("If you see this, kernel is running!\n");
    sbi_shutdown();
    
    // 如果关机失败，进入死循环
    while(1) {
        asm volatile("wfi");
    }
}