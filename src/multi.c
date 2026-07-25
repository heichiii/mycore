#include "multi.h"
#include "sbi.h"
#include "trap.h"
#include <stdint.h>

extern uint8_t _user_elf_hello[];
extern uint8_t _user_elf_yield0[];
extern uint8_t _user_elf_yield1[];
extern uint8_t _user_elf_yield2[];

#define MAX_TASKS 4
#define TASK_SLOT_SIZE 0x00400000ULL
#define TASK_STACK_SIZE 0x00010000ULL

#define TASK0_BASE  0x80400000ULL
#define TASK1_BASE  0x804c0000ULL
#define TASK2_BASE  0x804e0000ULL
#define TASK3_BASE  0x80500000ULL

#define TASK0_STACK 0x80700000ULL
#define TASK1_STACK 0x80b00000ULL
#define TASK2_STACK 0x80f00000ULL
#define TASK3_STACK 0x81300000ULL

#define PT_LOAD    1
#define PT_DYNAMIC 2
#define PF_X       1
#define ET_EXEC    2
#define ET_DYN     3
#define EI_NIDENT  16

#define DT_NULL    0
#define DT_RELA    7
#define DT_RELASZ  8
#define DT_RELAENT 9

#define R_RISCV_RELATIVE 3
#define SSTATUS_SPIE (1ULL << 5)
#define SSTATUS_SPP  (1ULL << 8)

typedef struct {
    unsigned char e_ident[EI_NIDENT];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} Elf64_Ehdr;

typedef struct {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} Elf64_Phdr;

typedef struct {
    int64_t d_tag;
    uint64_t d_val;
} Elf64_Dyn;

typedef struct {
    uint64_t r_offset;
    uint64_t r_info;
    int64_t r_addend;
} Elf64_Rela;

enum task_state {
    TASK_UNUSED,
    TASK_READY,
    TASK_RUNNING,
    TASK_EXITED,
};

struct task {
    int id;
    enum task_state state;
    struct trapframe context;
};

struct program {
    const uint8_t *image;
    uint64_t runtime_base;
    uint64_t stack_top;
};

static const struct program programs[MAX_TASKS] = {
    {_user_elf_yield0, TASK1_BASE, TASK0_STACK},
    {_user_elf_yield1, TASK2_BASE, TASK1_STACK},
    {_user_elf_yield2, TASK3_BASE, TASK2_STACK},
    {_user_elf_hello,  TASK0_BASE, TASK3_STACK},
};

static struct task tasks[MAX_TASKS];
static int current_task;


static void memcpy(void *dst, const void *src, uint64_t n)
{
    char *d = (char *)dst;
    const char *s = (const char *)src;
    for (uint64_t i = 0; i < n; i++) d[i] = s[i];
}

static void memset(void *dst, int c, uint64_t n)
{
    char *d = (char *)dst;
    for (uint64_t i = 0; i < n; i++) d[i] = (char)c;
}

static uint64_t elf_file_offset(Elf64_Phdr *phdrs, uint16_t phnum,
                                uint64_t vaddr)
{
    for (uint16_t i = 0; i < phnum; i++) {
        if (phdrs[i].p_type != PT_LOAD)
            continue;
        if (vaddr >= phdrs[i].p_vaddr &&
            vaddr - phdrs[i].p_vaddr < phdrs[i].p_filesz)
            return vaddr - phdrs[i].p_vaddr + phdrs[i].p_offset;
    }
    return 0;
}

static uint64_t preferred_base(Elf64_Phdr *phdrs, uint16_t phnum)
{
    uint64_t base = UINT64_MAX;

    for (uint16_t i = 0; i < phnum; i++) {
        if (phdrs[i].p_type == PT_LOAD &&
            (phdrs[i].p_flags & PF_X) && phdrs[i].p_vaddr < base)
            base = phdrs[i].p_vaddr;
    }
    return base;
}

uint64_t load_user(const uint8_t *image, uint64_t runtime_base)
{
    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)image;
    if (ehdr->e_ident[0] != 0x7f || ehdr->e_ident[1] != 'E' ||
        ehdr->e_ident[2] != 'L' || ehdr->e_ident[3] != 'F')
        return 0;
    if (ehdr->e_type != ET_EXEC && ehdr->e_type != ET_DYN)
        return 0;

    Elf64_Phdr *phdr = (Elf64_Phdr *)(image + ehdr->e_phoff);
    uint64_t base = preferred_base(phdr, ehdr->e_phnum);
    if (base == UINT64_MAX || runtime_base < base)
        return 0;
    uint64_t bias = runtime_base - base;

    for (uint16_t i = 0; i < ehdr->e_phnum; i++) {
        if (phdr[i].p_type != PT_LOAD)
            continue;
        if (phdr[i].p_filesz > phdr[i].p_memsz)
            return 0;
        if (phdr[i].p_vaddr + phdr[i].p_memsz < phdr[i].p_vaddr)
            return 0;
        if (phdr[i].p_vaddr == 0 && phdr[i].p_filesz > 0)
            continue;

        uint64_t dst_addr = phdr[i].p_vaddr + bias;
        uint8_t *dst = (uint8_t *)(uintptr_t)dst_addr;
        memcpy(dst, image + phdr[i].p_offset, phdr[i].p_filesz);
        if (phdr[i].p_memsz > phdr[i].p_filesz)
            memset(dst + phdr[i].p_filesz, 0,
                   phdr[i].p_memsz - phdr[i].p_filesz);
    }

    uint64_t rela_addr = 0, rela_size = 0, rela_ent = 0;
    for (uint16_t i = 0; i < ehdr->e_phnum; i++) {
        if (phdr[i].p_type != PT_DYNAMIC)
            continue;
        uint64_t dyn_off = elf_file_offset(phdr, ehdr->e_phnum,
                                           phdr[i].p_vaddr);
        Elf64_Dyn *dyn = (Elf64_Dyn *)(image + dyn_off);
        uint64_t count = phdr[i].p_filesz / sizeof(Elf64_Dyn);
        for (uint64_t j = 0; j < count; j++) {
            if (dyn[j].d_tag == DT_NULL)
                break;
            if (dyn[j].d_tag == DT_RELA) rela_addr = dyn[j].d_val;
            if (dyn[j].d_tag == DT_RELASZ) rela_size = dyn[j].d_val;
            if (dyn[j].d_tag == DT_RELAENT) rela_ent = dyn[j].d_val;
        }
        break;
    }

    if (rela_addr && rela_size && rela_ent >= sizeof(Elf64_Rela)) {
        uint64_t rela_off = elf_file_offset(phdr, ehdr->e_phnum, rela_addr);
        Elf64_Rela *rela = (Elf64_Rela *)(image + rela_off);
        uint64_t count = rela_size / rela_ent;
        for (uint64_t i = 0; i < count; i++) {
            uint32_t type = (uint32_t)rela[i].r_info;
            if (type == R_RISCV_RELATIVE) {
                uint64_t *ptr = (uint64_t *)(uintptr_t)
                    (rela[i].r_offset + bias);
                *ptr = (uint64_t)(rela[i].r_addend + (int64_t)bias);
            }
        }

        /* The static-PIE startup code would otherwise process the same
         * relocations again using the file-relative DT_RELA address. */
        for (uint16_t i = 0; i < ehdr->e_phnum; i++) {
            if (phdr[i].p_type != PT_DYNAMIC)
                continue;
            Elf64_Dyn *load_dyn = (Elf64_Dyn *)(uintptr_t)
                (phdr[i].p_vaddr + bias);
            uint64_t count_dyn = phdr[i].p_filesz / sizeof(Elf64_Dyn);
            for (uint64_t j = 0; j < count_dyn; j++) {
                if (load_dyn[j].d_tag == DT_NULL)
                    break;
                if (load_dyn[j].d_tag == DT_RELA ||
                    load_dyn[j].d_tag == DT_RELASZ)
                    load_dyn[j].d_val = 0;
            }
            break;
        }
    }

    return ehdr->e_entry + bias;
}

static uint64_t user_sstatus(void)
{
    uint64_t value;
    asm volatile("csrr %0, sstatus" : "=r"(value));
    value |= SSTATUS_SPIE;
    value &= ~SSTATUS_SPP;
    return value;
}

void task_init_all(void)
{
    for (int i = 0; i < MAX_TASKS; i++) {
        uint64_t entry = load_user(programs[i].image,
                                   programs[i].runtime_base);
        tasks[i].id = i;
        tasks[i].state = entry ? TASK_READY : TASK_UNUSED;
        tasks[i].context.sepc = entry;
        tasks[i].context.sp = programs[i].stack_top;
        tasks[i].context.sstatus = user_sstatus();
    }
    current_task = 0;
    tasks[0].state = TASK_RUNNING;
}

void task_start(void)
{
    enter_user(tasks[0].context.sepc, tasks[0].context.sp);
}

static int next_ready(void)
{
    for (int offset = 1; offset <= MAX_TASKS; offset++) {
        int index = (current_task + offset) % MAX_TASKS;
        if (tasks[index].state == TASK_READY)
            return index;
    }
    return -1;
}

void task_schedule(struct trapframe *tf)
{
    int next;
    tasks[current_task].context = *tf;
    tasks[current_task].state = TASK_READY;

    next = next_ready();
    if (next < 0) {
        tasks[current_task].state = TASK_RUNNING;
        return;
    }
    current_task = next;
    tasks[current_task].state = TASK_RUNNING;
    *tf = tasks[current_task].context;
}

void task_yield(struct trapframe *tf)
{
    tf->sepc += 4;
    tf->a0 = 0;
    task_schedule(tf);
}

void task_exit(struct trapframe *tf)
{
    int next;
    tasks[current_task].state = TASK_EXITED;
    next = next_ready();
    if (next < 0) {
        sbi_puts("[kernel] all tasks exited\n");
        sbi_shutdown();
        return;
    }
    current_task = next;
    tasks[current_task].state = TASK_RUNNING;
    *tf = tasks[current_task].context;
}
