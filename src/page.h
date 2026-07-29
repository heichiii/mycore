#pragma once
#include <stdint.h>

#define PAGE_SIZE 4096ULL

void page_init(void);
void *alloc_page(void);
