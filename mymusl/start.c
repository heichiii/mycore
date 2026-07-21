extern int main(int argc, char **argv);

void _start(void) {
    main(0, 0);
    asm volatile("li a7, 93; ecall");
    while(1) {}
}
