
#include <stdint.h>

struct sbiret {
    long error;
    long value;
};
void sbi_puts(const char *str);
void sbi_putchar(char c);
void sbi_shutdown();
void sbi_set_timer(uint64_t time);

