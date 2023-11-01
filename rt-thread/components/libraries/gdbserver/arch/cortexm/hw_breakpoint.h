/*
 * CORTEXM GDB support
 * arch-specific portion of GDB stub
 * 
 * File      : arch_gdb.h(cortexm)
 * This file is part of RT-Thread RTOS
 * COPYRIGHT (C) 2006, RT-Thread Develop Team
 *
 * The license and distribution terms for this file may be
 * found in the file LICENSE in this distribution or at
 * http://www.rt-thread.org/license/LICENSE
 *
 * Change Logs:
 * Date           Author       Notes
 * 2014-07-04     wzyy2      first version
 */
#ifndef __ARM_GDB_H__
#define __ARM_GDB_H__ 

#define INS_BREAK_CONNECT  0xbebebebf
#define INS_BREAK_SOFTWARE 0xbebebebe

#define BREAK_INSTR_SIZE      4
#define GDB_MAX_BREAKPOINTS   32

#define GEN_REG_NR   (17)
#define REG_NR       (17 + 32 + 16 + 3)

/* the exception stack without VFP registers */
struct rt_hw_exp_stack
{
    unsigned long           type;           // State type
    unsigned long           vector;         // Exception vector number
    unsigned long           basepri;        // BASEPRI

    unsigned long           r4;       // Remaining CPU registers
    unsigned long           r5;       // Remaining CPU registers
    unsigned long           r6;       // Remaining CPU registers
    unsigned long           r7;       // Remaining CPU registers
    unsigned long           r8;       // Remaining CPU registers
    unsigned long           r9;       // Remaining CPU registers
    unsigned long           r10;       // Remaining CPU registers
    unsigned long           fp;       // Remaining CPU registers
    unsigned long           sp;       // Remaining CPU registers

    unsigned long           xlr;            // Exception return LR
    // The following are saved and restored automatically by the CPU
    // for exceptions or interrupts.
    unsigned long           r0;
    unsigned long           r1;
    unsigned long           r2;
    unsigned long           r3;
    unsigned long           ip;
    unsigned long           lr;
    unsigned long           pc;
    unsigned long           cpsr;
};

struct rt_hw_stack
{
    unsigned long cpsr;
    unsigned long r0;
    unsigned long r1;
    unsigned long r2;
    unsigned long r3;
    unsigned long r4;
    unsigned long r5;
    unsigned long r6;
    unsigned long r7;
    unsigned long r8;
    unsigned long r9;
    unsigned long r10;
    unsigned long fp;
    unsigned long ip;
    unsigned long lr;
    unsigned long pc;
};

enum
{
   WATCHPOINT_MODE_WRITE = 0,
   WATCHPOINT_MODE_READ,
   WATCHPOINT_MODE_ACCESS,
};

void arch_enable_debug(void);

int arch_check_debug_event(struct rt_hw_exp_stack *regs, unsigned long pc_adj);
void arch_get_stack_regs(void* sp, struct rt_hw_exp_stack *regs);

int arch_is_connect_breakpoint(uint32_t ins);
int arch_is_software_breakpoint(uint32_t ins);
int arch_cmd_query(char *cmd_buff, char *reply_buff);

int arch_get_next_pc(void* addr, unsigned long* next, int* length);

void arch_breakpoint_init(void);
void arch_remove_breakpoints(void);
void arch_activate_breakpoints(void);
void arch_deactivate_breakpoints(void);

int arch_set_sw_break(void *addr, int length);
int arch_set_hw_break(void *addr);
int arch_remove_sw_break(void *addr);
int arch_remove_hw_break(void *addr);

int arch_set_hw_watch(void *addr, size_t len, int mode);
int arch_remove_hw_watch(void *addr, size_t len, int mode);
int arch_hw_watch_get_amode(void *addr, uint32_t mode, void** hit_addr);

void arch_set_step(void* addr);
void arch_clear_step(void);
void arch_activate_step(void);
void arch_deactivate_step(void);

#endif /* __CORTEXM_GDB_H__ */
