LINKER = linker.ld
CC = riscv64-unknown-elf-gcc
CFLAGS = -nostdlib -ffreestanding -mcmodel=medany
LDFLAGS = -T ${LINKER}
OS_ELF = build/os.elf
GDB = gdb-multiarch
BOOTLOADER = bootloader/rustsbi-qemu.bin

SRC_C = src/main.c src/load_single_app.c src/sbi.c src/syscall.c src/trap.c
SRC_ASM = src/entry.s
OBJ = build/entry.o build/main.o build/load_single_app.o build/sbi.o build/syscall.o build/trap.o

all: elf

build/entry.o: src/entry.s mymusl/build/hello.elf user/target/elf/ch2b_exit user/target/elf/ch2b_power
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
