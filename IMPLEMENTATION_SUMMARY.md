# Sv39 Virtual Memory Implementation - Summary

## Overview

This document explains the minimal Sv39 virtual memory implementation for a RISC-V OS kernel based on the actual code changes.

## What Changed

### New Files Added

1. **src/vm.c (155 lines)** - Core virtual memory management
2. **src/vm.h (23 lines)** - VM interface and PTE flag definitions
3. **src/page.c (22 lines)** - Physical page allocator
4. **src/page.h (7 lines)** - Page allocator interface
5. **SV39_IMPLEMENTATION.md** - Detailed technical documentation

### Modified Files

- **src/multi.c** - User program loading with virtual memory awareness
- **src/trap.c** - Cleaned up debug output
- **src/main.c** - Added vm_init() call
- **src/entry.s** - Trap entry preserves virtual addressing
- **Makefile** - Added vm.o and page.o to build
- **linker.ld** - Exported kernel boundaries

Total changes: **+676 lines, -34 lines** across 14 files.

## Architecture Design

### Memory Space Separation

```
┌─────────────────────────────────────┐
│         Physical Memory             │
├─────────────────────────────────────┤
│ 0x80000000 - 0x80200000  SBI        │
│ 0x80200000 - 0x80348008  Kernel     │
│ 0x86000000 - 0x87e00000  User Pages │
│ 0x87e00000 - 0x88000000  Device Tree│
└─────────────────────────────────────┘

┌─────────────────────────────────────┐
│      Kernel Virtual Address Space   │
├─────────────────────────────────────┤
│ 0x80200000 - 0x80348008  Kernel     │ Identity mapped
│ 0x86000000 - 0x87e00000  User Pages │ Identity mapped
└─────────────────────────────────────┘

┌─────────────────────────────────────┐
│    Per-Task Virtual Address Space   │
├─────────────────────────────────────┤
│ 0x80200000 - 0x80348008  Kernel     │ Cloned from kernel
│ 0x80400000 - 0x80500000  User Code  │ Task-specific
│ 0x80700000 - 0x81300000  User Stack │ Task-specific
│ 0x86000000 - 0x87e00000  User Pages │ Cloned from kernel
└─────────────────────────────────────┘
```

### Key Design Decisions

**1. Two-tier memory allocation:**
- **Page table pages** (256 × 4KB = 1MB): Static pool in kernel, for page table structures
- **User data pages** (30MB): Dynamic allocation from physical memory region

**2. Minimal kernel mapping:**
- Only map what kernel needs to access directly
- Avoids exhausting page table budget on unused mappings
- Enables supporting multiple user address spaces within 1MB page table pool

**3. Clone-on-create address spaces:**
- Each task gets kernel mappings cloned into its page table
- Allows kernel code/data access from user mode (for trap handling)
- User-specific mappings added after cloning

## Implementation Details

### 1. Page Table Walking (vm.c:45-64)

```c
static pte_t *walk(pagetable_t root, uint64_t va, int create)
{
    pagetable_t table = root;
    for (int level = 2; level > 0; level--) {
        uint64_t index = (va >> (12 + 9 * level)) & 0x1ff;
        pte_t *entry = &table[index];
        if (!(*entry & PTE_V)) {
            if (!create) return 0;
            pagetable_t next = alloc_page_table();
            *entry = pa_pte((uint64_t)next) | PTE_V;
        }
        table = (pagetable_t)pte_pa(*entry);
    }
    return &table[(va >> 12) & 0x1ff];
}
```

**How it works:**
- Traverses 3-level hierarchy: L2 (root) → L1 → L0 (leaf)
- At each level, extracts 9-bit index from VA: `(va >> (12 + 9 * level)) & 0x1ff`
- If PTE invalid and `create=1`, allocates new page table and links it
- Returns pointer to leaf PTE for final mapping

**Example:** VA = 0x80400000
```
Level 2: index = (0x80400000 >> 30) & 0x1ff = 2
Level 1: index = (0x80400000 >> 21) & 0x1ff = 2
Level 0: index = (0x80400000 >> 12) & 0x1ff = 0
```

### 2. Virtual Memory Initialization (vm.c:117-132)

```c
void vm_init(void)
{
    kernel_root = alloc_page_table();
    
    // Identity-map kernel code/data
    uint64_t kernel_start = 0x80200000;
    uint64_t kernel_end = 0x80348008;
    vm_map(kernel_root, kernel_start, kernel_start, 
           kernel_end - kernel_start,
           PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);
    
    // Identity-map physical page allocator
    vm_map(kernel_root, PAGE_ALLOC_START, PAGE_ALLOC_START,
           PAGE_ALLOC_END - PAGE_ALLOC_START,
           PTE_R | PTE_W | PTE_A | PTE_D);
    
    vm_activate(kernel_root);
}
```

**Why these specific regions?**

1. **Kernel region (0x80200000-0x80348008)**:
   - Contains kernel .text, .data, .rodata, .bss
   - Includes embedded user ELF binaries
   - Must be accessible for kernel execution

2. **Page allocator region (0x86000000-0x87e00000)**:
   - Physical pages returned by `alloc_page()`
   - Kernel needs to access these to zero pages and copy user data
   - Without this mapping, page allocation would fail

**Why NOT map all DRAM?**
- Mapping 0x80200000-0x88000000 (~120MB) requires too many page tables
- With 256 page table limit, we'd run out before loading any user tasks
- Minimal approach works: only map what kernel actually touches

### 3. Address Space Cloning (vm.c:82-101, 139-145)

```c
pagetable_t vm_clone_kernel(void)
{
    pagetable_t root = vm_create();
    if (!root || clone_table(root, kernel_root, 2) < 0)
        return 0;
    return root;
}

static int clone_table(pagetable_t dst, pagetable_t src, int level)
{
    for (int i = 0; i < PTE_PER_PAGE; i++) {
        if (!(src[i] & PTE_V))
            continue;
        if (level > 0 && !leaf(src[i])) {
            // Allocate new intermediate table
            pagetable_t child = alloc_page_table();
            dst[i] = pa_pte((uint64_t)child) | PTE_V;
            clone_table(child, (pagetable_t)pte_pa(src[i]), level - 1);
        } else {
            // Leaf PTE: copy directly (share physical pages)
            dst[i] = src[i];
        }
    }
    return 0;
}
```

**Why clone instead of share page table?**
- Each task needs its own root page table (referenced by SATP)
- Intermediate page tables must be separate to allow task-specific mappings
- Leaf PTEs can point to same physical pages (kernel memory is shared)

**Memory cost:** ~6 page table pages per task for kernel mapping clone

### 4. User Program Loading (multi.c:180-283)

The loader was completely rewritten to be virtual-memory-aware:

**Before (direct physical memory access):**
```c
uint64_t load_user(const uint8_t *image, uint64_t runtime_base)
{
    // ... parse ELF ...
    uint8_t *dst = (uint8_t *)dst_addr;  // Direct pointer!
    memcpy(dst, image + offset, filesz);
}
```

**After (page table aware):**
```c
uint64_t load_user(pagetable_t root, const uint8_t *image, 
                   uint64_t runtime_base)
{
    // Allocate and map pages
    for (uint64_t va = map_start; va < map_end; va += PAGE_SIZE) {
        void *page = alloc_page();
        vm_map(root, va, (uint64_t)page, PAGE_SIZE, flags);
    }
    
    // Copy through virtual memory translation
    copy_to_user(root, dst_addr, image + offset, filesz);
    zero_user(root, dst_addr + filesz, memsz - filesz);
}
```

**Why the change?**
- After paging is enabled, user virtual addresses aren't valid in kernel mode
- Must translate VA → PA using page table, then write to physical address
- Kernel can access physical addresses because they're identity-mapped

**Copy implementation (multi.c:126-141):**
```c
static void copy_to_user(pagetable_t root, uint64_t va,
                         const void *src, uint64_t n)
{
    const uint8_t *s = src;
    while (n) {
        uint64_t pa = vm_translate(root, va);  // Translate VA→PA
        uint64_t chunk = PAGE_SIZE - (va & (PAGE_SIZE - 1));
        if (chunk > n) chunk = n;
        memcpy((void *)pa, s, chunk);  // Copy to physical address
        va += chunk;
        s += chunk;
        n -= chunk;
    }
}
```

Handles page boundaries correctly by chunking the copy operation.

### 5. PIE Binary Relocation (multi.c:232-266)

User programs are compiled as position-independent executables (PIE):

```c
// Calculate relocation bias
uint64_t base = preferred_base(phdr, ehdr->e_phnum);
uint64_t bias = runtime_base - base;

// Apply R_RISCV_RELATIVE relocations
for (each relocation entry) {
    if (type == R_RISCV_RELATIVE) {
        uint64_t target = rela[i].r_offset + bias;
        uint64_t pa = vm_translate(root, target);
        *(uint64_t *)pa = rela[i].r_addend + bias;
    }
}
```

**Why relocations?**
- PIE binaries contain placeholder addresses relative to base 0
- Kernel loads them at arbitrary addresses (e.g., 0x80400000)
- Relocations update absolute address references to correct values

**Example:** Binary expects to load at 0x10000, kernel loads at 0x80400000:
- bias = 0x80400000 - 0x10000 = 0x803f0000
- Global pointer at offset 0x1234 becomes 0x803f1234

### 6. Context Switching (multi.c:489-503)

When switching tasks, the address space must switch:

```c
void task_schedule(struct trapframe *tf)
{
    // Save current task state
    tasks[current_task].context = *tf;
    tasks[current_task].state = TASK_READY;
    
    // Select next task
    current_task = next_ready();
    tasks[current_task].state = TASK_RUNNING;
    
    // Switch address space
    vm_activate(tasks[current_task].pagetable);
    
    // Restore new task state
    *tf = tasks[current_task].context;
}
```

**vm_activate implementation (vm.c:152-155):**
```c
void vm_activate(pagetable_t root)
{
    asm volatile("csrw satp, %0\n\t"
                 "sfence.vma" 
                 :: "r"(vm_root_satp(root)) 
                 : "memory");
}
```

**SATP format:**
```
[63:60] MODE = 8 (Sv39)
[59:44] ASID = 0 (unused)
[43:0]  PPN of root page table
```

**sfence.vma** flushes TLB entries so hardware uses new page table.

### 7. Stack Mapping (multi.c:295-309)

Each task gets a 64KB stack:

```c
uint64_t stack_start = programs[i].stack_top - TASK_STACK_SIZE;
for (uint64_t va = stack_start; va <= programs[i].stack_top; 
     va += PAGE_SIZE) {
    void *page = alloc_page();
    vm_map(tasks[i].pagetable, va, (uint64_t)page, PAGE_SIZE,
           PTE_R | PTE_W | PTE_U | PTE_A | PTE_D);
}
tasks[i].context.sp = programs[i].stack_top - 16;
```

**Key points:**
- Loop uses `<=` not `<` to include boundary page (bug fix)
- Stack grows downward from `stack_top`
- SP initialized to `stack_top - 16` to safely stay within mapped region
- Flags: U (user), R (read), W (write), A (accessed), D (dirty)

## Bug Fixes

### Bug 1: Page Allocator Not Mapped

**Symptom:** Kernel hung when initializing tasks

**Root cause:** `alloc_page()` returns physical addresses (0x86000000+), but kernel tried to zero them without mapping

**Fix:** Added identity mapping in vm_init():
```c
vm_map(kernel_root, 0x86000000, 0x86000000, 0x01e00000,
       PTE_R | PTE_W | PTE_A | PTE_D);
```

### Bug 2: Stack Boundary Not Mapped

**Symptom:** Load page fault at exact stack_top address

**Root cause:** Loop condition `va < stack_top` excluded the boundary page

**Fix:** Changed to `va <= stack_top` to include boundary

### Bug 3: Task/Stack Misalignment

**Symptom:** Task 3 accessed wrong stack address

**Root cause:** Programs array assigned wrong stacks to tasks

**Fix:** Aligned indices:
```c
// Before:
{_user_elf_yield0, TASK1_BASE, TASK0_STACK},  // Task 0
// After:
{_user_elf_yield0, TASK1_BASE, TASK1_STACK},  // Task 0
```

### Bug 4: Stack Pointer at Boundary

**Symptom:** Potential access to unmapped page

**Fix:** Initialize SP safely within range:
```c
tasks[i].context.sp = programs[i].stack_top - 16;
```

## Memory Budget Analysis

**Page table pool:** 256 pages × 4KB = 1MB

**Usage per task:**
- 1 root page table
- ~6 pages for cloned kernel mappings
- ~2 pages for user code/stack mappings
- Total: ~9 pages per task

**Total for 4 tasks:** 1 (kernel root) + 4 × 9 = 37 pages (~148KB)

**Remaining:** 219 pages (~876KB) available for future expansion

## Testing Results

```
Hello from kernel!
vm_init done
task_init done
trap_init done
AAAAAAAAAA [1/5]
CCCCCCCCCC [1/5]
BBBBBBBBBB [1/5]
hello:Hello World!
hello:Before loop.
hello:Finish loop.
[... tasks continue executing ...]
Test write A OK!
Test write C OK!
Test write B OK!
[kernel] all tasks exited
```

All 4 tasks run successfully with proper memory isolation and context switching.

## Key Takeaways

1. **Minimal mapping strategy** - Only map what kernel needs, conserves page tables
2. **Two-tier allocation** - Separate pools for page tables vs. user data
3. **Virtual-memory-aware copying** - Must translate VA→PA when accessing user memory
4. **Address space cloning** - Share kernel mappings, isolate user mappings
5. **PIE binary support** - Relocations enable loading at arbitrary addresses
6. **Context switching integration** - Switch page tables along with task context

## File Organization

```
src/
├── vm.c              # Core virtual memory (walk, map, clone, activate)
├── vm.h              # VM interface and PTE flag definitions
├── page.c            # Physical page allocator
├── page.h            # Page allocator interface
├── multi.c           # Task management with VM support
├── trap.c            # Exception handling
├── main.c            # Kernel entry (calls vm_init)
└── entry.s           # Trap entry/exit (preserves SATP)
```

## Conclusion

This minimal Sv39 implementation provides full virtual memory support with just 200 lines of core VM code, demonstrating that a functional MMU system can be achieved with careful resource management and targeted feature selection.
