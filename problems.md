## 7-18
1. opensbi can't jump to 0x80200000, $a2 = 0x07, cpu can't jump there so that enter exception.(solved:opensbi is different with rustsbi-qemu.vin)
2. Why can't put stack frame in data section? What if put it in bss but dont clear it?(solved: data section increase the size of elf, it's fine to not clearing bss)
3. How to use asm?register asm;inline asm
## 7-19
4. musl staic link executable
## 7-20
5. mem mapping of rv64
## 7-21
6. main need a ret addr, _start dont.
7. kernel's main and user app's main
8. sbi things
'''
struct sbiret {
    long error;
    long value;
};

struct sbiret sbi_ecall(int ext, int fid, unsigned long arg0,
                        unsigned long arg1, unsigned long arg2,
                        unsigned long arg3, unsigned long arg4,
                        unsigned long arg5) {
    struct sbiret ret;
    register unsigned long a0 asm ("a0") = arg0;
    register unsigned long a1 asm ("a1") = arg1;
    register unsigned long a2 asm ("a2") = arg2;
    register unsigned long a3 asm ("a3") = arg3;
    register unsigned long a4 asm ("a4") = arg4;
    register unsigned long a5 asm ("a5") = arg5;
    register unsigned long a6 asm ("a6") = fid;
    register unsigned long a7 asm ("a7") = ext;

    asm volatile ("ecall"
                  : "+r" (a0), "+r" (a1)
                  : "r" (a2), "r" (a3), "r" (a4), "r" (a5), "r" (a6), "r" (a7)
                  : "memory");
    ret.error = a0;
    ret.value = a1;
    return ret;
}
'''

## 7-22

9. musl syscalls, _start_c
10. align
11. gcc a.c -lm, gcc a.c, gcc -lm a.c
12. teneative definition,C language
13. inline asm (the note)

## 7-23

13. ELF

    1. layout :elf header,section header,program header(segment)

    1. type:elf;segment;

    1. rela

14. **Common Automatic Variables:**

| Variable | Meaning                             |
| -------- | ----------------------------------- |
| `$@`     | Target name                         |
| `$<`     | First prerequisite                  |
| `$^`     | All prerequisites (deduplicated)    |
| `$+`     | All prerequisites (with duplicates) |
| `$?`     | Prerequisites newer than target     |

15. mymusl exec segment fault, user exec ok

## 7-24