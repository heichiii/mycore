LINKER = linker.ld
CC= riscv64-unknown-elf-gcc
CFLAGS =  -nostdlib -ffreestanding -mcmodel=medany
LDFLAGS = -T ${LINKER}
OS_BIN = os.bin
OS_ELF = os.elf
GDB = gdb-multiarch
KERNEL_ENTRY_PA = 0x80200000

all: bin

elf: hello.c start.S ${LINKER}
	${CC} ${CFLAGS} ${LDFLAGS} \
		start.S hello.c -o os.elf
bin: elf
	riscv64-unknown-elf-objcopy -O binary os.elf ${OS_BIN}

# run: all
# 	qemu-system-riscv64 -machine virt -bios default -device loader,file=$(OS_BIN),addr=$(KERNEL_ENTRY_PA) -nographic -global virt-machine.opensbi-next-addr=0x80200000
run: all
	qemu-system-riscv64 -machine virt -bios default -nographic -kernel $(OS_ELF)

debug: all
	qemu-system-riscv64 -machine virt -bios default \
		-device loader,file=${OS_BIN},addr=${KERNEL_ENTRY_PA} \
		-nographic \
		-s -S

gdb: 
	${GDB} -ex "set architecture riscv:rv64" \
	-ex "target remote localhost:1234" \
	${OS_ELF}