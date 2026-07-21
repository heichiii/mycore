#include "syscall.h"
#include "sbi.h"
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
