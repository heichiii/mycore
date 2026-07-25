#include "sbi.h"
#include "trap.h"
#include "multi.h"

void main() {
    sbi_puts("Hello from kernel!\n");
    trap_init();
    task_init_all();
    task_start();
    while(1) {
        asm volatile("wfi");
    }
}
