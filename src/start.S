.section .text_entry
.globl kernel_start
kernel_start:
	la sp, _stack_top

	call main
1:  j 1b

.section .text
.globl trap_entry
trap_entry:
	csrw sscratch, sp

	addi sp, sp, -280

	sd x1, 0(sp)
	csrr t0, sscratch
	sd t0, 8(sp)
	sd x3, 16(sp)
	sd x4, 24(sp)
	sd x5, 32(sp)
	sd x6, 40(sp)
	sd x7, 48(sp)
	sd x8, 56(sp)
	sd x9, 64(sp)
	sd x10, 72(sp)
	sd x11, 80(sp)
	sd x12, 88(sp)
	sd x13, 96(sp)
	sd x14, 104(sp)
	sd x15, 112(sp)
	sd x16, 120(sp)
	sd x17, 128(sp)
	sd x18, 136(sp)
	sd x19, 144(sp)
	sd x20, 152(sp)
	sd x21, 160(sp)
	sd x22, 168(sp)
	sd x23, 176(sp)
	sd x24, 184(sp)
	sd x25, 192(sp)
	sd x26, 200(sp)
	sd x27, 208(sp)
	sd x28, 216(sp)
	sd x29, 224(sp)
	sd x30, 232(sp)
	sd x31, 240(sp)

	csrr t0, sepc
	sd t0, 248(sp)
	csrr t0, sstatus
	sd t0, 256(sp)
	csrr t0, scause
	sd t0, 264(sp)
	csrr t0, stval
	sd t0, 272(sp)

	mv a0, sp
	call trap_handler

	ld t0, 248(sp)
	csrw sepc, t0
	ld t0, 256(sp)
	csrw sstatus, t0

	ld x1, 0(sp)
	ld x3, 16(sp)
	ld x4, 24(sp)
	ld x5, 32(sp)
	ld x6, 40(sp)
	ld x7, 48(sp)
	ld x8, 56(sp)
	ld x9, 64(sp)
	ld x10, 72(sp)
	ld x11, 80(sp)
	ld x12, 88(sp)
	ld x13, 96(sp)
	ld x14, 104(sp)
	ld x15, 112(sp)
	ld x16, 120(sp)
	ld x17, 128(sp)
	ld x18, 136(sp)
	ld x19, 144(sp)
	ld x20, 152(sp)
	ld x21, 160(sp)
	ld x22, 168(sp)
	ld x23, 176(sp)
	ld x24, 184(sp)
	ld x25, 192(sp)
	ld x26, 200(sp)
	ld x27, 208(sp)
	ld x28, 216(sp)
	ld x29, 224(sp)
	ld x30, 232(sp)
	ld x31, 240(sp)

	ld sp, 8(sp)
	sret

.section .text
.globl enter_user
enter_user:
	csrw sepc, a0
	mv sp, a1
	li t0, 0x100
	csrc sstatus, t0
	li t0, 0x20
	csrs sstatus, t0
	sret

.section .rodata.user_elf, "a"
.globl _user_elf_start
_user_elf_start:
	.incbin "user/build/riscv64/ch2b_hello_world"
.globl _user_elf_end
_user_elf_end:

.section .bss
.align 4
_stack_lower_bound:
	.space 4096
_stack_top:

