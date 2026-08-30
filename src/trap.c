#include "trap.h"
#include "sbi.h"
#include "syscall.h"
#include "multi.h"
#include "page.h"

void trap_entry();
extern uint8_t _stack_top[];

#define SIE_STIE (1ULL << 5)
#define SSTATUS_SIE (1ULL << 1)
#define TIMER_INTERVAL 100000ULL

static uint64_t read_time(void)
{
    uint64_t value;
    asm volatile("rdtime %0" : "=r"(value));
    return value;
}

static void timer_init(void)
{
    asm volatile("csrs sie, %0" :: "r"(SIE_STIE));
    asm volatile("csrs sstatus, %0" :: "r"(SSTATUS_SIE));
    sbi_set_timer(read_time() + TIMER_INTERVAL);
}

void print_hex(uint64_t value)
{
    static const char digits[] = "0123456789abcdef";
    for (int shift = 60; shift >= 0; shift -= 4)
        sbi_putchar(digits[(value >> shift) & 0xf]);
}

static void page_fault_handler(struct trapframe *tf)
{
    uint64_t va = tf->stval;
    pagetable_t root = current_pagetable();

    /* Only handle faults in the user address space (sv39: bit 38 == 0). */
    if (va >= (1ULL << 38)) {
        sbi_puts("[trap] kernel page fault va=0x");
        print_hex(va);
        sbi_puts("\n");
        sbi_shutdown();
        return;
    }
    /* Already mapped (e.g. a protection fault) - the task must go. */
    if (vm_get_pte(root, va) & PTE_V) {
        sbi_puts("[trap] protection fault va=0x");
        print_hex(va);
        sbi_puts("\n");
        task_exit(tf);
        return;
    }

    void *pa = alloc_page();
    if (!pa) {
        sbi_puts("[trap] out of memory on page fault\n");
        sbi_shutdown();
        return;
    }
    if (vm_map(root, va & ~(PAGE_SIZE - 1), (uint64_t)(uintptr_t)pa,
               PAGE_SIZE, PTE_U | PTE_R | PTE_W | PTE_A | PTE_D) < 0) {
        sbi_puts("[trap] map failed on page fault\n");
        sbi_shutdown();
        return;
    }
    vm_sfence();
}

void trap_init()
{
    asm volatile("la t0, _stack_top\n\tcsrw sscratch, t0" ::: "t0");
    asm volatile(
        "csrw stvec, %0" 
        : 
        : "r"((uint64_t)trap_entry)
    );
    timer_init();
}
void trap_handler(struct trapframe *tf) {
    uint64_t scause = tf->scause;
    uint64_t cause = scause & ~(1ULL << 63);
    int is_interrupt = (scause >> 63) & 1;

    if (is_interrupt) {
        switch (cause) {
        case 5: /* supervisor timer interrupt */
            sbi_set_timer(read_time() + TIMER_INTERVAL);
            task_schedule(tf);
            break;
        default:
            sbi_puts("[trap] unknown interrupt\n");
            sbi_shutdown();
            break;
        }
        return;
    }

    switch (cause) {
    case 8: /* environment call from U-mode */
        tf->sepc += 4;
        switch (tf->a7) {
        case SYS_IOCTL:
            tf->a0 = 0;
            break;
        case SYS_WRITE:
            sys_write(tf);
            break;
        case SYS_READ:
            sys_read(tf);
            break;
        case SYS_sched_yield:
            sys_yield(tf);
            break;
        case SYS_WRITEV: {
            struct iovec { void *base; uint64_t len; };
            struct iovec *iov = (struct iovec *)tf->a1;
            int iovcnt = (int)tf->a2;
            uint64_t total = 0;
            for (int i = 0; i < iovcnt; i++) {
                char *buf = (char *)iov[i].base;
                for (uint64_t j = 0; j < iov[i].len; j++)
                    sbi_putchar(buf[j]);
                total += iov[i].len;
            }
            tf->a0 = total;
            break;
        }
        case SYS_EXIT:
        case SYS_exit_group:
            sys_exit(tf);
            break;
        case SYS_openat:
            tf->a0 = -ENOENT;
            break;
        case SYS_close:
            tf->a0 = 0;
            break;
        case SYS_lseek:
            tf->a0 = -ESPIPE;
            break;
        case SYS_ppoll:
            tf->a0 = 0;
            break;
        case SYS_readlinkat:
        case SYS_fstatat:
            tf->a0 = -ENOENT;
            break;
        case SYS_set_tid_address:
            tf->a0 = 0;
            break;
        case SYS_futex:
            tf->a0 = 0;
            break;
        case SYS_clone:
            /* fork/clone is not implemented by this scheduler yet. */
            tf->a0 = -ENOSYS;
            break;
        case SYS_rt_tgsigqueueinfo:
            /* Signals are not implemented yet; report the standard error. */
            tf->a0 = -ENOSYS;
            break;
        case SYS_mmap:
            sys_mmap(tf);
            break;
        case SYS_munmap:
            sys_munmap(tf);
            break;
        case SYS_sbrk:
            sys_sbrk(tf);
            break;
        case SYS_trace:
            sys_trace(tf);
            break;
        case SYS_getpid:
            tf->a0 = 1;
            break;
        default:
            sbi_puts("[trap] unknown syscall (a7=");
            sbi_puts("0x");
            print_hex(tf->a7);
            sbi_puts(")\n");
            tf->a0 = -ENOSYS;
            break;
        }
        break;
    case 12: /* instruction page fault */
        sbi_puts("[trap] instruction page fault sepc=0x");
        print_hex(tf->sepc);
        sbi_puts(" stval=0x");
        print_hex(tf->stval);
        sbi_puts("\n");
        sbi_shutdown();
        break;
    case 13: /* load page fault */
    case 15: /* store/amo page fault */
        page_fault_handler(tf);
        break;
    default:
        sbi_puts("[trap] unhandled exception (cause=");
        sbi_putchar('0' + (cause / 10));
        sbi_putchar('0' + (cause % 10));
        sbi_puts(")\n");
        sbi_shutdown();
        break;
    }
}
