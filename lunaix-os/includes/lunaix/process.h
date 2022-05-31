#ifndef __LUNAIX_PROCESS_H
#define __LUNAIX_PROCESS_H

#include <stdint.h>
#include <arch/x86/interrupts.h>
#include <lunaix/mm/mm.h>
#include <lunaix/types.h>
#include <lunaix/clock.h>
#include <lunaix/timer.h>

// 虽然内核不是进程，但为了区分，这里使用Pid=-1来指代内核。这主要是方便物理页所有权检查。
#define KERNEL_PID -1

#define PROC_STOPPED 0
#define PROC_RUNNING 1
#define PROC_TERMNAT 2
#define PROC_DESTROY 4
#define PROC_SPOILED 8
#define PROC_BLOCKED 16


struct proc_mm {
    heap_context_t u_heap;
    struct mm_region* regions;
};

struct proc_info {
    pid_t pid;
    struct proc_info* parent;
    isr_param intr_ctx;
    struct proc_mm mm;
    void* page_table;
    time_t created;
    uint8_t state;
    int32_t exit_code;
    int32_t k_status;
    struct lx_timer* timer;
};

extern struct proc_info* __current;


pid_t alloc_pid();

/**
 * @brief 向系统发布一个进程，使其可以被调度。
 * 
 * @param process 
 */
void push_process(struct proc_info* process);

void destroy_process(pid_t pid);

void setup_proc_mem(struct proc_info* proc, uintptr_t kstack_from);

/**
 * @brief 复制当前进程（LunaixOS的类 fork (unix) 实现）
 * 
 */
void dup_proc();

/**
 * @brief 创建新进程（LunaixOS的类 CreateProcess (Windows) 实现）
 * 
 */
void new_proc();

/**
 * @brief 终止（退出）当前进程
 * 
 */
void terminate_proc(int exit_code);

int orphaned_proc(pid_t pid);

struct proc_info* get_process(pid_t pid);

#endif /* __LUNAIX_PROCESS_H */
