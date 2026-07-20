#include <stdint.h>

#define SBI_LEGACY_PUTCHAR 0x01

#define SYS_WRITE  64
#define SYS_EXIT   93

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

#define SBI_EXT_SRST 0x53525354
#define SBI_EXT_SRST_RESET 0x0
#define SBI_SRST_RESET_TYPE_SHUTDOWN 0x0
#define SBI_SRST_RESET_REASON_NONE 0x0

static inline void sbi_shutdown(void) {
    register unsigned long a0 asm("a0") = SBI_SRST_RESET_TYPE_SHUTDOWN;
    register unsigned long a1 asm("a1") = SBI_SRST_RESET_REASON_NONE;
    register unsigned long a6 asm("a6") = SBI_EXT_SRST_RESET;
    register unsigned long a7 asm("a7") = SBI_EXT_SRST;
    
    asm volatile (
        "ecall"
        : 
        : "r" (a0), "r" (a1), "r" (a6), "r" (a7)
        : "memory"
    );
}

struct trapframe {
    uint64_t ra;        // x1,   offset 0
    uint64_t sp;        // x2,   offset 8
    uint64_t gp;        // x3,   offset 16
    uint64_t tp;        // x4,   offset 24
    uint64_t t0;        // x5,   offset 32
    uint64_t t1;        // x6,   offset 40
    uint64_t t2;        // x7,   offset 48
    uint64_t s0;        // x8,   offset 56
    uint64_t s1;        // x9,   offset 64
    uint64_t a0;        // x10,  offset 72
    uint64_t a1;        // x11,  offset 80
    uint64_t a2;        // x12,  offset 88
    uint64_t a3;        // x13,  offset 96
    uint64_t a4;        // x14,  offset 104
    uint64_t a5;        // x15,  offset 112
    uint64_t a6;        // x16,  offset 120
    uint64_t a7;        // x17,  offset 128
    uint64_t s2;        // x18,  offset 136
    uint64_t s3;        // x19,  offset 144
    uint64_t s4;        // x20,  offset 152
    uint64_t s5;        // x21,  offset 160
    uint64_t s6;        // x22,  offset 168
    uint64_t s7;        // x23,  offset 176
    uint64_t s8;        // x24,  offset 184
    uint64_t s9;        // x25,  offset 192
    uint64_t s10;       // x26,  offset 200
    uint64_t s11;       // x27,  offset 208
    uint64_t t3;        // x28,  offset 216
    uint64_t t4;        // x29,  offset 224
    uint64_t t5;        // x30,  offset 232
    uint64_t t6;        // x31,  offset 240
    uint64_t sepc;      // offset 248
    uint64_t sstatus;   // offset 256
    uint64_t scause;    // offset 264
    uint64_t stval;     // offset 272
};

extern void trap_entry(void);

void trap_init(void) {
    asm volatile("csrw stvec, %0" : : "r"((uint64_t)trap_entry));
    // asm volatile("csrw sie, %0" : : "r"(0x20ULL));    // enable supervisor timer interrupt
    // asm volatile("csrsi sstatus, 0x2");                // set SIE in sstatus
}

void sys_write(struct trapframe *tf) {
    int fd = (int)tf->a0;
    const char *buf = (const char *)tf->a1;
    uint64_t count = tf->a2;

    if (fd == 1 || fd == 2) {
        for (uint64_t i = 0; i < count; i++) {
            sbi_putchar(buf[i]);
        }
        tf->a0 = count;
    } else {
        tf->a0 = -1;
    }
}

void sys_exit(struct trapframe *tf) {
    int status = (int)tf->a0;
    sbi_puts("[kernel] user program exited with status: ");
    if (status == 0) {
        sbi_puts("0\n");
    } else {
        sbi_putchar('0' + (status / 10));
        sbi_putchar('0' + (status % 10));
        sbi_putchar('\n');
    }
    sbi_shutdown();
}

void trap_handler(struct trapframe *tf) {
    uint64_t scause = tf->scause;
    uint64_t cause = scause & ~(1ULL << 63);
    int is_interrupt = (scause >> 63) & 1;

    if (is_interrupt) {
        switch (cause) {
        case 5: /* supervisor timer interrupt */
            sbi_puts("[trap] timer interrupt\n");
            break;
        default:
            sbi_puts("[trap] unknown interrupt\n");
            sbi_shutdown();
            break;
        }
        return;
    }

    switch (cause) {
    case 8: /* environment call from U-mode */
        switch (tf->a7) {
        case SYS_WRITE:
            sys_write(tf);
            break;
        case SYS_EXIT:
            sys_exit(tf);
            break;
        default:
            sbi_puts("[trap] unknown syscall (a7=");
            sbi_putchar('0' + (tf->a7 / 10));
            sbi_putchar('0' + (tf->a7 % 10));
            sbi_puts(")\n");
            break;
        }
        tf->sepc += 4;
        break;
    case 12: /* instruction page fault */
        sbi_puts("[trap] instruction page fault\n");
        sbi_shutdown();
        break;
    case 13: /* load page fault */
        sbi_puts("[trap] load page fault\n");
        sbi_shutdown();
        break;
    case 15: /* store/amo page fault */
        sbi_puts("[trap] store page fault\n");
        sbi_shutdown();
        break;
    default:
        sbi_puts("[trap] unhandled exception (cause=");
        sbi_putchar('0' + (cause / 10));
        sbi_putchar('0' + (cause % 10));
        sbi_puts(")\n");
        sbi_shutdown();
        break;
    }
}

extern uint8_t _user_elf_start[];
extern void enter_user(uint64_t entry, uint64_t sp);

static void memcpy(void *dst, const void *src, uint64_t n) {
    char *d = (char *)dst;
    const char *s = (const char *)src;
    for (uint64_t i = 0; i < n; i++) d[i] = s[i];
}

static void load_user(void) {
    memcpy((void *)0x80400000, _user_elf_start + 0x1000, 0x1b38);
}

void main() {
    sbi_puts("Hello from kernel!\n");
    trap_init();
    load_user();
    enter_user(0x80400000, 0x80500000);
    while(1) {
        asm volatile("wfi");
    }
}