#include "syscall.h"
#include "sbi.h"
#include "multi.h"
#include "page.h"
#include "vm.h"
void sys_write(struct trapframe *tf) {
    int fd = (int)tf->a0;
    const char *buf = (const char *)tf->a1;
    uint64_t count = tf->a2;

    if (fd == 1 || fd == 2) {
        for (uint64_t i = 0; i < count; i++) {
            sbi_putchar(buf[i]);
        }
        tf->a0 = count;
    } else {
        tf->a0 = -1;
    }
}

void sys_read(struct trapframe *tf) {
    int fd = (int)tf->a0;
    char *buf = (char *)tf->a1;
    uint64_t count = tf->a2;

    if (fd != 0) {
        tf->a0 = -1;
        return;
    }

    uint64_t n = 0;
    while (n < count) {
        long c = sbi_getchar();
        if (c < 0)
            continue;
        buf[n++] = (char)c;
        if (c == '\n')
            break;
    }
    tf->a0 = n;
}

void sys_exit(struct trapframe *tf) {
    task_exit(tf);
}

void sys_yield(struct trapframe *tf) {
    task_yield(tf);
}

long sys_mmap(struct trapframe *tf)
{
    uint64_t start = tf->a0;
    uint64_t len = tf->a1;
    int prot = (int)tf->a2;
    pagetable_t root = current_pagetable();
    uint64_t npages;
    uint64_t flags;
    uint64_t i;

    if (start & (PAGE_SIZE - 1))
        goto fail;
    if (len == 0)
        goto fail;
    if (start + len < start)
        goto fail;
    if (prot == 0 || (prot & ~(PROT_READ | PROT_WRITE | PROT_EXEC)))
        goto fail;

    npages = (len + PAGE_SIZE - 1) / PAGE_SIZE;
    flags = PTE_U | PTE_A | PTE_D;
    if (prot & PROT_READ) flags |= PTE_R;
    if (prot & PROT_WRITE) flags |= PTE_W;
    if (prot & PROT_EXEC) flags |= PTE_X;

    for (i = 0; i < npages; i++)
        if (vm_get_pte(root, start + i * PAGE_SIZE) & PTE_V)
            goto fail;

    for (i = 0; i < npages; i++) {
        uint64_t va = start + i * PAGE_SIZE;
        void *pa = alloc_page();
        if (!pa || vm_map(root, va, (uint64_t)(uintptr_t)pa,
                           PAGE_SIZE, flags) < 0) {
            tf->a0 = -ENOMEM;
            return -ENOMEM;
        }
    }
    vm_sfence();
    tf->a0 = 0;
    return 0;

fail:
    tf->a0 = -1;
    return -1;
}

long sys_munmap(struct trapframe *tf)
{
    uint64_t start = tf->a0;
    uint64_t len = tf->a1;
    pagetable_t root = current_pagetable();
    uint64_t npages;
    uint64_t i;

    if (start & (PAGE_SIZE - 1))
        goto fail;
    if (len == 0)
        goto fail;
    if (start + len < start)
        goto fail;

    npages = (len + PAGE_SIZE - 1) / PAGE_SIZE;
    for (i = 0; i < npages; i++)
        if (!(vm_get_pte(root, start + i * PAGE_SIZE) & PTE_V))
            goto fail;

    for (i = 0; i < npages; i++)
        vm_unmap(root, start + i * PAGE_SIZE, PAGE_SIZE);
    vm_sfence();
    tf->a0 = 0;
    return 0;

fail:
    tf->a0 = -1;
    return -1;
}

long sys_trace(struct trapframe *tf)
{
    int req = (int)tf->a0;
    uint64_t addr = tf->a1;
    uint8_t data = (uint8_t)tf->a2;
    pagetable_t root = current_pagetable();
    pte_t pte = vm_get_pte(root, addr);

    if (!(pte & PTE_V) || !(pte & PTE_U))
        goto fail;
    if (addr >= (1ULL << 38))
        goto fail;

    switch (req) {
    case TRACE_READ:
        tf->a0 = *(uint8_t *)(uintptr_t)vm_translate(root, addr);
        return tf->a0;
    case TRACE_WRITE:
        if (!(pte & PTE_W))
            goto fail;
        *(uint8_t *)(uintptr_t)vm_translate(root, addr) = data;
        tf->a0 = 0;
        return 0;
    default:
        break;
    }

fail:
    tf->a0 = -1;
    return -1;
}
