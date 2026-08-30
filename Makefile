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

USR_ELF = user/build/riscv64/ch4_mmap0 user/build/riscv64/ch4_mmap1 \
          user/build/riscv64/ch4_mmap2 user/build/riscv64/ch4_mmap3 \
          user/build/riscv64/ch4_sbrk  user/build/riscv64/ch4_trace1 \
          user/build/riscv64/ch4_unmap0 user/build/riscv64/ch4_unmap1

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
