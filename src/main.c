#include "sbi.h"
#include "trap.h"
#include "multi.h"
#include "vm.h"

void main() {
    sbi_puts("Hello from kernel!\n");
    vm_init();
    sbi_puts("vm_init done\n");
    task_init_all();
    sbi_puts("task_init done\n");
    trap_init();
    sbi_puts("trap_init done\n");
    task_start();
    while(1) {
        asm volatile("wfi");
    }
}
