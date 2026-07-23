#include <stdint.h>
struct trapframe;
extern uint8_t _user_elf_hello[];

uint64_t load_user(const uint8_t *image);
int batch_next(struct trapframe *tf);
extern void enter_user(uint64_t entry, uint64_t sp);
