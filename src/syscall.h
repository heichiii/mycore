
#include "trap.h"

#define SYS_IOCTL          29
#define SYS_openat         56
#define SYS_close          57
#define SYS_lseek          62
#define SYS_WRITE          64
#define SYS_READ           63
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
#define SYS_getpid         172
#define SYS_getppid        173
#define SYS_munmap         215
#define SYS_sbrk           214
#define SYS_mmap          222
#define SYS_execve        221
#define SYS_wait4         260
#define SYS_trace          410

#define ENOSYS  38
#define ENOENT   2
#define ENOMEM  12
#define ESPIPE  29

#define PROT_NONE 0
#define PROT_READ 1
#define PROT_WRITE 2
#define PROT_EXEC 4
#define MAP_ANONYMOUS 0x20

#define TRACE_READ    0
#define TRACE_WRITE   1
#define TRACE_SYSCALL 2

void sys_write(struct trapframe *tf);
void sys_read(struct trapframe *tf);
void sys_exit(struct trapframe *tf);
void sys_yield(struct trapframe *tf);
long sys_mmap(struct trapframe *tf);
long sys_munmap(struct trapframe *tf);
long sys_sbrk(struct trapframe *tf);
long sys_trace(struct trapframe *tf);
