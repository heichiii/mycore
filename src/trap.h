#pragma once
#include<stdint.h>

struct trapframe {
    uint64_t ra;        // x1,   offset 0
    uint64_t sp;        // x2,   offset 8
    uint64_t gp;        // x3,   offset 16
    uint64_t tp;        // x4,   offset 24
    uint64_t t0;        // x5,   offset 32
    uint64_t t1;        // x6,   offset 40
    uint64_t t2;        // x7,   offset 48
    uint64_t s0;        // x8,   offset 56
    uint64_t s1;        // x9,   offset 64
    uint64_t a0;        // x10,  offset 72
    uint64_t a1;        // x11,  offset 80
    uint64_t a2;        // x12,  offset 88
    uint64_t a3;        // x13,  offset 96
    uint64_t a4;        // x14,  offset 104
    uint64_t a5;        // x15,  offset 112
    uint64_t a6;        // x16,  offset 120
    uint64_t a7;        // x17,  offset 128
    uint64_t s2;        // x18,  offset 136
    uint64_t s3;        // x19,  offset 144
    uint64_t s4;        // x20,  offset 152
    uint64_t s5;        // x21,  offset 160
    uint64_t s6;        // x22,  offset 168
    uint64_t s7;        // x23,  offset 176
    uint64_t s8;        // x24,  offset 184
    uint64_t s9;        // x25,  offset 192
    uint64_t s10;       // x26,  offset 200
    uint64_t s11;       // x27,  offset 208
    uint64_t t3;        // x28,  offset 216
    uint64_t t4;        // x29,  offset 224
    uint64_t t5;        // x30,  offset 232
    uint64_t t6;        // x31,  offset 240
    uint64_t sepc;      // offset 248
    uint64_t sstatus;   // offset 256
    uint64_t scause;    // offset 264
    uint64_t stval;     // offset 272
};

void trap_init();