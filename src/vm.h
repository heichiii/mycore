#pragma once
#include <stdint.h>

typedef uint64_t pte_t;
typedef pte_t *pagetable_t;

#define PTE_V (1ULL << 0)
#define PTE_R (1ULL << 1)
#define PTE_W (1ULL << 2)
#define PTE_X (1ULL << 3)
#define PTE_U (1ULL << 4)
#define PTE_A (1ULL << 6)
#define PTE_D (1ULL << 7)

void vm_init(void);
pagetable_t vm_create(void);
pagetable_t vm_clone_kernel(void);
int vm_map(pagetable_t root, uint64_t va, uint64_t pa,
           uint64_t size, uint64_t flags);
int vm_unmap(pagetable_t root, uint64_t va, uint64_t size);
void vm_activate(pagetable_t root);
void vm_sfence(void);
uint64_t vm_root_satp(pagetable_t root);
uint64_t vm_translate(pagetable_t root, uint64_t va);
uint64_t vm_get_pte(pagetable_t root, uint64_t va);
