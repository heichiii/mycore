#include "vm.h"
#include "page.h"

#define SATP_MODE_SV39 (8ULL << 60)
#define PTE_PER_PAGE 512
#define DRAM_END 0x88000000ULL
#define PAGE_ALLOC_START 0x86000000ULL
#define PAGE_ALLOC_END   0x87e00000ULL

extern uint8_t _kernel_start[];
extern uint8_t _kernel_end[];

static pagetable_t kernel_root;

/* Page-table pages live in the kernel image; user/data pages use page.c. */
static uint8_t page_table_pool[256 * 4096] __attribute__((aligned(4096)));
static uint64_t page_table_next;

static void *alloc_page_table(void)
{
    if (page_table_next >= sizeof(page_table_pool))
        return 0;
    void *page = page_table_pool + page_table_next;
    page_table_next += 4096;
    for (uint64_t i = 0; i < 4096; i++)
        ((uint8_t *)page)[i] = 0;
    return page;
}

static uint64_t pte_pa(pte_t pte)
{
    return (pte >> 10) << 12;
}

static pte_t pa_pte(uint64_t pa)
{
    return (pa >> 12) << 10;
}

static int leaf(pte_t pte)
{
    return (pte & (PTE_R | PTE_W | PTE_X)) != 0;
}

static pte_t *walk(pagetable_t root, uint64_t va, int create)
{
    pagetable_t table = root;
    for (int level = 2; level > 0; level--) {
        uint64_t index = (va >> (12 + 9 * level)) & 0x1ff;
        pte_t *entry = &table[index];
        if (!(*entry & PTE_V)) {
            if (!create)
                return 0;
            pagetable_t next = (pagetable_t)alloc_page_table();
            if (!next)
                return 0;
            *entry = pa_pte((uint64_t)(uintptr_t)next) | PTE_V;
        } else if (leaf(*entry)) {
            return 0;
        }
        table = (pagetable_t)(uintptr_t)pte_pa(*entry);
    }
    return &table[(va >> 12) & 0x1ff];
}

uint64_t vm_translate(pagetable_t root, uint64_t va)
{
    pte_t *entry = walk(root, va, 0);
    if (!entry || !(*entry & PTE_V) || !leaf(*entry))
        return 0;
    return pte_pa(*entry) | (va & 0xfff);
}

uint64_t vm_get_pte(pagetable_t root, uint64_t va)
{
    pte_t *entry = walk(root, va, 0);
    if (!entry || !(*entry & PTE_V))
        return 0;
    return *entry;
}

static int clone_table(pagetable_t dst, pagetable_t src, int level)
{
    for (int i = 0; i < PTE_PER_PAGE; i++) {
        if (!(src[i] & PTE_V))
            continue;
        if (level > 0 && !leaf(src[i])) {
            pagetable_t child = (pagetable_t)alloc_page_table();
            if (!child)
                return -1;
            dst[i] = pa_pte((uint64_t)(uintptr_t)child) | PTE_V;
            if (clone_table(child,
                            (pagetable_t)(uintptr_t)pte_pa(src[i]),
                            level - 1) < 0)
                return -1;
        } else {
            dst[i] = src[i];
        }
    }
    return 0;
}

int vm_map(pagetable_t root, uint64_t va, uint64_t pa,
           uint64_t size, uint64_t flags)
{
    if ((va & 0xfff) || (pa & 0xfff) || (size & 0xfff))
        return -1;
    for (uint64_t offset = 0; offset < size; offset += 4096) {
        pte_t *entry = walk(root, va + offset, 1);
        if (!entry || (*entry & PTE_V))
            return -1;
        *entry = pa_pte(pa + offset) | flags | PTE_V;
    }
    return 0;
}

void vm_init(void)
{
    page_table_next = 0;
    page_init();
    kernel_root = (pagetable_t)alloc_page_table();

    uint64_t kernel_start = (uint64_t)(uintptr_t)_kernel_start & ~0xfffULL;
    uint64_t kernel_end = ((uint64_t)(uintptr_t)_kernel_end + 0xfff) & ~0xfffULL;
    vm_map(kernel_root, kernel_start, kernel_start, kernel_end - kernel_start,
           PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);
    /* Map physical page allocator region */
    vm_map(kernel_root, PAGE_ALLOC_START, PAGE_ALLOC_START,
           PAGE_ALLOC_END - PAGE_ALLOC_START,
           PTE_R | PTE_W | PTE_A | PTE_D);
    vm_activate(kernel_root);
}

pagetable_t vm_create(void)
{
    return (pagetable_t)alloc_page_table();
}

pagetable_t vm_clone_kernel(void)
{
    pagetable_t root = vm_create();
    if (!root || clone_table(root, kernel_root, 2) < 0)
        return 0;
    return root;
}

uint64_t vm_root_satp(pagetable_t root)
{
    return SATP_MODE_SV39 | ((uint64_t)(uintptr_t)root >> 12);
}

void vm_activate(pagetable_t root)
{
    asm volatile("csrw satp, %0\n\tsfence.vma" :: "r"(vm_root_satp(root)) : "memory");
}

int vm_unmap(pagetable_t root, uint64_t va, uint64_t size)
{
    if ((va & 0xfff) || (size & 0xfff))
        return -1;
    for (uint64_t offset = 0; offset < size; offset += 4096) {
        pte_t *entry = walk(root, va + offset, 0);
        if (entry && (*entry & PTE_V))
            *entry = 0;
    }
    return 0;
}

void vm_sfence(void)
{
    asm volatile("sfence.vma" ::: "memory");
}

static void kmemcpy(void *dst, const void *src, uint64_t n)
{
    char *d = (char *)dst;
    const char *s = (const char *)src;
    for (uint64_t i = 0; i < n; i++) d[i] = s[i];
}

static int clone_deep(pagetable_t dst, pagetable_t src, int level)
{
    for (int i = 0; i < PTE_PER_PAGE; i++) {
        if (!(src[i] & PTE_V))
            continue;
        if (level > 0 && !leaf(src[i])) {
            pagetable_t child = (pagetable_t)alloc_page_table();
            if (!child)
                return -1;
            dst[i] = pa_pte((uint64_t)(uintptr_t)child) | PTE_V;
            if (clone_deep(child,
                           (pagetable_t)(uintptr_t)pte_pa(src[i]),
                           level - 1) < 0)
                return -1;
        } else {
            if (src[i] & PTE_U) {
                uint64_t src_pa = pte_pa(src[i]);
                void *new_page = alloc_page();
                if (!new_page)
                    return -1;
                kmemcpy(new_page, (void *)(uintptr_t)src_pa, PAGE_SIZE);
                dst[i] = pa_pte((uint64_t)(uintptr_t)new_page) |
                         (src[i] & 0x3FF);
            } else {
                dst[i] = src[i];
            }
        }
    }
    return 0;
}

pagetable_t vm_clone_deep(pagetable_t src)
{
    pagetable_t dst = vm_create();
    if (!dst)
        return 0;
    if (clone_deep(dst, src, 2) < 0)
        return 0;
    return dst;
}
