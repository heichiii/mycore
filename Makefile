LINKER = linker.ld
CC= riscv64-unknown-elf-gcc
CFLAGS =  -nostdlib -ffreestanding -mcmodel=medany
LDFLAGS = -T ${LINKER}
OS_ELF = build/os.elf
GDB = gdb-multiarch
SRC = src/hello.c src/start.S
BOOTLOADER = bootloader/rustsbi-qemu.bin

all: elf

elf: $(SRC) ${LINKER}
	${CC} ${CFLAGS} ${LDFLAGS} \
		$(SRC) -o $(OS_ELF)

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