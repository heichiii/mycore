# Sv39 Virtual Memory Implementation

## Overview

This kernel implements a minimal Sv39 virtual memory system for RISC-V. The implementation enables each user task to run in its own isolated virtual address space while sharing the kernel mappings.

## Architecture

### Sv39 Address Translation

Sv39 uses a 3-level page table with 39-bit virtual addresses:

```
Virtual Address (39 bits):
[38:30] - VPN[2] (9 bits) - Level 2 page table index
[29:21] - VPN[1] (9 bits) - Level 1 page table index  
[20:12] - VPN[0] (9 bits) - Level 0 page table index
[11:0]  - Page offset (12 bits)

Physical Address:
[55:12] - PPN (44 bits) - Physical page number
[11:0]  - Page offset (12 bits)
```

### Page Table Entry Format

```
PTE bits:
[63:54] - Reserved
[53:10] - PPN (44 bits) - Physical page number
[9:8]   - RSW (Reserved for software)
[7]     - D (Dirty)
[6]     - A (Accessed)
[5]     - G (Global)
[4]     - U (User accessible)
[3]     - X (Executable)
[2]     - W (Writable)
[1]     - R (Readable)
[0]     - V (Valid)
```

## Implementation Details

### 1. Page Table Walking (vm.c:43-62)

The `walk()` function traverses the 3-level page table hierarchy:

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

**Key points:**
- Iterates from L2 → L1 → L0
- Extracts 9-bit index per level using `(va >> (12 + 9 * level)) & 0x1ff`
- Creates intermediate page tables on demand if `create=1`
- Returns pointer to the leaf PTE

### 2. Kernel Memory Layout (vm.c:117-130)

The kernel maps two regions:

```c
void vm_init(void)
{
    kernel_root = alloc_page_table();
    
    // Map kernel code/data (identity mapping)
    uint64_t kernel_start = 0x80200000;  // from linker script
    uint64_t kernel_end = 0x80348008;     // ~1.3MB kernel
    vm_map(kernel_root, kernel_start, kernel_start, 
           kernel_end - kernel_start,
           PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);
    
    // Map physical page allocator region (identity mapping)
    vm_map(kernel_root, 0x86000000, 0x86000000,
           0x87e00000 - 0x86000000,  // ~30MB for user pages
           PTE_R | PTE_W | PTE_A | PTE_D);
    
    vm_activate(kernel_root);
}
```

**Why these specific regions?**
- **Kernel region (0x80200000-0x80348008)**: Contains kernel code, data, and embedded user ELF binaries
- **Page allocator (0x86000000-0x87e00000)**: Physical memory pool for user pages. The kernel needs to access these physical addresses to zero pages and copy data.

**Why NOT map everything?**
- Mapping all of DRAM (0x80200000-0x88000000) would require ~120MB worth of page table pages
- With only 256 page tables (1MB pool), we'd exhaust memory before loading any tasks
- Minimal approach: only map what the kernel actually needs to access

### 3. Task Address Spaces (multi.c:294-310)

Each task gets its own address space:

```c
void task_init_all(void)
{
    for (int i = 0; i < MAX_TASKS; i++) {
        // Clone kernel mappings into task's page table
        tasks[i].pagetable = vm_clone_kernel();
        
        // Load user ELF binary into task's address space
        uint64_t entry = load_user(tasks[i].pagetable, 
                                   programs[i].image,
                                   programs[i].runtime_base);
        
        // Map user stack (64KB per task)
        uint64_t stack_start = programs[i].stack_top - TASK_STACK_SIZE;
        for (uint64_t va = stack_start; va <= programs[i].stack_top; 
             va += PAGE_SIZE) {
            void *page = alloc_page();
            vm_map(tasks[i].pagetable, va, (uint64_t)page, PAGE_SIZE,
                   PTE_R | PTE_W | PTE_U | PTE_A | PTE_D);
        }
    }
}
```

**Memory layout per task:**
```
Task 0: Code @ 0x804c0000, Stack @ 0x80b00000
Task 1: Code @ 0x804e0000, Stack @ 0x80f00000
Task 2: Code @ 0x80500000, Stack @ 0x81300000
Task 3: Code @ 0x80400000, Stack @ 0x80700000
```

### 4. User Program Loading (multi.c:180-283)

The `load_user()` function handles PIE (Position Independent Executable) loading:

```c
uint64_t load_user(pagetable_t root, const uint8_t *image,
                   uint64_t runtime_base)
{
    // Parse ELF header
    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)image;
    Elf64_Phdr *phdr = (Elf64_Phdr *)(image + ehdr->e_phoff);
    
    // Calculate load bias (relocation offset)
    uint64_t base = preferred_base(phdr, ehdr->e_phnum);
    uint64_t bias = runtime_base - base;
    
    // Load each PT_LOAD segment
    for (each segment) {
        // Allocate physical pages for this segment
        for (uint64_t va = map_start; va < map_end; va += PAGE_SIZE) {
            void *page = alloc_page();
            vm_map(root, va, (uint64_t)page, PAGE_SIZE, flags);
        }
        
        // Copy segment data through page table translation
        copy_to_user(root, dst_addr, image + offset, filesz);
        zero_user(root, dst_addr + filesz, memsz - filesz);
    }
    
    // Process relocations for PIE
    for (each R_RISCV_RELATIVE relocation) {
        uint64_t target = reloc_offset + bias;
        uint64_t pa = vm_translate(root, target);
        *(uint64_t *)pa = addend + bias;
    }
    
    return entry_point + bias;
}
```

**Key implementation details:**

1. **Virtual memory aware copying** (multi.c:126-153):
   ```c
   static void copy_to_user(pagetable_t root, uint64_t va,
                            const void *src, uint64_t n)
   {
       while (n) {
           uint64_t pa = vm_translate(root, va);  // VA→PA
           uint64_t chunk = PAGE_SIZE - (va & (PAGE_SIZE - 1));
           if (chunk > n) chunk = n;
           memcpy((void *)pa, src, chunk);  // Copy to physical address
           va += chunk;
           s += chunk;
           n -= chunk;
       }
   }
   ```
   
   **Why translate?** After enabling paging, we can't directly write to user virtual addresses from kernel code. We must:
   1. Translate user VA → PA using the user's page table
   2. Write to the physical address (which is identity-mapped in kernel space)

2. **PIE Relocation**:
   - User binaries are compiled as static-PIE to be relocatable
   - The kernel applies `R_RISCV_RELATIVE` relocations to fix up addresses
   - Without this, absolute address references would point to wrong locations

### 5. Context Switching (multi.c:489-503, 510-524)

When switching tasks, we must switch page tables:

```c
void task_schedule(struct trapframe *tf)
{
    tasks[current_task].context = *tf;
    tasks[current_task].state = TASK_READY;
    
    int next = next_ready();
    current_task = next;
    tasks[current_task].state = TASK_RUNNING;
    
    // Switch to the new task's address space
    vm_activate(tasks[current_task].pagetable);
    
    *tf = tasks[current_task].context;
}

void vm_activate(pagetable_t root)
{
    uint64_t satp = SATP_MODE_SV39 | ((uint64_t)root >> 12);
    asm volatile("csrw satp, %0\n\tsfence.vma" 
                 :: "r"(satp) : "memory");
}
```

**SATP register format:**
```
[63:60] - MODE (8 = Sv39)
[59:44] - ASID (Address Space ID, unused here)
[43:0]  - PPN of root page table
```

**sfence.vma**: Flushes TLB entries. Required after changing SATP to ensure hardware uses the new page table.

## Bug Fixes Applied

### Bug 1: Missing Page Allocator Mapping

**Problem**: The kernel couldn't access physical pages returned by `alloc_page()` (0x86000000-0x87e00000).

**Symptom**: Kernel hung during task initialization when trying to zero allocated pages.

**Fix** (vm.c:119-121):
```c
vm_map(kernel_root, PAGE_ALLOC_START, PAGE_ALLOC_START,
       PAGE_ALLOC_END - PAGE_ALLOC_START,
       PTE_R | PTE_W | PTE_A | PTE_D);
```

### Bug 2: Stack Boundary Not Mapped

**Problem**: Stack mapping loop used `va < stack_top`, leaving the top boundary page unmapped.

**Symptom**: Page fault at exact stack_top address (e.g., 0x81300000).

**Fix** (multi.c:300):
```c
// Changed from: va < programs[i].stack_top
for (uint64_t va = stack_start; va <= programs[i].stack_top; va += PAGE_SIZE)
```

### Bug 3: Task/Stack Misalignment

**Problem**: Tasks were assigned wrong stacks in the programs array.

**Symptom**: PIE binaries with hardcoded references failed when stack didn't match expected location.

**Fix** (multi.c:102-107):
```c
// Before: Task 0 got TASK0_STACK, Task 3 got TASK3_STACK
// After: Align task index with stack index
{_user_elf_yield0, TASK1_BASE, TASK1_STACK},  // Task 0
{_user_elf_yield1, TASK2_BASE, TASK2_STACK},  // Task 1
{_user_elf_yield2, TASK3_BASE, TASK3_STACK},  // Task 2
{_user_elf_hello,  TASK0_BASE, TASK0_STACK},  // Task 3
```

### Bug 4: Stack Pointer at Boundary

**Problem**: Stack pointer initialized to exact boundary (stack_top), which may be accessed before decrementing.

**Fix** (multi.c:309):
```c
tasks[i].context.sp = programs[i].stack_top - 16;
```

## Memory Efficiency

**Page table budget**: 256 page tables × 4KB = 1MB

**Usage breakdown**:
- 1 page table for kernel root
- 4 page tables for task roots (cloned from kernel)
- ~6 page tables per task for kernel mapping clone
- ~2 page tables per task for user code/stack mappings
- Total: ~1 + 4 × (1 + 6 + 2) = ~37 page tables

**Why minimal mapping works**:
- User code/stack only exists in per-task page tables
- Kernel page table only contains what kernel actually accesses
- Avoids exhausting the page table pool

## Summary

This minimal Sv39 implementation demonstrates:

1. **3-level page table walking** with on-demand intermediate table allocation
2. **Kernel-user address space separation** through page table cloning
3. **Per-task virtual memory isolation** with shared kernel mappings
4. **PIE binary loading** with relocation and virtual-memory-aware copying
5. **Context switching** with SATP updates and TLB flushes
6. **Memory-efficient design** that fits within a 1MB page table budget

The implementation successfully runs 4 concurrent user tasks with proper virtual memory isolation.
