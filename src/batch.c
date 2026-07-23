#include "batch.h"
#include "sbi.h"
#include "trap.h"
#include <stdint.h>

extern uint8_t _user_elf_hello[];
extern uint8_t _user_elf_exit[];
extern uint8_t _user_elf_power[];

#define USER_STACK 0x80500000
#define APP_COUNT 3

static const uint8_t *const apps[APP_COUNT] = {
    _user_elf_hello,
    _user_elf_exit,
    _user_elf_power,
};
static unsigned int current_app;

#define PT_NULL    0
#define PT_LOAD    1
#define PT_DYNAMIC 2
#define EI_NIDENT  16

#define DT_NULL    0
#define DT_RELA    7
#define DT_RELASZ  8
#define DT_RELAENT 9

#define R_RISCV_RELATIVE 3

typedef struct {
    unsigned char e_ident[EI_NIDENT];
    uint16_t      e_type;
    uint16_t      e_machine;
    uint32_t      e_version;
    uint64_t      e_entry;
    uint64_t      e_phoff;
    uint64_t      e_shoff;
    uint32_t      e_flags;
    uint16_t      e_ehsize;
    uint16_t      e_phentsize;
    uint16_t      e_phnum;
    uint16_t      e_shentsize;
    uint16_t      e_shnum;
    uint16_t      e_shstrndx;
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
    int64_t  d_tag;
    uint64_t d_val;
} Elf64_Dyn;

typedef struct {
    uint64_t r_offset;
    uint64_t r_info;
    int64_t  r_addend;
} Elf64_Rela;

static void memcpy(void *dst, const void *src, uint64_t n) {
    char *d = (char *)dst;
    const char *s = (const char *)src;
    for (uint64_t i = 0; i < n; i++) d[i] = s[i];
}

static void memset(void *dst, int c, uint64_t n) {
    char *d = (char *)dst;
    for (uint64_t i = 0; i < n; i++) d[i] = (char)c;
}

static uint64_t elf_file_offset(Elf64_Phdr *phdrs, uint16_t phnum, uint64_t vaddr) {
    for (uint16_t i = 0; i < phnum; i++) {
        if (phdrs[i].p_type != PT_LOAD)
            continue;
        if (vaddr >= phdrs[i].p_vaddr &&
            vaddr < phdrs[i].p_vaddr + phdrs[i].p_filesz) {
            return (vaddr - phdrs[i].p_vaddr) + phdrs[i].p_offset;
        }
    }
    return 0;
}

uint64_t load_user(const uint8_t *image) {
    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)image;

    if (ehdr->e_ident[0] != 0x7f ||
        ehdr->e_ident[1] != 'E' ||
        ehdr->e_ident[2] != 'L' ||
        ehdr->e_ident[3] != 'F') {
        return 0;
    }

    Elf64_Phdr *phdr = (Elf64_Phdr *)(image + ehdr->e_phoff);
    uint16_t phnum = ehdr->e_phnum;

    for (uint16_t i = 0; i < phnum; i++) {
        if (phdr[i].p_type != PT_LOAD)
            continue;
        if (phdr[i].p_vaddr == 0 && phdr[i].p_filesz > 0)
            continue;

        uint8_t *dst = (uint8_t *)(uintptr_t)phdr[i].p_vaddr;
        uint8_t *src = (uint8_t *)image + phdr[i].p_offset;

        memcpy(dst, src, phdr[i].p_filesz);

        if (phdr[i].p_memsz > phdr[i].p_filesz) {
            memset(dst + phdr[i].p_filesz, 0,
                   phdr[i].p_memsz - phdr[i].p_filesz);
        }
    }

    uint64_t rela_addr = 0, rela_size = 0, rela_ent = 0;

    for (uint16_t i = 0; i < phnum; i++) {
        if (phdr[i].p_type != PT_DYNAMIC)
            continue;

        Elf64_Dyn *dyn = (Elf64_Dyn *)image;
        uint64_t dyn_off = elf_file_offset(phdr, phnum, phdr[i].p_vaddr);
        dyn = (Elf64_Dyn *)(image + dyn_off);
        uint64_t dyn_count = phdr[i].p_filesz / sizeof(Elf64_Dyn);

        for (uint64_t j = 0; j < dyn_count; j++) {
            if (dyn[j].d_tag == DT_NULL)
                break;
            if (dyn[j].d_tag == DT_RELA)
                rela_addr = dyn[j].d_val;
            if (dyn[j].d_tag == DT_RELASZ)
                rela_size = dyn[j].d_val;
            if (dyn[j].d_tag == DT_RELAENT)
                rela_ent = dyn[j].d_val;
        }
        break;
    }

    if (rela_addr && rela_size && rela_ent >= sizeof(Elf64_Rela)) {
        uint64_t rela_off = elf_file_offset(phdr, phnum, rela_addr);
        Elf64_Rela *rela = (Elf64_Rela *)(image + rela_off);
        uint64_t rela_count = rela_size / rela_ent;

        for (uint64_t i = 0; i < rela_count; i++) {
            uint32_t type = (uint32_t)(rela[i].r_info & 0xffffffff);
            if (type == R_RISCV_RELATIVE) {
                uint64_t *ptr = (uint64_t *)(uintptr_t)rela[i].r_offset;
                *ptr = (uint64_t)rela[i].r_addend;
            }
        }

        for (uint16_t i = 0; i < phnum; i++) {
            if (phdr[i].p_type != PT_DYNAMIC)
                continue;
            if (phdr[i].p_vaddr < 0x80000000)
                continue;
            Elf64_Dyn *load_dyn = (Elf64_Dyn *)(uintptr_t)phdr[i].p_vaddr;
            uint64_t ld_count = phdr[i].p_filesz / sizeof(Elf64_Dyn);
            for (uint64_t j = 0; j < ld_count; j++) {
                if (load_dyn[j].d_tag == DT_NULL)
                    break;
                if (load_dyn[j].d_tag == DT_RELA)
                    load_dyn[j].d_val = 0;
                if (load_dyn[j].d_tag == DT_RELASZ)
                    load_dyn[j].d_val = 0;
            }
            break;
        }
    }

    return ehdr->e_entry;
}

int batch_next(struct trapframe *tf) {
    if (current_app + 1 >= APP_COUNT) {
        sbi_puts("[kernel] batch complete\n");
        sbi_shutdown();
        return 0;
    }

    current_app++;
    uint64_t entry = load_user(apps[current_app]);
    if (entry == 0) {
        sbi_puts("[kernel] invalid batch application\n");
        sbi_shutdown();
        return 0;
    }

    // trap_handler advances sepc after returning from every syscall.
    tf->sepc = entry - 4;
    tf->sp = USER_STACK;
    return 1;
}
