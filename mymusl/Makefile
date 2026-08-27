all: dynamic_pie static_exec dynamic_exec static_pie

hello: hello.c
	riscv64-linux-musl-gcc hello.c -o build/hello.elf -static -Wl,-Ttext,0x4000
	
dynamic_pie: hello.c
	riscv64-linux-musl-gcc hello.c -o build/dp.elf

static_exec: hello.c
	riscv64-linux-musl-gcc hello.c -o build/se.elf -static -fno-pie -no-pie

dynamic_exec: hello.c
	riscv64-linux-musl-gcc hello.c -o build/de.elf -fno-pie -no-pie

static_pie: hello.c
	riscv64-linux-musl-gcc hello.c -o build/sp.elf -static


