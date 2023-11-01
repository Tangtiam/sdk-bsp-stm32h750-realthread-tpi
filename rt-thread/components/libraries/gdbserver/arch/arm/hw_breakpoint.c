/*
 * Copyright (c) 2011-2020, Shanghai Real-Thread Electronic Technology Co.,Ltd
 *
 * Change Logs:
 * Date           Author       Notes
 * 2020-02-23     Jesven         first version.
 */
#include <rtthread.h>
#include <gdbserver.h>
#include <dlmodule.h>
#include <string.h>
#include <board.h>
#include <cpuport.h>
#include "hw_breakpoint.h"

#define ARM_GP_REGS         16
#define ARM_FP_REGS         8
#define ARM_EXTRA_REGS      2
#define GDB_MAX_REGS        (ARM_GP_REGS + (ARM_FP_REGS * 3) + ARM_EXTRA_REGS)
#define NUMREGBYTES         (GDB_MAX_REGS << 2)

#define ARCH_MAX_HW_BREAKPOINTS    (16 - 2)
#define ARCH_MAX_HW_WATCHPOINTS   (16)

#define IS_THUMB_ADDR(addr)     ((addr) & 1)
#define MAKE_THUMB_ADDR(addr)   ((addr) | 1)
#define UNMAKE_THUMB_ADDR(addr) ((addr) & ~1)

struct gdb_bkpt
{
    void *bpt_addr;
    int  state;
    int  inst_len;
    union
    {
        uint32_t len_mask;
        uint32_t wcr;
        uint8_t saved_instr[BREAK_INSTR_SIZE];
    } u;
};

enum gdb_wp_type
{
    BP_WP_ERROR = 0,
    BP_WRITE_WATCHPOINT,
    BP_READ_WATCHPOINT,
    BP_ACCESS_WATCHPOINT,
};

enum gdb_bpstate
{
    BP_UNDEFINED = 0,
    BP_SET,
    BP_ACTIVE,
};

enum regnames
{
    GDB_R0,     /*0*/
    GDB_R1,     /*1*/
    GDB_R2,     /*2*/
    GDB_R3,     /*3*/
    GDB_R4,     /*4*/
    GDB_R5,     /*5*/
    GDB_R6,     /*6*/
    GDB_R7,     /*7*/
    GDB_R8,     /*8*/
    GDB_R9,     /*9*/
    GDB_R10,    /*10*/
    GDB_FP,     /*11*/
    GDB_IP,     /*12*/
    GDB_SP,    /*13*/
    GDB_LR,     /*14*/
    GDB_PC,     /*15*/
    GDB_CPSR = GDB_MAX_REGS - 1
};

static struct gdb_bkpt gdb_sw_breakpoint[GDB_MAX_BREAKPOINTS];
static struct gdb_bkpt gdb_hw_breakpoint[ARCH_MAX_HW_BREAKPOINTS];
static struct gdb_bkpt gdb_hw_watchpoint[ARCH_MAX_HW_WATCHPOINTS];

unsigned long bt(struct rt_hw_exp_stack *regs)
{
    unsigned long fp, lr;
    unsigned long ret = 0;

    fp = regs->fp;
    lr = regs->lr;

    while(1)
    {
        lr = *(unsigned long*)(fp);
        fp = *(unsigned long*)(fp - 4);

        if(lr >= HEAP_BEGIN)
        {
            ret = lr;
            break;
        }

        rt_kprintf("fp 0x%x, lr:0x%x\n", fp, lr);
    }

    return ret;
}

void gdb_get_register(unsigned long *gdb_regs)
{
    int regno;
    struct rt_hw_exp_stack *regs = gdb_get_arch_regs();

    for (regno = 0; regno < GDB_MAX_REGS; regno++)
    {
        gdb_regs[regno] = 0;
    }

    gdb_regs[GDB_R0]        = regs->r0;
    gdb_regs[GDB_R1]        = regs->r1;
    gdb_regs[GDB_R2]        = regs->r2;
    gdb_regs[GDB_R3]        = regs->r3;
    gdb_regs[GDB_R4]        = regs->r4;
    gdb_regs[GDB_R5]        = regs->r5;
    gdb_regs[GDB_R6]        = regs->r6;
    gdb_regs[GDB_R7]        = regs->r7;
    gdb_regs[GDB_R8]        = regs->r8;
    gdb_regs[GDB_R9]        = regs->r9;
    gdb_regs[GDB_R10]       = regs->r10;
    gdb_regs[GDB_FP]        = regs->fp;
    gdb_regs[GDB_IP]        = regs->ip;
    gdb_regs[GDB_SP]        = regs->sp;
    gdb_regs[GDB_LR]        = regs->lr;
    gdb_regs[GDB_PC]        = regs->pc;
    gdb_regs[GDB_CPSR]      = regs->cpsr;
};

void arch_get_stack_regs(void* sp, struct rt_hw_exp_stack *regs)
{
    struct rt_hw_stack* stack = (struct rt_hw_stack*)sp;

    regs->cpsr = stack->cpsr;
    regs->r0   = stack->r0;
    regs->r1   = stack->r1;
    regs->r2   = stack->r2;
    regs->r3   = stack->r3;
    regs->r4   = stack->r4;
    regs->r5   = stack->r5;
    regs->r6   = stack->r6;
    regs->r7   = stack->r7;
    regs->r8   = stack->r8;
    regs->r9   = stack->r9;
    regs->r10  = stack->r10;
    regs->fp   = stack->fp;
    regs->ip   = stack->ip;
    regs->lr   = stack->lr;
    regs->pc   = stack->pc;
    regs->sp   = (unsigned long)stack + sizeof(stack) + 4;
}
#define PS_N 0x80000000
#define PS_Z 0x40000000
#define PS_C 0x20000000
#define PS_V 0x10000000

#define ARM_GP_REGS         16
#define ARM_FP_REGS         8
#define ARM_EXTRA_REGS      2
#define GDB_MAX_REGS        (ARM_GP_REGS + (ARM_FP_REGS * 3) + ARM_EXTRA_REGS)
#define NUMREGBYTES         (GDB_MAX_REGS << 2)


int arch_check_debug_event(struct rt_hw_exp_stack *regs, unsigned long pc_adj)
{
    gdb_thread_info thread_info;
    uint32_t ifsr, dfar, dfsr;
    int ret;

    if (pc_adj == 4 || pc_adj == 2) /* pabt */
    {
        /* check breakpoint event */
        asm volatile ("MRC p15, 0, %0, c5, c0, 1":"=r"(ifsr));
        ifsr &= ((1UL << 12) | 0x3fUL); /* status */
        if (ifsr == 0x2UL)
        {
            /* is breakpoint event */

            /* is Thumb mode */
            if (regs->cpsr & 0x20)
            {
                if(pc_adj == 4)
                {
                    return 0;
                }

                regs->pc -= 2;
            }
            else
            {
                if(pc_adj == 2)
                {
                    return 0;
                }
                regs->pc -= pc_adj;
            }

            arch_deactivate_breakpoints();

            do {
                struct rt_dlmodule *dbg_module = gdb_get_dbg_module();
                struct rt_dlmodule *module;

                if (!dbg_module)
                {
                    break;
                }
                module = dlmodule_self();
                if (module == dbg_module)
                {
                    break;
                }
                *(uint32_t *)regs->pc = module->bak_first_ins;
                module->debug = 0;
                return 1;
            } while (0);

            thread_info.notify_type = GDB_NOTIFIY_BREAKPOINT;
            thread_info.abt_ins = *(uint32_t *)regs->pc;
            ret = 1;
        }
        else
        {
            return 0; /* not debug pabt */
        }
    }
    else
    {
        /* watchpoing event */
        asm volatile ("MRC p15, 0, %0, c5, c0, 0":"=r"(dfsr));
        dfsr = (((dfsr & (1UL << 10)) >> 6) | (dfsr & 0xfUL)); /* status */
        if (dfsr == 0x2UL)
        {
            /* is watchpoint event */
            regs->pc -= pc_adj;
            asm volatile ("MRC p15, 0, %0, c6, c0, 0":"=r"(dfar));
            thread_info.watch_addr = (void *)dfar;
            thread_info.rw = (1UL << (((~*(uint32_t *)regs->pc) >> 20) & 1UL));
            thread_info.notify_type = GDB_NOTIFIY_WATCHPOINT;
            ret =  2;
        }
        else
        {
            return 0; /* not debug dabt */
        }
    }
    thread_info.thread = rt_thread_self();
    thread_info.thread->regs = (rt_ubase_t)regs;
    rt_thread_suspend(thread_info.thread);
    rt_mq_send(gdb_get_server_mq(), (void *)&thread_info, sizeof(gdb_thread_info));

    return ret;
}

int arch_is_connect_breakpoint(uint32_t ins)
{
    if (ins == INS_BREAK_CONNECT)
    {
        return 1;
    }
    return 0;
}

int arch_is_software_breakpoint(uint32_t ins)
{
    if (ins == INS_BREAK_SOFTWARE)
    {
        return 1;
    }
    return 0;
}

#define __write_hw_dbg_reg(n, op2, v) case n: asm volatile ("MCR p14, 0, %0, c0, c" #n ", " #op2::"r"(v)); break

#define _write_hw_dbg_reg(n, op2, v)\
do {\
    switch (n) {\
        __write_hw_dbg_reg(0, op2, v);\
        __write_hw_dbg_reg(1, op2, v);\
        __write_hw_dbg_reg(2, op2, v);\
        __write_hw_dbg_reg(3, op2, v);\
        __write_hw_dbg_reg(4, op2, v);\
        __write_hw_dbg_reg(5, op2, v);\
        __write_hw_dbg_reg(6, op2, v);\
        __write_hw_dbg_reg(7, op2, v);\
        __write_hw_dbg_reg(8, op2, v);\
        __write_hw_dbg_reg(9, op2, v);\
        __write_hw_dbg_reg(10, op2, v);\
        __write_hw_dbg_reg(11, op2, v);\
        __write_hw_dbg_reg(12, op2, v);\
        __write_hw_dbg_reg(13, op2, v);\
        __write_hw_dbg_reg(14, op2, v);\
        __write_hw_dbg_reg(15, op2, v);\
    default:\
        break;\
    }\
} while (0)

#define __read_hw_dbg_reg(n, op2, v) case n: asm volatile ("MRC p14, 0, %0, c0, c" #n ", " #op2:"=r"(v)); break

#define _read_hw_dbg_reg(n, op2, v)\
do {\
    switch (n) {\
        __read_hw_dbg_reg(0, op2, v);\
        __read_hw_dbg_reg(1, op2, v);\
        __read_hw_dbg_reg(2, op2, v);\
        __read_hw_dbg_reg(3, op2, v);\
        __read_hw_dbg_reg(4, op2, v);\
        __read_hw_dbg_reg(5, op2, v);\
        __read_hw_dbg_reg(6, op2, v);\
        __read_hw_dbg_reg(7, op2, v);\
        __read_hw_dbg_reg(8, op2, v);\
        __read_hw_dbg_reg(9, op2, v);\
        __read_hw_dbg_reg(10, op2, v);\
        __read_hw_dbg_reg(11, op2, v);\
        __read_hw_dbg_reg(12, op2, v);\
        __read_hw_dbg_reg(13, op2, v);\
        __read_hw_dbg_reg(14, op2, v);\
        __read_hw_dbg_reg(15, op2, v);\
    default:\
        break;\
    }\
} while (0)

static uint32_t arch_read_hw_bcr_reg(int n)
{
    uint32_t v = 0;
    _read_hw_dbg_reg(n, 5, v);
    return v;
}

static void arch_write_hw_bvr_reg(int n, uint32_t v)
{
    _write_hw_dbg_reg(n, 4, v);
}

static void arch_write_hw_bcr_reg(int n, uint32_t v)
{
    _write_hw_dbg_reg(n, 5, v);
}

static void arch_write_hw_wvr_reg(int n, uint32_t v)
{
    _write_hw_dbg_reg(n, 6, v);
}

static void arch_write_hw_wcr_reg(int n, uint32_t v)
{
    _write_hw_dbg_reg(n, 7, v);
}

void unsed_fun(void)
{
    arch_write_hw_wvr_reg(0, 0);
    arch_write_hw_wcr_reg(0, 0);
}

static int arch_hw_bk_nr = 0;
static int arch_hw_wp_nr = 0;
static int arch_hw_wp_max_len = 0;
static int arch_hw_bk_step = 0;
static int arch_hw_bk_context = 0;

static const char support_str[] = "PacketSize=4160;swbreak+;hwbreak+;qXfer:features:read+";

static const char target_xml[] = "<?xml version=\"1.0\"?><!DOCTYPE target SYSTEM \"gdb-target.dtd\"><target><xi:include href=\"arm-core.xml\"/><xi:include href=\"arm-neon.xml\"/></target>";

static const char arm_core_xml[] = "\
<?xml version=\"1.0\"?>\
<!-- Copyright (C) 2008 Free Software Foundation, Inc.\
\
Copying and distribution of this file, with or without modification,\
        are permitted in any medium without royalty provided the copyright\
        notice and this notice are preserved.  -->\
\
        <!DOCTYPE feature SYSTEM \"gdb-target.dtd\">\
        <feature name=\"org.gnu.gdb.arm.core\">\
        <reg name=\"r0\" bitsize=\"32\"/>\
        <reg name=\"r1\" bitsize=\"32\"/>\
        <reg name=\"r2\" bitsize=\"32\"/>\
        <reg name=\"r3\" bitsize=\"32\"/>\
        <reg name=\"r4\" bitsize=\"32\"/>\
        <reg name=\"r5\" bitsize=\"32\"/>\
        <reg name=\"r6\" bitsize=\"32\"/>\
        <reg name=\"r7\" bitsize=\"32\"/>\
        <reg name=\"r8\" bitsize=\"32\"/>\
        <reg name=\"r9\" bitsize=\"32\"/>\
        <reg name=\"r10\" bitsize=\"32\"/>\
        <reg name=\"r11\" bitsize=\"32\"/>\
        <reg name=\"r12\" bitsize=\"32\"/>\
        <reg name=\"sp\" bitsize=\"32\" type=\"data_ptr\"/>\
        <reg name=\"lr\" bitsize=\"32\"/>\
        <reg name=\"pc\" bitsize=\"32\" type=\"code_ptr\"/>\
\
        <!-- The CPSR is register 25, rather than register 16, because\
        the FPA registers historically were placed between the PC\
        and the CPSR in the \"g\" packet.  -->\
        <reg name=\"cpsr\" bitsize=\"32\" regnum=\"16\"/>\
        </feature>";

static const char neon_xml[] = "\
<?xml version=\"1.0\"?>\
<!-- Copyright (C) 2008 Free Software Foundation, Inc.\
\
Copying and distribution of this file, with or without modification,\
        are permitted in any medium without royalty provided the copyright\
        notice and this notice are preserved.  -->\
        <!DOCTYPE feature SYSTEM \"gdb-target.dtd\">\
        <feature name=\"org.gnu.gdb.arm.vfp\">\
        <vector id=\"neon_uint8x8\" type=\"uint8\" count=\"8\"/>\
        <vector id=\"neon_uint16x4\" type=\"uint16\" count=\"4\"/>\
        <vector id=\"neon_uint32x2\" type=\"uint32\" count=\"2\"/>\
        <vector id=\"neon_float32x2\" type=\"ieee_single\" count=\"2\"/>\
        <union id=\"neon_d\">\
        <field name=\"u8\" type=\"neon_uint8x8\"/>\
        <field name=\"u16\" type=\"neon_uint16x4\"/>\
        <field name=\"u32\" type=\"neon_uint32x2\"/>\
        <field name=\"u64\" type=\"uint64\"/>\
        <field name=\"f32\" type=\"neon_float32x2\"/>\
        <field name=\"f64\" type=\"ieee_double\"/>\
        </union>\
        <vector id=\"neon_uint8x16\" type=\"uint8\" count=\"16\"/>\
        <vector id=\"neon_uint16x8\" type=\"uint16\" count=\"8\"/>\
        <vector id=\"neon_uint32x4\" type=\"uint32\" count=\"4\"/>\
        <vector id=\"neon_uint64x2\" type=\"uint64\" count=\"2\"/>\
        <vector id=\"neon_float32x4\" type=\"ieee_single\" count=\"4\"/>\
        <vector id=\"neon_float64x2\" type=\"ieee_double\" count=\"2\"/>\
        <union id=\"neon_q\">\
        <field name=\"u8\" type=\"neon_uint8x16\"/>\
        <field name=\"u16\" type=\"neon_uint16x8\"/>\
        <field name=\"u32\" type=\"neon_uint32x4\"/>\
        <field name=\"u64\" type=\"neon_uint64x2\"/>\
        <field name=\"f32\" type=\"neon_float32x4\"/>\
        <field name=\"f64\" type=\"neon_float64x2\"/>\
        </union>\
        <reg name=\"d0\" bitsize=\"64\" type=\"neon_d\"/>\
        <reg name=\"d1\" bitsize=\"64\" type=\"neon_d\"/>\
        <reg name=\"d2\" bitsize=\"64\" type=\"neon_d\"/>\
        <reg name=\"d3\" bitsize=\"64\" type=\"neon_d\"/>\
        <reg name=\"d4\" bitsize=\"64\" type=\"neon_d\"/>\
        <reg name=\"d5\" bitsize=\"64\" type=\"neon_d\"/>\
        <reg name=\"d6\" bitsize=\"64\" type=\"neon_d\"/>\
        <reg name=\"d7\" bitsize=\"64\" type=\"neon_d\"/>\
        <reg name=\"d8\" bitsize=\"64\" type=\"neon_d\"/>\
        <reg name=\"d9\" bitsize=\"64\" type=\"neon_d\"/>\
        <reg name=\"d10\" bitsize=\"64\" type=\"neon_d\"/>\
        <reg name=\"d11\" bitsize=\"64\" type=\"neon_d\"/>\
        <reg name=\"d12\" bitsize=\"64\" type=\"neon_d\"/>\
        <reg name=\"d13\" bitsize=\"64\" type=\"neon_d\"/>\
        <reg name=\"d14\" bitsize=\"64\" type=\"neon_d\"/>\
        <reg name=\"d15\" bitsize=\"64\" type=\"neon_d\"/>\
        <reg name=\"d16\" bitsize=\"64\" type=\"neon_d\"/>\
        <reg name=\"d17\" bitsize=\"64\" type=\"neon_d\"/>\
        <reg name=\"d18\" bitsize=\"64\" type=\"neon_d\"/>\
        <reg name=\"d19\" bitsize=\"64\" type=\"neon_d\"/>\
        <reg name=\"d20\" bitsize=\"64\" type=\"neon_d\"/>\
        <reg name=\"d21\" bitsize=\"64\" type=\"neon_d\"/>\
        <reg name=\"d22\" bitsize=\"64\" type=\"neon_d\"/>\
        <reg name=\"d23\" bitsize=\"64\" type=\"neon_d\"/>\
        <reg name=\"d24\" bitsize=\"64\" type=\"neon_d\"/>\
        <reg name=\"d25\" bitsize=\"64\" type=\"neon_d\"/>\
        <reg name=\"d26\" bitsize=\"64\" type=\"neon_d\"/>\
        <reg name=\"d27\" bitsize=\"64\" type=\"neon_d\"/>\
        <reg name=\"d28\" bitsize=\"64\" type=\"neon_d\"/>\
        <reg name=\"d29\" bitsize=\"64\" type=\"neon_d\"/>\
        <reg name=\"d30\" bitsize=\"64\" type=\"neon_d\"/>\
        <reg name=\"d31\" bitsize=\"64\" type=\"neon_d\"/>\
\
        <reg name=\"q0\" bitsize=\"128\" type=\"neon_q\"/>\
        <reg name=\"q1\" bitsize=\"128\" type=\"neon_q\"/>\
        <reg name=\"q2\" bitsize=\"128\" type=\"neon_q\"/>\
        <reg name=\"q3\" bitsize=\"128\" type=\"neon_q\"/>\
        <reg name=\"q4\" bitsize=\"128\" type=\"neon_q\"/>\
        <reg name=\"q5\" bitsize=\"128\" type=\"neon_q\"/>\
        <reg name=\"q6\" bitsize=\"128\" type=\"neon_q\"/>\
        <reg name=\"q7\" bitsize=\"128\" type=\"neon_q\"/>\
        <reg name=\"q8\" bitsize=\"128\" type=\"neon_q\"/>\
        <reg name=\"q9\" bitsize=\"128\" type=\"neon_q\"/>\
        <reg name=\"q10\" bitsize=\"128\" type=\"neon_q\"/>\
        <reg name=\"q10\" bitsize=\"128\" type=\"neon_q\"/>\
        <reg name=\"q12\" bitsize=\"128\" type=\"neon_q\"/>\
        <reg name=\"q13\" bitsize=\"128\" type=\"neon_q\"/>\
        <reg name=\"q14\" bitsize=\"128\" type=\"neon_q\"/>\
        <reg name=\"q15\" bitsize=\"128\" type=\"neon_q\"/>\
\
        <reg name=\"fpsid\" bitsize=\"32\" type=\"int\" group=\"float\"/>\
        <reg name=\"fpscr\" bitsize=\"32\" type=\"int\" group=\"float\"/>\
        <reg name=\"fpexc\" bitsize=\"32\" type=\"int\" group=\"float\"/>\
        </feature>";

int arch_cmd_query(char *cmd_buff, char *reply_buff)
{
    if (memcmp(cmd_buff, "qSupported:", sizeof("qSupported:") - 1) == 0)
    {
        strcpy(reply_buff, support_str);
        return 1;
    }
    else if (memcmp(cmd_buff, "qXfer:features:read:target.xml", sizeof("qXfer:features:read:target.xml") - 1) == 0)
    {
        reply_buff[0] = 'l';
        strcpy(reply_buff + 1, target_xml);
        return 1;
    }
    else if (memcmp(cmd_buff, "qXfer:features:read:arm-core.xml", sizeof("qXfer:features:read:arm-core.xml") - 1) == 0)
    {
        reply_buff[0] = 'l';
        strcpy(reply_buff + 1, arm_core_xml);
        return 1;
    }
    else if (memcmp(cmd_buff, "qXfer:features:read:arm-neon.xml:0,", sizeof("qXfer:features:read:arm-neon.xml:0,") - 1) == 0)
    {
        reply_buff[0] = 'm';
        strncpy(reply_buff + 1, neon_xml, 0xff0);
        return 1;
    }
    else if (memcmp(cmd_buff, "qXfer:features:read:arm-neon.xml:ff0", sizeof("qXfer:features:read:arm-neon.xml:ff0") - 1) == 0)
    {
        reply_buff[0] = 'l';
        strcpy(reply_buff + 1, neon_xml + 0xff0);
        return 1;
    }

    return 0;
}

/***************************************************************/

struct arch_reg_info reg_info[] = {
    {(size_t)&((struct rt_hw_exp_stack *)0)->r0, 4},
    {(size_t)&((struct rt_hw_exp_stack *)0)->r1, 4},
    {(size_t)&((struct rt_hw_exp_stack *)0)->r2, 4},
    {(size_t)&((struct rt_hw_exp_stack *)0)->r3, 4},
    {(size_t)&((struct rt_hw_exp_stack *)0)->r4, 4},
    {(size_t)&((struct rt_hw_exp_stack *)0)->r5, 4},
    {(size_t)&((struct rt_hw_exp_stack *)0)->r6, 4},
    {(size_t)&((struct rt_hw_exp_stack *)0)->r7, 4},
    {(size_t)&((struct rt_hw_exp_stack *)0)->r8, 4},
    {(size_t)&((struct rt_hw_exp_stack *)0)->r9, 4},
    {(size_t)&((struct rt_hw_exp_stack *)0)->r10, 4},
    {(size_t)&((struct rt_hw_exp_stack *)0)->fp, 4},
    {(size_t)&((struct rt_hw_exp_stack *)0)->ip, 4},
    {(size_t)&((struct rt_hw_exp_stack *)0)->sp, 4},
    {(size_t)&((struct rt_hw_exp_stack *)0)->lr, 4},
    {(size_t)&((struct rt_hw_exp_stack *)0)->pc, 4},
    {(size_t)&((struct rt_hw_exp_stack *)0)->cpsr, 4},
    {(size_t)ARCH_REG_OFFSET_INVALID , 8}, /* neon_d */
    {(size_t)ARCH_REG_OFFSET_INVALID , 8},
    {(size_t)ARCH_REG_OFFSET_INVALID , 8},
    {(size_t)ARCH_REG_OFFSET_INVALID , 8},
    {(size_t)ARCH_REG_OFFSET_INVALID , 8},
    {(size_t)ARCH_REG_OFFSET_INVALID , 8},
    {(size_t)ARCH_REG_OFFSET_INVALID , 8},
    {(size_t)ARCH_REG_OFFSET_INVALID , 8},
    {(size_t)ARCH_REG_OFFSET_INVALID , 8},
    {(size_t)ARCH_REG_OFFSET_INVALID , 8},
    {(size_t)ARCH_REG_OFFSET_INVALID , 8},
    {(size_t)ARCH_REG_OFFSET_INVALID , 8},
    {(size_t)ARCH_REG_OFFSET_INVALID , 8},
    {(size_t)ARCH_REG_OFFSET_INVALID , 8},
    {(size_t)ARCH_REG_OFFSET_INVALID , 8},
    {(size_t)ARCH_REG_OFFSET_INVALID , 8},
    {(size_t)ARCH_REG_OFFSET_INVALID , 8},
    {(size_t)ARCH_REG_OFFSET_INVALID , 8},
    {(size_t)ARCH_REG_OFFSET_INVALID , 8},
    {(size_t)ARCH_REG_OFFSET_INVALID , 8},
    {(size_t)ARCH_REG_OFFSET_INVALID , 8},
    {(size_t)ARCH_REG_OFFSET_INVALID , 8},
    {(size_t)ARCH_REG_OFFSET_INVALID , 8},
    {(size_t)ARCH_REG_OFFSET_INVALID , 8},
    {(size_t)ARCH_REG_OFFSET_INVALID , 8},
    {(size_t)ARCH_REG_OFFSET_INVALID , 8},
    {(size_t)ARCH_REG_OFFSET_INVALID , 8},
    {(size_t)ARCH_REG_OFFSET_INVALID , 8},
    {(size_t)ARCH_REG_OFFSET_INVALID , 8},
    {(size_t)ARCH_REG_OFFSET_INVALID , 8},
    {(size_t)ARCH_REG_OFFSET_INVALID , 8},
    {(size_t)ARCH_REG_OFFSET_INVALID , 8},
    {(size_t)ARCH_REG_OFFSET_INVALID /* &(struct rt_hw_exp_stack *)0)->fpu[0] */, 16}, /* neno_q */
    {(size_t)ARCH_REG_OFFSET_INVALID /* &(struct rt_hw_exp_stack *)0)->fpu[1] */, 16},
    {(size_t)ARCH_REG_OFFSET_INVALID /* &(struct rt_hw_exp_stack *)0)->fpu[2] */, 16},
    {(size_t)ARCH_REG_OFFSET_INVALID /* &(struct rt_hw_exp_stack *)0)->fpu[3] */, 16},
    {(size_t)ARCH_REG_OFFSET_INVALID /* &(struct rt_hw_exp_stack *)0)->fpu[4] */, 16},
    {(size_t)ARCH_REG_OFFSET_INVALID /* &(struct rt_hw_exp_stack *)0)->fpu[5] */, 16},
    {(size_t)ARCH_REG_OFFSET_INVALID /* &(struct rt_hw_exp_stack *)0)->fpu[6] */, 16},
    {(size_t)ARCH_REG_OFFSET_INVALID /* &(struct rt_hw_exp_stack *)0)->fpu[7] */, 16},
    {(size_t)ARCH_REG_OFFSET_INVALID /* &(struct rt_hw_exp_stack *)0)->fpu[8] */, 16},
    {(size_t)ARCH_REG_OFFSET_INVALID /* &(struct rt_hw_exp_stack *)0)->fpu[9] */, 16},
    {(size_t)ARCH_REG_OFFSET_INVALID /* &(struct rt_hw_exp_stack *)0)->fpu[10]*/, 16},
    {(size_t)ARCH_REG_OFFSET_INVALID /* &(struct rt_hw_exp_stack *)0)->fpu[11]*/, 16},
    {(size_t)ARCH_REG_OFFSET_INVALID /* &(struct rt_hw_exp_stack *)0)->fpu[12]*/, 16},
    {(size_t)ARCH_REG_OFFSET_INVALID /* &(struct rt_hw_exp_stack *)0)->fpu[13]*/, 16},
    {(size_t)ARCH_REG_OFFSET_INVALID /* &(struct rt_hw_exp_stack *)0)->fpu[14]*/, 16},
    {(size_t)ARCH_REG_OFFSET_INVALID /* &(struct rt_hw_exp_stack *)0)->fpu[15]*/, 16},
    {(size_t)ARCH_REG_OFFSET_INVALID /* &((struct rt_hw_exp_stack *)0)->fpsid */, 4},
    {(size_t)ARCH_REG_OFFSET_INVALID /* &((struct rt_hw_exp_stack *)0)->fpsr */, 4},
    {(size_t)ARCH_REG_OFFSET_INVALID /* &((struct rt_hw_exp_stack *)0)->fpcr */, 4},
};

void arch_reg_check(struct rt_hw_exp_stack *reg, size_t no, size_t *v)
{
    size_t old_cpsr, new_cpsr;

    if (no == 16) /* cpsr */
    {
        old_cpsr = *(size_t *)((char *)reg + (size_t)&((struct rt_hw_exp_stack *)0)->cpsr);
        new_cpsr = *v;
        old_cpsr &= ~((0x1fUL << 27) | (0xfUL << 16));
        new_cpsr &= ((0x1fUL << 27) | (0xfUL << 16));
        *v = (new_cpsr | old_cpsr);
    }
}
/***************************************************************/

static void arch_hw_breakpoint_init(void)
{
    int i;
    uint32_t v;
    uint32_t dbgdidr;

    asm volatile ("MRC p14, 0, %0, c0, c0, 0":"=r"(dbgdidr));
    arch_hw_wp_nr = (dbgdidr >> 28) + 1;
    arch_hw_bk_nr = ((dbgdidr >> 24) & 0xfUL) + 1 - 2; //-2 for context id and step

    arch_hw_bk_step = arch_hw_bk_nr;
    arch_hw_bk_context = arch_hw_bk_step + 1;

    v = (0xffUL << 5);
    arch_write_hw_wcr_reg(0, v);
    /* wcr0 */
    asm volatile ("MRC p14, 0, %0, c0, c0, 7":"=r"(v));
    if (v == (0xffUL << 5))
    {
        arch_hw_wp_max_len = 8;
    }
    else
    {
        arch_hw_wp_max_len = 4;
    }

    v = 0;
    /* wp */
    for (i = 0; i < arch_hw_wp_nr; i++)
    {
        arch_write_hw_wvr_reg(i, v);
        arch_write_hw_wcr_reg(i, v);
    }
    /* bp */
    for (i = 0; i < arch_hw_bk_nr; i++)
    {
        arch_write_hw_bvr_reg(i, v);
        arch_write_hw_bcr_reg(i, v);
    }

    arch_write_hw_bvr_reg(arch_hw_bk_step, v);
    arch_write_hw_bcr_reg(arch_hw_bk_step, v);

    arch_write_hw_bvr_reg(arch_hw_bk_context, v);
    v  = (0x0UL << 24);
    v |= (0x3UL << 20);
    v |= (0x0UL << 16);
    v |= (0xfUL << 5);
    v |= (0x3UL << 1);
    v |= (0x1UL << 0);
    arch_write_hw_bcr_reg(arch_hw_bk_context, v);

    rt_hw_isb();

    /* manager */
    for (i = 0; i < arch_hw_wp_nr; i++)
    {
        gdb_hw_breakpoint[i].state = BP_UNDEFINED;
    }
}

/********************* hw_breakpoint *********************/

static void hw_breakpoint_init(void)
{
    int i;

    for (i = 0; i < arch_hw_bk_nr; i++)
    {
        gdb_hw_breakpoint[i].state = BP_UNDEFINED;
    }
}

static void arch_activate_hw_breakpoint(int idx)
{
    uint32_t v;

    if (gdb_hw_breakpoint[idx].state == BP_SET)
    {
        arch_write_hw_bvr_reg(idx, (uint32_t)(size_t)gdb_hw_breakpoint[idx].bpt_addr);
        v =  (0x0UL << 24);
        v |= (0x1UL << 20);
        v |= (arch_hw_bk_context << 16);
        v |= (0xfUL << 5);
        v |= (0x2UL << 1);
        v |= (0x1UL << 0);
        arch_write_hw_bcr_reg(idx, v);
        gdb_hw_breakpoint[idx].state = BP_ACTIVE;
        rt_hw_isb();
    }
}

static void arch_deactivate_hw_breakpoint(int idx)
{
    uint32_t v = 0;

    if (gdb_hw_breakpoint[idx].state == BP_ACTIVE)
    {
        arch_write_hw_bcr_reg(idx, v);
        gdb_hw_breakpoint[idx].state = BP_SET;
        rt_hw_isb();
    }
}

static void arch_activate_hw_breakpoints(void)
{
    int i;

    for (i = 0; i < arch_hw_bk_nr; i++)
    {
        arch_activate_hw_breakpoint(i);
    }
}

static void arch_deactivate_hw_breakpoints(void)
{
    int i;

    for (i = 0; i < arch_hw_bk_nr; i++)
    {
        arch_deactivate_hw_breakpoint(i);
    }
}

int arch_set_hw_break(void *addr)
{
    int i;

    for (i = 0; i < arch_hw_bk_nr; i++)
    {
        if (gdb_hw_breakpoint[i].state == BP_UNDEFINED)
        {
            gdb_hw_breakpoint[i].bpt_addr = addr;
            gdb_hw_breakpoint[i].state = BP_SET;
            return 0;
        }
    }
    return -1;
}

int arch_remove_hw_break(void *addr)
{
    int i;

    for (i = 0; i < arch_hw_bk_nr; i++)
    {
        /* Now, the state must be BP_SET or BP_UNDEFINED */
        if (gdb_hw_breakpoint[i].state != BP_SET)
            continue;

        if (gdb_hw_breakpoint[i].bpt_addr != addr)
            continue;

        /* Found */
        gdb_hw_breakpoint[i].state = BP_UNDEFINED;
        return 0;
    }
    return -1;
}

/********************* hw_watchpoint *********************/

static void hw_watchpoint_init(void)
{
    int i;

    for (i = 0; i < arch_hw_wp_nr; i++)
    {
        gdb_hw_watchpoint[i].state = BP_UNDEFINED;
    }
}

static void arch_activate_hw_watchpoint(int idx)
{
    if (gdb_hw_watchpoint[idx].state == BP_SET)
    {
        arch_write_hw_wvr_reg(idx, (uint32_t)(size_t)gdb_hw_watchpoint[idx].bpt_addr);
        arch_write_hw_wcr_reg(idx, (uint32_t)(size_t)gdb_hw_watchpoint[idx].u.wcr);
        gdb_hw_watchpoint[idx].state = BP_ACTIVE;
        rt_hw_isb();
    }
}

static void arch_deactivate_hw_watchpoint(int idx)
{
    uint32_t v = 0;

    if (gdb_hw_watchpoint[idx].state == BP_ACTIVE)
    {
        arch_write_hw_wcr_reg(idx, v);
        gdb_hw_watchpoint[idx].state = BP_SET;
        rt_hw_isb();
    }
}

static void arch_activate_hw_watchpoints(void)
{
    int i;

    for (i = 0; i < arch_hw_wp_nr; i++)
    {
        arch_activate_hw_watchpoint(i);
    }
}

static void arch_deactivate_hw_watchpoints(void)
{
    int i;

    for (i = 0; i < arch_hw_wp_nr; i++)
    {
        arch_deactivate_hw_watchpoint(i);
    }
}

static int watchpoint_get_cv(void *addr, size_t len, int mode, uint32_t *wcr, void **wvr)
{
    uint32_t wcr_value = 0;
    size_t len_mask;
    size_t wvr_value = (size_t)addr;
    size_t offset;
    size_t align_mask = 0x3UL;
    int type = BP_WP_ERROR;

    if (len > arch_hw_wp_max_len)
    {
        return type;
    }
    if (len == 8)
    {
        align_mask = 0x7UL;
    }
    offset = wvr_value & align_mask;
    wvr_value &= ~align_mask;
    len_mask = (1UL << len) - 1;
    switch (offset)
    {
        case 0: //aligned
            break;
        case 1:
        case 2:
            if (len_mask == 0x3UL)
                break;
        case 3:
            if (len_mask == 0x1UL)
                break;
        default:
            return type;
    }
    len_mask <<= offset;

    wcr_value |= (len_mask << 5);

    switch (mode)
    {
        case WATCHPOINT_MODE_WRITE:
            wcr_value |= (0x2UL << 3);
            type = BP_WRITE_WATCHPOINT;
            break;
        case WATCHPOINT_MODE_READ:
            wcr_value |= (0x1UL << 3);
            type = BP_READ_WATCHPOINT;
            break;
        case WATCHPOINT_MODE_ACCESS:
            wcr_value |= (0x3UL << 3);
            type = BP_ACCESS_WATCHPOINT;
            break;
        default:
            return type;
    }
    wcr_value |= (arch_hw_bk_context << 16);
    wcr_value |= (0x1UL << 20);
    wcr_value |= (0x2UL << 1);
    wcr_value |= (0x1UL << 0);
    *wcr = wcr_value;
    *wvr = (void*)wvr_value;
    return type;
}

int arch_set_hw_watch(void *addr, size_t len, int mode)
{
    int i;
    uint32_t wcr;
    void* wvr;

    if (watchpoint_get_cv(addr, len, mode, &wcr, &wvr) == BP_WP_ERROR)
    {
        return -1;
    }
    for (i = 0; i < arch_hw_wp_nr; i++)
    {
        if (gdb_hw_watchpoint[i].state == BP_UNDEFINED)
        {
            gdb_hw_watchpoint[i].bpt_addr = wvr;
            gdb_hw_watchpoint[i].u.wcr = wcr;
            gdb_hw_watchpoint[i].state = BP_SET;
            return 0;
        }
    }
    return -1;
}

int arch_remove_hw_watch(void *addr, size_t len, int mode)
{
    int i;
    uint32_t wcr;
    void* wvr;

    if (watchpoint_get_cv(addr, len, mode, &wcr, &wvr) == BP_WP_ERROR)
    {
        return -1;
    }
    for (i = 0; i < arch_hw_wp_nr; i++)
    {
        /* Now, the state must be BP_SET or BP_UNDEFINED */
        if (gdb_hw_watchpoint[i].state != BP_SET)
            continue;

        if (gdb_hw_watchpoint[i].bpt_addr != wvr)
            continue;

        if (gdb_hw_watchpoint[i].u.wcr != wcr)
            continue;

        /* Found */
        gdb_hw_watchpoint[i].state = BP_UNDEFINED;
        return 0;
    }
    return -1;
}

int arch_hw_watch_get_amode(void *addr, uint32_t mode, void** hit_addr)
{
    int i, pass;
    uint32_t wcr;
    uint32_t rw;
    uint32_t offset_start;
    int type;

    for (pass = 0; pass < 2; pass++)
    {
        for (i = 0; i < arch_hw_wp_nr; i++)
        {
            uint32_t len_mask, align_mask;

            /* Now, the state must be BP_SET or BP_UNDEFINED */
            if (gdb_hw_watchpoint[i].state != BP_SET)
                continue;

            wcr = gdb_hw_watchpoint[i].u.wcr;

            rw = wcr >> 3;
            rw &= 0x3UL;
            if ((rw & mode) == 0)
            {
                continue;
            }
            len_mask = (wcr >> 5) & 0xffUL;

            if (len_mask > 0xfUL)
            {
                align_mask = 0x7UL;
            }
            else
            {
                align_mask = 0x3UL;
            }

            if (pass == 0)
            {
                if (!((1UL << ((size_t)addr & align_mask)) & len_mask))
                {
                    continue;
                }
            }
            else
            {
                if (((size_t)addr & ~align_mask) != ((size_t)gdb_hw_watchpoint[i].bpt_addr & ~align_mask))
                {
                    continue;
                }
                len_mask ^= -len_mask;
                len_mask = ~len_mask;
                asm volatile("clz %0, %1":"=r"(offset_start):"r"(len_mask));
                offset_start = 31 - offset_start;
                addr += offset_start;
            }

            switch (rw)
            {
                case 1UL:
                    type = WATCHPOINT_MODE_READ;
                    break;
                case 2UL:
                    type = WATCHPOINT_MODE_WRITE;
                    break;
                case 3UL:
                    type = WATCHPOINT_MODE_ACCESS;
                    break;
                default:
                    return -1;
            }
            *hit_addr = addr;
            return type;
        }
    }
    return -1;
}

/********************* context *********************/

void arch_set_context(uint32_t context)
{
    /* enable context cmp*/
    arch_write_hw_bvr_reg(arch_hw_bk_context, (context << 8));
    rt_hw_isb();
}

void arch_clear_context(void)
{
    uint32_t v = 0;

    arch_write_hw_bvr_reg(arch_hw_bk_context, v);
    rt_hw_isb();
}

/********************* sw_breakpoint *********************/

static void sw_breakpoint_init(void)
{
    int i;

    for (i = 0; i < GDB_MAX_BREAKPOINTS; i++)
    {
        gdb_sw_breakpoint[i].state = BP_UNDEFINED;
    }
}

static void arch_activate_sw_breakpoint(int idx)
{
    if (gdb_sw_breakpoint[idx].state == BP_SET)
    {
        void* addr;
        uint32_t ins = INS_BREAK_SOFTWARE;

        addr = gdb_sw_breakpoint[idx].bpt_addr;
        memcpy(addr, &ins, gdb_sw_breakpoint[idx].inst_len);
        gdb_sw_breakpoint[idx].state = BP_ACTIVE;
    }
}

static void arch_deactivate_sw_breakpoint(int idx)
{
    if (gdb_sw_breakpoint[idx].state == BP_ACTIVE)
    {
        void* addr;

        addr = gdb_sw_breakpoint[idx].bpt_addr;
        memcpy(addr, gdb_sw_breakpoint[idx].u.saved_instr, gdb_sw_breakpoint[idx].inst_len);
        gdb_sw_breakpoint[idx].state = BP_SET;
    }
}

static void arch_activate_sw_breakpoints(void)
{
    int i;

    for (i = 0; i < GDB_MAX_BREAKPOINTS; i++)
    {
        arch_activate_sw_breakpoint(i);
    }
}

static void arch_deactivate_sw_breakpoints(void)
{
    int i;

    for (i = 0; i < GDB_MAX_BREAKPOINTS; i++)
    {
        arch_deactivate_sw_breakpoint(i);
    }
}

int arch_set_sw_break(void *addr, int length)
{
    int i;
    unsigned long fixed_addr;
    int break_instr_size = BREAK_INSTR_SIZE;

    if (IS_THUMB_ADDR((unsigned long)addr))
    {
        fixed_addr = UNMAKE_THUMB_ADDR((unsigned long)addr);
        break_instr_size = 2;
    }
    else
    {
        fixed_addr = addr;
    }

    if (length != 0)
    {
        break_instr_size = length;
    }

    rt_kprintf("arch_set_sw_break 0x%x, length %d\n", fixed_addr, break_instr_size);
    for (i = 0; i < GDB_MAX_BREAKPOINTS; i++)
    {
        if (gdb_sw_breakpoint[i].state == BP_UNDEFINED)
        {
            memcpy((void*)gdb_sw_breakpoint[i].u.saved_instr, (void*)fixed_addr, break_instr_size);
            gdb_sw_breakpoint[i].bpt_addr = fixed_addr;
            gdb_sw_breakpoint[i].state = BP_SET;
            gdb_sw_breakpoint[i].inst_len = break_instr_size;
            return 0;
        }
    }
    return -1;
}

int arch_remove_sw_break(void *addr)
{
    int i;
    unsigned long fixed_addr;

    if (IS_THUMB_ADDR((unsigned long)addr))
    {
        fixed_addr = UNMAKE_THUMB_ADDR((unsigned long)addr);
    }
    else
    {
        fixed_addr = (unsigned long)addr;
    }

    for (i = 0; i < GDB_MAX_BREAKPOINTS; i++)
    {
        /* Now, the state must be BP_SET or BP_UNDEFINED */
        if (gdb_sw_breakpoint[i].state != BP_SET)
            continue;

        if (gdb_sw_breakpoint[i].bpt_addr != fixed_addr)
            continue;

        /* Found */
        gdb_sw_breakpoint[i].state = BP_UNDEFINED;
        return 0;
    }
    return -1;
}
/******************************************/

void arch_activate_breakpoints(void)
{
    arch_activate_sw_breakpoints();
    arch_activate_hw_breakpoints();
    arch_activate_hw_watchpoints();
}

void arch_deactivate_breakpoints(void)
{
    arch_deactivate_sw_breakpoints();
    arch_deactivate_hw_breakpoints();
    arch_deactivate_hw_watchpoints();
}

void arch_remove_breakpoints(void)
{
    sw_breakpoint_init();
    hw_breakpoint_init();
    hw_watchpoint_init();
}

/******************************************/

void arch_set_step(void* addr)
{
    uint32_t v;

    rt_kprintf("arch_set_step 0x%x\n", addr);
    arch_write_hw_bvr_reg(arch_hw_bk_step, (uint32_t)((size_t)addr & ~3UL));
    v =  (0x0UL << 24);
    v |= (0x5UL << 20);
    v |= (arch_hw_bk_context << 16);
    v |= (0xfUL << 5);
    v |= (0x2UL << 1);
    v |= (0x0UL << 0); /* is not activated */
    arch_write_hw_bcr_reg(arch_hw_bk_step, v);
    rt_hw_isb();
}

void arch_activate_step(void)
{
    uint32_t v;
    rt_kprintf("arch_activate_step\n");
    v = arch_read_hw_bcr_reg(arch_hw_bk_step);
    v |= 1UL;
    arch_write_hw_bcr_reg(arch_hw_bk_step, v);
    rt_hw_isb();
}

void arch_deactivate_step(void)
{
    uint32_t v;
    rt_kprintf("arch_deactivate_step\n");
    v = arch_read_hw_bcr_reg(arch_hw_bk_step);
    v &= ~1UL;
    arch_write_hw_bcr_reg(arch_hw_bk_step, v);
    rt_hw_isb();
}

void arch_clear_step(void)
{
    uint32_t v = 0;
    rt_kprintf("arch_clear_step\n");
    arch_write_hw_bcr_reg(arch_hw_bk_step, v);
    rt_hw_isb();
}

/******************************************/

void arch_breakpoint_init(void)
{
    sw_breakpoint_init();

    arch_hw_breakpoint_init();
    hw_breakpoint_init();
    hw_watchpoint_init();
}

void arch_enable_debug(void)
{
    uint32_t v;

    arch_breakpoint_init();

    //clear lock
    asm volatile ("MRC p14, 0, %0, c1, c1, 4":"=r"(v));
    v = ~0xC5ACCE55UL;
    asm volatile ("MCR p14, 0, %0, c1, c0, 4"::"r"(v));

    //clear vcr
    v = 0;
    asm volatile ("MCR p14, 0, %0, c0, c7, 0"::"r"(v));

    rt_hw_isb();

    asm volatile ("MRC p14, 0, %0, c0, c1, 0":"=r"(v));
    v |= (1UL << 15); //MDBGen enable
    asm volatile ("MCR p14, 0, %0, c0, c2, 2"::"r"(v));

    rt_hw_isb();

#ifdef ARCH_ARM_SECURE_MODE
    asm volatile ("mrc p15, 0, %0, c1, c1, 1":"=r"(v));
    v |= 1UL; //Secure debug enable
    asm volatile ("mcr p15, 0, %0, c1, c1, 1"::"r"(v));

    rt_hw_isb();
#endif

    v = 1;
    asm volatile ("MCR p15, 0, %0, c13, c0, 3"::"r"(v));

    rt_hw_isb();

    v = 1;
    asm volatile ("MCR p15, 0, %0, c13, c0, 1"::"r"(v));

    rt_hw_isb();

    arch_set_context(1);
}

void show_regs(struct rt_hw_exp_stack *regs)
{
    rt_kprintf("gdbserver:\n");
    rt_kprintf("r00:0x%08x r01:0x%08x r02:0x%08x r03:0x%08x\n", regs->r0, regs->r1, regs->r2, regs->r3);
    rt_kprintf("r04:0x%08x r05:0x%08x r06:0x%08x r07:0x%08x\n", regs->r4, regs->r5, regs->r6, regs->r7);
    rt_kprintf("r08:0x%08x r09:0x%08x r10:0x%08x\n", regs->r8, regs->r9, regs->r10);
    rt_kprintf("fp :0x%08x ip :0x%08x\n", regs->fp, regs->ip);
    rt_kprintf("sp :0x%08x lr :0x%08x pc :0x%08x\n", regs->sp, regs->lr, regs->pc);
    rt_kprintf("cpsr:0x%08x\n", regs->cpsr);
}

/******************************************/

#define PS_N 0x80000000
#define PS_Z 0x40000000
#define PS_C 0x20000000
#define PS_V 0x10000000

static int arm_ins_will_execute(unsigned long ins);
static int thumb_ins_will_execute(unsigned long ins);
static unsigned long arm_target_ins(unsigned long *pc, unsigned long ins);
static unsigned long thumb_target_ins(unsigned short *pc, unsigned short ins);
static int thumb_32bit_code(uint16_t ic);

static unsigned long gdb_arch_regs[GDB_MAX_REGS];
static int arm_ins_will_execute(unsigned long ins)
{
    struct rt_hw_exp_stack* regs;
    int res = 0;
    unsigned long psr;
    regs = gdb_get_arch_regs();
    psr = regs->cpsr;

    /* condition codes */
    switch ((ins & 0xF0000000) >> 28)
    {
        case 0x0: /* EQ */
            res = (psr & PS_Z) != 0;
            break;
        case 0x1: /* NE */
            res = (psr & PS_Z) == 0;
            break;
        case 0x2: /* CS */
            res = (psr & PS_C) != 0;
            break;
        case 0x3: /* CC */
            res = (psr & PS_C) == 0;
            break;
        case 0x4: /* MI */
            res = (psr & PS_N) != 0;
            break;
        case 0x5: /* PL */
            res = (psr & PS_N) == 0;
            break;
        case 0x6: /* VS */
            res = (psr & PS_V) != 0;
            break;
        case 0x7: /* VC */
            res = (psr & PS_V) == 0;
            break;
        case 0x8: /* HI */
            res = ((psr & PS_C) != 0) && ((psr & PS_Z) == 0);
            break;
        case 0x9: /* LS */
            res = ((psr & PS_C) == 0) || ((psr & PS_Z) != 0);
            break;
        case 0xA: /* GE */
            res = ((psr & (PS_N | PS_V)) == (PS_N | PS_V)) ||
                  ((psr & (PS_N | PS_V)) == 0);
            break;
        case 0xB: /* LT */
            res = ((psr & (PS_N | PS_V)) == PS_N) ||
                  ((psr & (PS_N | PS_V)) == PS_V);
            break;
        case 0xC: /* GT */
            res = ((psr & (PS_N | PS_V)) == (PS_N | PS_V)) ||
                  ((psr & (PS_N | PS_V)) == 0);
            res = ((psr & PS_Z) == 0) && res;
            break;
        case 0xD: /* LE */
            res = ((psr & (PS_N | PS_V)) == PS_N) ||
                  ((psr & (PS_N | PS_V)) == PS_V);
            res = ((psr & PS_Z) == PS_Z) || res;
            break;
        case 0xE: /* AL */
            res = 1;
            break;
        case 0xF: /* NV */
            if ((((ins & 0x0E000000) >> 24) == 0xA) || (((ins & 0x0E000000) >> 24) == 0xB))
            {
                res = 1;
            }
            else
            {
                res = 0;
            }
            break;
    }
    return res;
}

static int thumb_ins_will_execute(unsigned long ins)
{
    struct rt_hw_exp_stack* regs;
    int res = 0;
    unsigned long psr;
    regs = gdb_get_arch_regs();
    psr = regs->cpsr;

    if (((ins & 0xF000) >> 12) != 0xD)
    {
        return 1;
    }
    /* condition codes */
    switch ((ins & 0x0F00) >> 8)
    {
        case 0x0: /* EQ */
            res = (psr & PS_Z) != 0;
            break;
        case 0x1: /* NE */
            res = (psr & PS_Z) == 0;
            break;
        case 0x2: /* CS */
            res = (psr & PS_C) != 0;
            break;
        case 0x3: /* CC */
            res = (psr & PS_C) == 0;
            break;
        case 0x4: /* MI */
            res = (psr & PS_N) != 0;
            break;
        case 0x5: /* PL */
            res = (psr & PS_N) == 0;
            break;
        case 0x6: /* VS */
            res = (psr & PS_V) != 0;
            break;
        case 0x7: /* VC */
            res = (psr & PS_V) == 0;
            break;
        case 0x8: /* HI */
            res = ((psr & PS_C) != 0) && ((psr & PS_Z) == 0);
            break;
        case 0x9: /* LS */
            res = ((psr & PS_C) == 0) || ((psr & PS_Z) != 0);
            break;
        case 0xA: /* GE */
            res = ((psr & (PS_N | PS_V)) == (PS_N | PS_V)) ||
                  ((psr & (PS_N | PS_V)) == 0);
            break;
        case 0xB: /* LT */
            res = ((psr & (PS_N | PS_V)) == PS_N) ||
                  ((psr & (PS_N | PS_V)) == PS_V);
            break;
        case 0xC: /* GT */
            res = ((psr & (PS_N | PS_V)) == (PS_N | PS_V)) ||
                  ((psr & (PS_N | PS_V)) == 0);
            res = ((psr & PS_Z) == 0) && res;
            break;
        case 0xD: /* LE */
            res = ((psr & (PS_N | PS_V)) == PS_N) ||
                  ((psr & (PS_N | PS_V)) == PS_V);
            res = ((psr & PS_Z) == PS_Z) || res;
            break;
        case 0xE: /* AL */
            res = 1;
            break;
        case 0xF: /* NV */
            /* TODO what is NV condition for thumb state. */
            res = 1;
            break;
    }
    return res;
}

static unsigned long RmShifted(int shift)
{
    unsigned long Rm = gdb_arch_regs[shift & 0x00F];
    int shift_count;
    if ((shift & 0x010) == 0)
    {
        shift_count = (shift & 0xF80) >> 7;
    }
    else
    {
        shift_count = gdb_arch_regs[(shift & 0xF00) >> 8];
    }
    switch ((shift & 0x060) >> 5)
    {
        case 0x0: /* Logical left */
            Rm <<= shift_count;
            break;
        case 0x1: /* Logical right */
            Rm >>= shift_count;
            break;
        case 0x2: /* Arithmetic right */
            Rm = (unsigned long)((long)Rm >> shift_count);
            break;
        case 0x3: /* Rotate right */
            if (shift_count == 0)
            {
                /* Special case, RORx */
                Rm >>= 1;
                if (gdb_arch_regs[GDB_CPSR] & PS_C)
                {
                    Rm |= 0x80000000;
                }
            }
            else
            {
                Rm = (Rm >> shift_count) | (Rm << (32 - shift_count));
            }
            break;
    }
    return Rm;
}
/*
 * @param pc: the current program counter pointer
 * @param ins: the instruction value
 * Decide the next instruction to be executed for a given instruction
 */
unsigned long thumb_decoder(unsigned short *pc, unsigned short ins);
unsigned long thumb_get_next_pc(unsigned short *pc, unsigned short ins);
static unsigned long thumb_target_ins(unsigned short *pc, unsigned short ins)
{
    return thumb_get_next_pc(pc, ins);
}

/*
 * @param pc: the current program counter pointer
 * @param ins: the instruction value
 * Decide the next instruction to be executed for a given instruction
 */
static unsigned long arm_target_ins(unsigned long *pc, unsigned long ins)
{
    unsigned long new_pc, offset, op2;
    unsigned long Rn;
    int i, reg_count, c;

    gdb_get_register(gdb_arch_regs);

    switch ((ins & 0x0C000000) >> 26)
    {
        case 0x0:
            /* BX or BLX */
            if ((ins & 0x0FFFFFD0) == 0x012FFF10)
            {
                new_pc = (unsigned long)gdb_arch_regs[ins & 0x0000000F];
                return new_pc;
            }
            /* Data processing */
            new_pc = (unsigned long)(pc + 1);
            if ((ins & 0x0000F000) == 0x0000F000)
            {
                /* Destination register is PC */
                if ((ins & 0x0FBF0000) != 0x010F0000)
                {
                    Rn = (unsigned long)gdb_arch_regs[(ins & 0x000F0000) >> 16];
                    if ((ins & 0x000F0000) == 0x000F0000)
                    {
                        Rn += 8;    /* PC prefetch! */
                    }
                    if ((ins & 0x02000000) == 0)
                    {
                        op2 = RmShifted(ins & 0x00000FFF);
                    }
                    else
                    {
                        op2 = ins & 0x000000FF;
                        i = (ins & 0x00000F00) >> 8;  /* Rotate count */
                        op2 = (op2 >> (i * 2)) | (op2 << (32 - (i * 2)));
                    }
                    switch ((ins & 0x01E00000) >> 21)
                    {
                        case 0x0: /* AND */
                            new_pc = Rn & op2;
                            break;
                        case 0x1: /* EOR */
                            new_pc = Rn ^ op2;
                            break;
                        case 0x2: /* SUB */
                            new_pc = Rn - op2;
                            break;
                        case 0x3: /* RSB */
                            new_pc = op2 - Rn;
                            break;
                        case 0x4: /* ADD */
                            new_pc = Rn + op2;
                            break;
                        case 0x5: /* ADC */
                            c = (gdb_arch_regs[GDB_CPSR] & PS_C) != 0;
                            new_pc = Rn + op2 + c;
                            break;
                        case 0x6: /* SBC */
                            c = (gdb_arch_regs[GDB_CPSR] & PS_C) != 0;
                            new_pc = Rn - op2 + c - 1;
                            break;
                        case 0x7: // RSC
                            c = (gdb_arch_regs[GDB_CPSR] & PS_C) != 0;
                            new_pc = op2 - Rn + c - 1;
                            break;
                        case 0x8: /* TST */
                        case 0x9: /* TEQ */
                        case 0xA: /* CMP */
                        case 0xB: /* CMN */
                            break; /* PC doesn't change */
                        case 0xC: /* ORR */
                            new_pc = Rn | op2;
                            break;
                        case 0xD: /* MOV */
                            new_pc = op2;
                            break;
                        case 0xE: /* BIC */
                            new_pc = Rn & ~op2;
                            break;
                        case 0xF: /* MVN */
                            new_pc = ~op2;
                            break;
                    }
                }
            }
            return new_pc;
        case 0x1:
            if ((ins & 0x02000010) == 0x02000010)
            {
                /* Undefined! */
                return (unsigned long)(pc + 1);
            }
            else
            {
                if ((ins & 0x00100000) == 0)
                {
                    /* STR */
                    return (unsigned long)(pc + 1);
                }
                else
                {
                    /* LDR */
                    if ((ins & 0x0000F000) != 0x0000F000)
                    {
                        /* Rd not PC */
                        return (unsigned long)(pc + 1);
                    }
                    else
                    {
                        Rn = (unsigned long)gdb_arch_regs[(ins & 0x000F0000) >> 16];
                        if ((ins & 0x000F0000) == 0x000F0000)
                        {
                            Rn += 8;    /* PC prefetch! */
                        }
                        if (ins & 0x01000000)
                        {
                            /* Add/subtract offset before */
                            if ((ins & 0x02000000) == 0)
                            {
                                /* Immediate offset */
                                if (ins & 0x00800000)
                                {
                                    /* Add offset */
                                    Rn += (ins & 0x00000FFF);
                                }
                                else
                                {
                                    /* Subtract offset */
                                    Rn -= (ins & 0x00000FFF);
                                }
                            }
                            else
                            {
                                /* Offset is in a register */
                                if (ins & 0x00800000)
                                {
                                    /* Add offset */
                                    Rn += RmShifted(ins & 0x00000FFF);
                                }
                                else
                                {
                                    /* Subtract offset */
                                    Rn -= RmShifted(ins & 0x00000FFF);
                                }
                            }
                        }
                        return *(unsigned long *)Rn;
                    }
                }
            }
            return (unsigned long)(pc + 1);
        case 0x2:  /* Branch, LDM/STM */
            if ((ins & 0x02000000) == 0)
            {
                /* LDM/STM */
                if ((ins & 0x00100000) == 0)
                {
                    /* STM */
                    return (unsigned long)(pc + 1);
                }
                else
                {
                    /* LDM */
                    if ((ins & 0x00008000) == 0)
                    {
                        /* PC not in list */
                        return (unsigned long)(pc + 1);
                    }
                    else
                    {
                        Rn = (unsigned long)gdb_arch_regs[(ins & 0x000F0000) >> 16];
                        if ((ins & 0x000F0000) == 0x000F0000)
                        {
                            Rn += 8;    /* PC prefetch! */
                        }
                        offset = ins & 0x0000FFFF;
                        reg_count = 0;
                        for (i = 0;  i < 15;  i++)
                        {
                            if (offset & (1 << i))
                            {
                                reg_count++;
                            }
                        }
                        if (ins & 0x00800000)
                        {
                            /* Add offset */
                            Rn += reg_count * 4;
                        }
                        else
                        {
                            /* Subtract offset */
                            Rn -= 4;
                        }
                        return *(unsigned long *)Rn;
                    }
                }
            }
            else
            {
                /* Branch */
                if (arm_ins_will_execute(ins))
                {
                    offset = (ins & 0x00FFFFFF) << 2;
                    if (ins & 0x00800000)
                    {
                        offset |= 0xFC000000;    /* sign extend */
                    }
                    new_pc = (unsigned long)(pc + 2) + offset;
                    /* If its BLX, make new_pc a thumb address. */
                    if ((ins & 0xFE000000) == 0xFA000000)
                    {
                        if ((ins & 0x01000000) == 0x01000000)
                        {
                            /* new_pc |= 2; */
                        }
                        {
                            unsigned short nextins = (*(unsigned short *)new_pc) & 0xffff;
                            if (!thumb_32bit_code(nextins))
                            {
                                new_pc = MAKE_THUMB_ADDR(new_pc);
                            }
                        }
                    }
                    return new_pc;
                }
                else
                {
                    /* Falls through */
                    return (unsigned long)(pc + 1);
                }
            }
        case 0x3:  /* Coprocessor & SWI */
            if (((ins & 0x03000000) == 0x03000000) && arm_ins_will_execute(ins))
            {
                /* SWI */
                /* TODO(wzyy2) some problems.*/
            }
            else
            {
                return (unsigned long)(pc + 1);
            }
        default:
            /* Never reached - but fixes compiler warning.*/
            return 0;
    }
}

#define BITS_SHIFT(x, high, low) (((x) >> (low)) & ((1<<((high)-(low)+1))-1))
#define BITS(x, high, low) ((x) & (((1<<((high)-(low)+1))-1) << (low)))
#define BITS_SHIFT(x, high, low) (((x) >> (low)) & ((1<<((high)-(low)+1))-1))
#define SIGN_EXTEND(val, topbit) (BIT(val, topbit) ? ((val) | (0xffffffff << (topbit))) : (val))
/* #define SIGN_EXTEND(val, topbit) (ASR_SIMPLE(LSL(val, 32-(topbit)), 32-(topbit))) */
#define BIT(x, bit) ((x) & (1 << (bit)))
#define BIT(x, bit) ((x) & (1 << (bit)))
#define submask(x) ((1L << ((x) + 1)) - 1)
#define SBITS(obj,st,fn) \
    ((long) (BITS(obj,st,fn) | ((long) BIT(obj,st) * ~ submask (st - fn))))

#define LSL(val, shift) (((shift) >= 32) ? 0 : ((val) << (shift)))

#define ASR_SIMPLE(val, shift) (((int)(val)) >> (shift))

static int thumb_32bit_code(uint16_t ic)
{
    uint16_t op = (ic >> 11) & 0xFFFF;
    if (op == 0x1D || op == 0x1E || op == 0x1F)
    {
        return 1;
    }
    return 0;
}

static int bitcount(unsigned long val)
{
    int nbits;
    for (nbits = 0; val != 0; nbits++)
    {
        val &= val - 1;    /* Delete rightmost 1-bit in val.  */
    }
    return nbits;
}

unsigned long read_mem_uint(unsigned long memaddr, int len)
{
    unsigned long *addr = (unsigned long *)(memaddr);
    return *addr;
}

#define ARM_INT_REGISTER_SIZE 4

unsigned long thumb_get_next_pc(unsigned short *pc, unsigned short ins)
{
    int Rm;
    int immed;
    unsigned long nextpc;
    unsigned short inst1 = ins;

    gdb_get_register(gdb_arch_regs);

    unsigned long pc_val = ((unsigned long) pc) + 4;  /* PC after prefetch */
    pc_val = MAKE_THUMB_ADDR(pc_val);

    if ((ins & 0xff00) == 0xbd00)   /* pop {rlist, pc} */
    {
        unsigned long sp;

        /* Fetch the saved PC from the stack.  It's stored above
           all of the other registers.  */
        unsigned long offset = bitcount(BITS(inst1, 7, 0)) * ARM_INT_REGISTER_SIZE;
        sp = gdb_arch_regs[GDB_SP];
        nextpc = read_mem_uint(sp + offset, 4);
        nextpc = MAKE_THUMB_ADDR(nextpc);
        return nextpc;
    }
    else if ((inst1 & 0xff87) == 0x4687)  /* mov pc, REG */
    {
        if (BITS(inst1, 6, 3) == 0x0f)
        {
            nextpc = pc_val;
        }
        else
        {
            nextpc = gdb_arch_regs[BITS(inst1, 6, 3)];
        }
        nextpc = MAKE_THUMB_ADDR(nextpc);
        return nextpc;
    }
    else if ((inst1 & 0xf500) == 0xb100)
    {
        /* CBNZ or CBZ.  */
        int imm = (BIT(inst1, 9) << 6) + (BITS(inst1, 7, 3) << 1);
        unsigned long reg = gdb_arch_regs[BITS(inst1, 3, 0)];

        if (BIT(inst1, 11) && reg != 0)
        {
            nextpc = pc_val + imm;
        }
        else if (!BIT(inst1, 11) && reg == 0)
        {
            nextpc = pc_val + imm;
        }
    }

    switch (BITS_SHIFT(ins, 15, 11))
    {
        case 0x00:
        case 0x01:
        case 0x02:
            // shift by immediate
            return MAKE_THUMB_ADDR((unsigned long)(pc + 1));
        case 0x03:
            // add/subtract 3 register
            return MAKE_THUMB_ADDR((unsigned long)(pc + 1));
        case 0x04:
        case 0x05:
        case 0x06:
        case 0x07:
            // add/subtract/compare/move immediate
            return MAKE_THUMB_ADDR((unsigned long)(pc + 1));
        case 0x08:
            if (BIT(ins, 10))
            {
                if (BITS_SHIFT(ins, 9, 8) == 3)
                {
                    Rm = BITS_SHIFT(ins, 6, 3);
                    return (unsigned long)gdb_arch_regs[Rm];
                }
                else
                {
                    /*TODO */
                }
            }
            else
            {
                /*TODO */
            }
            break;
        case 0x09:
            break;
        case 0x0a:
        case 0x0b:
            break;
        case 0x0c:
        case 0x0d:
        case 0x0e:
        case 0x0f:
            break;
        case 0x10:
        case 0x11:
            break;
        case 0x12:
        case 0x13:
            break;
        case 0x14:
        case 0x15:
            break;
        case 0x16:
        case 0x17:
            break;
        case 0x18:
        case 0x19:
            break;
        case 0x1a:
        case 0x1b:
            switch (BITS_SHIFT(ins, 11, 8))
            {
                default:
                    immed = BITS(ins, 7, 0);
                    immed <<= 1;
                    immed = SIGN_EXTEND(immed, 8);
                    immed += 4;
                    return MAKE_THUMB_ADDR((unsigned long)gdb_arch_regs[GDB_PC] + immed);
                case 0xe:
                    break;
                case 0xf:
                    break;
            }
            break;
        case 0x1c:
            // unconditional branch
            immed = BITS(ins, 10, 0);
            immed <<= 1;
            immed = SIGN_EXTEND(immed, 11);
            immed += 4;
            return MAKE_THUMB_ADDR((unsigned long)gdb_arch_regs[GDB_PC] + immed);
        case 0x1d:
            if (ins & 1)
            {
                // undefined instruction
            }
            else
            {
                // blx suffix
            }
            break;
        case 0x1e:
        case 0x1f:
            // bl/blx prefix
            break;
    }
    return MAKE_THUMB_ADDR((unsigned long)(pc + 1));
}

unsigned long thumbv2_get_next_pc(unsigned short *pc_p, unsigned short ins)
{
    unsigned long nextpc;
    unsigned short inst1 = ins;
    unsigned short inst2;
    unsigned long pc = (unsigned long)pc_p;

    gdb_get_register(gdb_arch_regs);

    unsigned long pc_val = ((unsigned long) pc) + 4;  /* PC after prefetch */
    pc_val = MAKE_THUMB_ADDR(pc_val);

    inst2 = read_mem_uint(((unsigned long) pc) + 2, 2) & 0xffff;

    /* Default to the next instruction.  */
    nextpc = pc + 4;
    nextpc = MAKE_THUMB_ADDR(nextpc);

    if ((inst1 & 0xf800) == 0xf000 && (inst2 & 0x8000) == 0x8000)
    {
        /* Branches and miscellaneous control instructions.  */

        if ((inst2 & 0x1000) != 0 || (inst2 & 0xd001) == 0xc000)
        {
            /* B, BL, BLX.  */
            int j1, j2, imm1, imm2;

            imm1 = SBITS(inst1, 10, 0);
            imm2 = BITS(inst2, 10, 0);
            j1 = BIT(inst2, 13);
            j2 = BIT(inst2, 11);

            unsigned long offset = ((imm1 << 12) + (imm2 << 1));
            offset ^= ((!j2) << 22) | ((!j1) << 23);

            nextpc = pc_val + offset;
            /* For BLX make sure to clear the low bits.  */
            if (BIT(inst2, 12) == 0)
            {
                nextpc = nextpc & 0xfffffffc;
            }
        }
        else if (inst1 == 0xf3de && (inst2 & 0xff00) == 0x3f00)
        {
            /* SUBS PC, LR, #imm8.  */
            nextpc = gdb_arch_regs[GDB_LR];
            nextpc -= inst2 & 0x00ff;
        }
        else if ((inst2 & 0xd000) == 0x8000 && (inst1 & 0x0380) != 0x0380)
        {
            /* Conditional branch.  */
            if (thumb_ins_will_execute(inst1))
            {
                int sign, j1, j2, imm1, imm2;

                sign = SBITS(inst1, 10, 10);
                imm1 = BITS(inst1, 5, 0);
                imm2 = BITS(inst2, 10, 0);
                j1 = BIT(inst2, 13);
                j2 = BIT(inst2, 11);

                unsigned long offset
                    = (sign << 20) + (j2 << 19) + (j1 << 18);
                offset += (imm1 << 12) + (imm2 << 1);

                nextpc = pc_val + offset;
            }
        }
    }
    else if ((inst1 & 0xfe50) == 0xe810)
    {
        /* Load multiple or RFE.  */
        int rn, offset, load_pc = 1;

        rn = BITS(inst1, 3, 0);
        if (BIT(inst1, 7) && !BIT(inst1, 8))
        {
            /* LDMIA or POP */
            if (!BIT(inst2, 15))
            {
                load_pc = 0;
            }
            offset = bitcount(inst2) * 4 - 4;
        }
        else if (!BIT(inst1, 7) && BIT(inst1, 8))
        {
            /* LDMDB */
            if (!BIT(inst2, 15))
            {
                load_pc = 0;
            }
            offset = -4;
        }
        else if (BIT(inst1, 7) && BIT(inst1, 8))
        {
            /* RFEIA */
            offset = 0;
        }
        else if (!BIT(inst1, 7) && !BIT(inst1, 8))
        {
            /* RFEDB */
            offset = -8;
        }
        else
        {
            load_pc = 0;
        }

        if (load_pc)
        {
            unsigned long addr = gdb_arch_regs[rn];
            nextpc = read_mem_uint(addr + offset, 4);
        }
    }
    else if ((inst1 & 0xffef) == 0xea4f && (inst2 & 0xfff0) == 0x0f00)
    {
        /* MOV PC or MOVS PC.  */
        nextpc = gdb_arch_regs[BITS(inst2, 3, 0)];
        nextpc = MAKE_THUMB_ADDR(nextpc);
    }
    else if ((inst1 & 0xff70) == 0xf850 && (inst2 & 0xf000) == 0xf000)
    {
        /* LDR PC.  */
        unsigned long base;
        int rn, load_pc = 1;

        rn = BITS(inst1, 3, 0);
        base = gdb_arch_regs[rn];
        if (rn == GDB_PC)
        {
            base = (base + 4) & ~(unsigned long) 0x3;
            if (BIT(inst1, 7))
            {
                base += BITS(inst2, 11, 0);
            }
            else
            {
                base -= BITS(inst2, 11, 0);
            }
        }
        else if (BIT(inst1, 7))
        {
            base += BITS(inst2, 11, 0);
        }
        else if (BIT(inst2, 11))
        {
            if (BIT(inst2, 10))
            {
                if (BIT(inst2, 9))
                {
                    base += BITS(inst2, 7, 0);
                }
                else
                {
                    base -= BITS(inst2, 7, 0);
                }
            }
        }
        else if ((inst2 & 0x0fc0) == 0x0000)
        {
            int shift = BITS(inst2, 5, 4), rm = BITS(inst2, 3, 0);
            base += gdb_arch_regs[rm] << shift;
        }
        else
            /* Reserved.  */
        {
            load_pc = 0;
        }

        if (load_pc)
        {
            nextpc = read_mem_uint(base, 4);
        }
    }
    else if ((inst1 & 0xfff0) == 0xe8d0 && (inst2 & 0xfff0) == 0xf000)
    {
        /* TBB.  */
        unsigned long tbl_reg, table, offset, length;

        tbl_reg = BITS(inst1, 3, 0);
        if (tbl_reg == 0x0f)
        {
            table = pc + 4;    /* Regcache copy of PC isn't right yet.  */
        }
        else
        {
            table = gdb_arch_regs[tbl_reg];
        }

        offset = gdb_arch_regs[BITS(inst2, 3, 0)];
        length = read_mem_uint(table + offset, 1) & 0xf;
        nextpc = pc_val + length;
    }
    else if ((inst1 & 0xfff0) == 0xe8d0 && (inst2 & 0xfff0) == 0xf010)
    {
        /* TBH.  */
        unsigned long tbl_reg, table, offset, length;

        tbl_reg = BITS(inst1, 3, 0);
        if (tbl_reg == 0x0f)
        {
            table = pc + 4;    /* Regcache copy of PC isn't right yet.  */
        }
        else
        {
            table = gdb_arch_regs[tbl_reg];
        }

        offset = 2 * gdb_arch_regs[BITS(inst2, 3, 0)];
        length = (2 * read_mem_uint(table + offset, 2)) & 0xff;
        nextpc = pc_val + length;
    }
    return nextpc;
}

unsigned long thumb_decoder(unsigned short *pc, unsigned short ins)
{
    int Rm;
    int immed;

    gdb_get_register(gdb_arch_regs);
    switch (BITS_SHIFT(ins, 15, 11))
    {
        case 0x00:
        case 0x01:
        case 0x02:
            // shift by immediate
            return MAKE_THUMB_ADDR((unsigned long)(pc + 1));
        case 0x03:
            // add/subtract 3 register
            return MAKE_THUMB_ADDR((unsigned long)(pc + 1));
        case 0x04:
        case 0x05:
        case 0x06:
        case 0x07:
            // add/subtract/compare/move immediate
            return MAKE_THUMB_ADDR((unsigned long)(pc + 1));
        case 0x08:
            if (BIT(ins, 10))
            {
                if (BITS_SHIFT(ins, 9, 8) == 3)
                {
                    Rm = BITS_SHIFT(ins, 6, 3);
                    return (unsigned long)gdb_arch_regs[Rm];
                }
                else
                {
                    /*TODO */
                }
            }
            else
            {
                /*TODO */
            }
            break;
        case 0x09:
            break;
        case 0x0a:
        case 0x0b:
            break;
        case 0x0c:
        case 0x0d:
        case 0x0e:
        case 0x0f:
            break;
        case 0x10:
        case 0x11:
            break;
        case 0x12:
        case 0x13:
            break;
        case 0x14:
        case 0x15:
            break;
        case 0x16:
        case 0x17:
            break;
        case 0x18:
        case 0x19:
            break;
        case 0x1a:
        case 0x1b:
            switch (BITS_SHIFT(ins, 11, 8))
            {
                default:
                    immed = BITS(ins, 7, 0);
                    immed <<= 1;
                    immed = SIGN_EXTEND(immed, 8);
                    immed += 4;
                    return MAKE_THUMB_ADDR((unsigned long)gdb_arch_regs[GDB_PC] + immed);
                case 0xe:
                    break;
                case 0xf:
                    break;
            }
            break;
        case 0x1c:
            // unconditional branch
            immed = BITS(ins, 10, 0);
            immed <<= 1;
            immed = SIGN_EXTEND(immed, 11);
            immed += 4;
            return MAKE_THUMB_ADDR((unsigned long)gdb_arch_regs[GDB_PC] + immed);
        case 0x1d:
            if (ins & 1)
            {
                // undefined instruction
            }
            else
            {
                // blx suffix
            }
            break;
        case 0x1e:
        case 0x1f:
            // bl/blx prefix
            break;
    }
    return MAKE_THUMB_ADDR((unsigned long)(pc + 1));
}

int arch_get_next_pc(void* addr, unsigned long* next, int* length)
{
    struct rt_hw_exp_stack* regs;
    regs = gdb_get_arch_regs();
    unsigned long step_addr = 0;
    int ins_len = 4;
    unsigned long curins;
    unsigned long nextins;

    if (regs->cpsr & 0x20)
    {
        uint16_t testins;
        testins = (*(unsigned short *)addr) & 0xffff;
        if (thumb_32bit_code(testins) == 1)
        {
            step_addr = (unsigned long)((unsigned long*)addr + 4);
            if (thumb_32bit_code((*(unsigned short *)(step_addr)) & 0xffff) != 1)
            {
                ins_len = 2;
                step_addr = MAKE_THUMB_ADDR(step_addr);
            }
            *next = step_addr;
            *length = ins_len;
            return 1;
        }

        curins = (*(unsigned short *)(addr)) & 0xffff;
        /* Decode instruction to decide what the next pc will be */
        if (thumb_ins_will_execute(curins))
        {
            step_addr = thumb_target_ins((unsigned short *)addr, (unsigned short)curins);
        }
        else
        {
            unsigned long tmp = (unsigned long)((unsigned long*)addr + 2);
            step_addr = MAKE_THUMB_ADDR(tmp);
        }

        if (IS_THUMB_ADDR(step_addr))
        {
            nextins = (*(unsigned short *)(UNMAKE_THUMB_ADDR(step_addr))) & 0xffff;
        }
        else
        {
            nextins = (*(unsigned short *)(step_addr)) & 0xffff;
        }

        ins_len = 2;
        if (thumb_32bit_code(nextins) == 1)
        {
            ins_len = 4;
            step_addr = UNMAKE_THUMB_ADDR(step_addr);
        }
    }
    else
    {
        curins = *(unsigned long *)(addr);
        /* Decode instruction to decide what the next pc will be */
        if (arm_ins_will_execute(curins))
        {
            step_addr = arm_target_ins((unsigned long *)addr, curins);
        }
        else
        {
            step_addr = (unsigned long)((unsigned long *)addr + 4);
        }
        if (IS_THUMB_ADDR(step_addr))
        {
            ins_len = 2;
        }
    }

    RT_ASSERT(step_addr != 0);
    rt_kprintf("\n next will be %x, ins_len %d\n", step_addr, ins_len);

    *next = step_addr;
    *length = ins_len;
    return 1;
}
