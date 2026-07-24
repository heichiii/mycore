
#include "trap.h"

#define SYS_IOCTL          29
#define SYS_openat         56
#define SYS_close          57
#define SYS_lseek          62
#define SYS_WRITE          64
#define SYS_WRITEV         66
#define SYS_ppoll          73
#define SYS_readlinkat     78
#define SYS_fstatat        79
#define SYS_EXIT           93
#define SYS_exit_group     94
#define SYS_set_tid_address 96
#define SYS_futex          98
#define SYS_sched_yield    124
#define SYS_rt_tgsigqueueinfo 240
#define SYS_clone          220
#define SYS_mmap          222

#define ENOSYS  38
#define ENOENT   2
#define ENOMEM  12
#define ESPIPE  29

void sys_write(struct trapframe *tf);
void sys_exit(struct trapframe *tf);
void sys_yield(struct trapframe *tf);
