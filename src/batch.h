#include <stdint.h>
struct trapframe;
extern uint8_t _user_elf_hello[];

uint64_t load_user(const uint8_t *image, uint64_t runtime_base);
void task_init_all(void);
void task_start(void);
void task_yield(struct trapframe *tf);
void task_exit(struct trapframe *tf);
extern void enter_user(uint64_t entry, uint64_t sp);
