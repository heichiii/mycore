#include "page.h"

#define PAGE_ALLOC_START 0x86000000ULL
#define PAGE_ALLOC_END   0x87e00000ULL

static uint64_t next_page;

void page_init(void)
{
    next_page = PAGE_ALLOC_START;
}

void *alloc_page(void)
{
    uint64_t page = next_page;
    if (page >= PAGE_ALLOC_END)
        return 0;
    next_page += PAGE_SIZE;
    for (uint64_t i = 0; i < PAGE_SIZE; i++)
        ((uint8_t *)(uintptr_t)page)[i] = 0;
    return (void *)(uintptr_t)page;
}
