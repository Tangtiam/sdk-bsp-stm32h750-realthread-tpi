/*
 * Copyright (c) 2011-2020, Shanghai Real-Thread Electronic Technology Co.,Ltd
 *
 * Change Logs:
 * Date           Author       Notes
 * 2020-02-23     Jesven         first version.
 */
#ifndef  __HW_BREAKPOINT_H__
#define  __HW_BREAKPOINT_H__

#ifdef   SOC_VEXPRESS_A9
#define INS_BREAK_CONNECT  0xe12fff7f
#else
#define INS_BREAK_CONNECT  0xde01de01   //like STM32H750XB
#endif
#define INS_BREAK_SOFTWARE 0xe12fff7e

#define BREAK_INSTR_SIZE      4
#define GDB_MAX_BREAKPOINTS   32

#define GEN_REG_NR   (17)
#define REG_NR       (17 + 32 + 16 + 3)

enum
{
   WATCHPOINT_MODE_WRITE = 0,
   WATCHPOINT_MODE_READ,
   WATCHPOINT_MODE_ACCESS,
};

int arch_check_debug_event(struct rt_hw_exp_stack *regs, unsigned long pc_adj);
void arch_enable_debug(void);
void arch_breakpoint_init(void);
int arch_is_connect_breakpoint(uint32_t ins);
int arch_is_software_breakpoint(uint32_t ins);
int arch_cmd_query(char *cmd_buff, char *reply_buff);
void arch_remove_breakpoints(void);

void arch_activate_breakpoints(void);
void arch_deactivate_breakpoints(void);

int arch_get_next_pc(void* addr, unsigned long* next, int* length);
int arch_remove_sw_break(void *addr);

int arch_set_hw_break(void *addr);
int arch_remove_hw_break(void *addr);

int arch_set_hw_watch(void *addr, size_t len, int mode);
int arch_remove_hw_watch(void *addr, size_t len, int mode);
int arch_hw_watch_get_amode(void *addr, uint32_t mode, void** hit_addr);

void arch_set_context(uint32_t context);
void arch_clear_context(void);

void arch_set_step(void* addr);
void arch_clear_step(void);
void arch_activate_step(void);
void arch_deactivate_step(void);

void arch_get_thread_regs(rt_thread_t thread, struct rt_hw_exp_stack *regs);
#endif

