#include "sbi.h"
#include "trap.h"
#include "load_single_app.h"

void main() {
    sbi_puts("Hello from kernel!\n");
    trap_init();
    uint64_t entry = load_user();
    enter_user(entry, 0x80500000);
    while(1) {
        asm volatile("wfi");
    }
}
