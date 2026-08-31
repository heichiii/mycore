#include <stdint.h>
#include "vm.h"
struct trapframe;

uint64_t load_user(pagetable_t root, const uint8_t *image,
                   uint64_t runtime_base);
void task_init_all(void);
void task_start(void);
void task_yield(struct trapframe *tf);
void task_schedule(struct trapframe *tf);
void task_exit(struct trapframe *tf);
void task_fork(struct trapframe *tf);
void task_exec(struct trapframe *tf);
void task_wait(struct trapframe *tf);
int task_getpid(void);
int task_getppid(void);
pagetable_t current_pagetable(void);
long sys_mmap(struct trapframe *tf);
long sys_munmap(struct trapframe *tf);
long sys_sbrk(struct trapframe *tf);
extern void enter_user(uint64_t entry, uint64_t sp);
