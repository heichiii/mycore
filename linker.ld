ENTRY(kernel_start)

/* 明确声明程序头的权限 */
PHDRS {
    text PT_LOAD FLAGS(5);  /* 5 = 读+执行 (RX) */
    data PT_LOAD FLAGS(6);  /* 6 = 读+写 (RW) */
}

SECTIONS {
    /* 代码从 0x80000000 开始（QEMU virt 的 DRAM 起始地址） */
    . = 0x80200000;
    _kernel_start = .;
    
    /* 代码段：只读 + 可执行 */
    .text : {
        *(.text_entry)
        *(.text)
        *(.text.*)
    } :text  /* 放入 text 程序头 */
    
    /* 只读数据段：只读，不可执行 */
    .rodata : {
        *(.rodata)
        *(.rodata.*)
    } :text  /* 放入 text 程序头（RX） */
    
    /* 已初始化数据段：可读写 */
    .data : {
        *(.data)
        *(.data.*)
    } :data  /* 放入 data 程序头（RW） */
    
    /* 未初始化数据段：可读写，启动时清零 */
    .bss : {
        *(.bss)
        *(.bss.*)
        *(COMMON)
    } :data  /* 放入 data 程序头（RW） */

    _kernel_end = .;
}
