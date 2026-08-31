LINKER = linker.ld
CC = riscv64-unknown-elf-gcc
CFLAGS = -nostdlib -ffreestanding -mcmodel=medany
LDFLAGS = -T ${LINKER}
OS_ELF = build/os.elf
GDB = gdb-multiarch
BOOTLOADER = bootloader/rustsbi-qemu.bin

SRC_C = src/main.c src/multi.c src/page.c src/sbi.c src/syscall.c src/trap.c src/vm.c
SRC_ASM = src/entry.s
OBJ = build/entry.o build/main.o build/multi.o build/page.o build/sbi.o build/syscall.o build/trap.o build/vm.o

all: elf

USR_ELF = user/build/riscv64/usershell \
          user/build/riscv64/ch2b_hello_world \
          user/build/riscv64/ch5_exit0 \
          user/build/riscv64/ch5_exit1 \
          user/build/riscv64/ch5_ppid \
          user/build/riscv64/ch5b_exit \
          user/build/riscv64/ch5b_exec_simple \
          user/build/riscv64/ch5b_forktest0 \
          user/build/riscv64/ch5b_forktest1 \
          user/build/riscv64/ch5b_forktest2 \
          user/build/riscv64/ch5b_getpid

build/entry.o: src/entry.s $(USR_ELF)
	mkdir -p build
	${CC} ${CFLAGS} -c $< -o $@

build/%.o: src/%.s
	mkdir -p build
	${CC} ${CFLAGS} -c $< -o $@

build/%.o: src/%.c
	mkdir -p build
	${CC} ${CFLAGS} -c $< -o $@

elf: $(OBJ) ${LINKER}
	${CC} ${CFLAGS} ${LDFLAGS} $(OBJ) -o $(OS_ELF)

run: all
	qemu-system-riscv64 -machine virt \
	    -bios $(BOOTLOADER) \
	    -device loader,file=$(OS_ELF) \
	    -nographic

debug: all
	qemu-system-riscv64 -machine virt \
	    -bios $(BOOTLOADER) \
	    -device loader,file=$(OS_ELF) \
	    -nographic \
		-s -S

gdb: 
	${GDB} -ex "set architecture riscv:rv64" \
	-ex "target remote localhost:1234" \
	${OS_ELF}

clean:
	rm -rf build/*.o build/os.elf

.PHONY: all elf run debug gdb clean
