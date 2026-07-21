#include <stdint.h>
uint64_t load_user(void);
extern void enter_user(uint64_t entry, uint64_t sp);