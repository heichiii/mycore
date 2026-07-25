#include "syscall.h"
#include "sbi.h"
#include "multi.h"
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
    task_exit(tf);
}

void sys_yield(struct trapframe *tf) {
    task_yield(tf);
}
