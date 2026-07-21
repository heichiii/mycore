all: hello

hello: hello.c
	riscv64-linux-musl-gcc hello.c -o build/hello.elf -static -Wl,-Ttext,0x80400000
	
