.section .text_entry
.globl kernel_start
kernel_start:
	la sp, _stack_top

	call main
1:  j 1b

.section .bss
.align 4
_stack_lower_bound:
	.space 4096
_stack_top:

