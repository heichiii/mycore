## 7-18
1. opensbi can't jump to 0x80200000, $a2 = 0x07, cpu can't jump there so that enter exception.(solved:opensbi is different with rustsbi-qemu.vin)
2. Why can't put stack frame in data section? What if put it in bss but dont clear it?(solved: data section increase the size of elf, it's fine to not clearing bss)
3. How to use asm?register asm;inline asm
## 7-19
4. musl staic link executable
## 7-20
5. mem mapping of rv64
