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

void main() {
    sbi_puts("Hello from kernel!\n");
    sbi_puts("If you see this, kernel is running!\n");
    
    while(1) {
        asm volatile("wfi");
    }
}