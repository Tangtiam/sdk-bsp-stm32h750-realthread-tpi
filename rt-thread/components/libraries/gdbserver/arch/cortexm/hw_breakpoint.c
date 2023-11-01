/*
 * CORTEXM GDB support
 * arch-specific portion of GDB stub
 * 
 * File      : cortexm_stub.c
 * This file is part of RT-Thread RTOS
 * COPYRIGHT (C) 2006, RT-Thread Develop Team
 *
 * The license and distribution terms for this file may be
 * found in the file LICENSE in this distribution or at
 * http://www.rt-thread.org/license/LICENSE
 *
 * Change Logs:
 * Date           Author       Notes
 * 2014-07-04     Wzyy2      first version
 */
#include <rtthread.h>
#include <gdbserver.h>
#include <dlmodule.h>
#include <string.h>
#include <board.h>
#include "hw_breakpoint.h"

#define GDB_DEBUG_REG_BASE      0xE000EDF0
#define GDB_DEBUG_REG_DHSR      0x00
#define GDB_DEBUG_REG_DCRSR      0x04
#define GDB_DEBUG_REG_DCRDR      0x08
#define GDB_DEBUG_REG_DEMCR      0x0c

#define GDB_DEBUG_REG_DEMCR_MON_EN      (1UL << 16)
#define GDB_DEBUG_REG_DEMCR_MON_PEND      (1UL << 17)
#define GDB_DEBUG_REG_DEMCR_MON_STEP      (1UL << 18)
#define GDB_DEBUG_REG_DEMCR_TRCENA      (1UL << 24)

#define GDB_NVIC_REG_BASE       0xE000E000
#define GDB_NVIC_REG_SHCSR      0xD24
#define GDB_NVIC_REG_DFSR       0xD30

#define GDB_NVIC_REG_SHCSR_MEMFAULTENA   (1 << 16)

#define GDB_FPB_REG_BASE       0xE0002000
#define GDB_FPB_REG_CTRL       0x0
#define GDB_FPB_REG_REMAP      0x4
#define GDB_FPB_REG_COMP       0x8

#define GDB_FPB_REG_CTRL_KEY        (1UL << 1)
#define GDB_FPB_REG_CTRL_ENABLE      1
#define GDB_FPB_REG_COMP_ENABLE      1
#define GDB_FPB_REG_COMP_ADDR       (((1UL << 29) -1) << 2)
#define GDB_FPB_REG_COMP_REPLACE       (((1UL << 32) -1) << 30)

#define GDB_DWT_REG_BASE       0xE0001000
#define GDB_DWT_REG_CTRL       0x0
#define GDB_DWT_REG_COMP       0x20
#define GDB_DWT_REG_MASK       0x24
#define GDB_DWT_REG_FUNCTION       0x28

#define GDB_DWT_REG_FUNCTION_FUC    (((1UL << 4) -1) << 0)

#define IS_THUMB_ADDR(addr)     ((addr) & 1)
#define MAKE_THUMB_ADDR(addr)   ((addr) | 1)
#define UNMAKE_THUMB_ADDR(addr) ((addr) & ~1)

static int arch_hw_bk_nr = 0;
static int arch_hw_wp_nr = 0;
static int arch_hw_wp_max_len = 0;
static int arch_hw_bk_step = 0;
static int arch_hw_bk_context = 0;

struct gdb_bkpt
{
    void *bpt_addr;
    int  state;
    int  inst_len;
    union
    {
        size_t len_mask;
        uint64_t wcr;
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

#define ARCH_MAX_HW_BREAKPOINTS    (16 - 2)
#define ARCH_MAX_HW_WATCHPOINTS   (16)

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

int arch_check_debug_event(struct rt_hw_exp_stack *regs, unsigned long pc_adj)
{
    gdb_thread_info thread_info;

    arch_deactivate_breakpoints();

    rt_kprintf("r00: 0x%08x\n", regs->r0);
    rt_kprintf("r01: 0x%08x\n", regs->r1);
    rt_kprintf("r02: 0x%08x\n", regs->r2);
    rt_kprintf("r03: 0x%08x\n", regs->r3);
    rt_kprintf("r04: 0x%08x\n", regs->r4);
    rt_kprintf("r05: 0x%08x\n", regs->r5);
    rt_kprintf("r06: 0x%08x\n", regs->r6);
    rt_kprintf("r07: 0x%08x\n", regs->r7);
    rt_kprintf("r08: 0x%08x\n", regs->r8);
    rt_kprintf("r09: 0x%08x\n", regs->r9);
    rt_kprintf("r10: 0x%08x\n", regs->r10);
    rt_kprintf("r11: 0x%08x\n", regs->fp);
    rt_kprintf("r12: 0x%08x\n", regs->ip);
    rt_kprintf("sp: 0x%08x\n", regs->sp);
    rt_kprintf(" lr: 0x%08x\n", regs->lr);
    rt_kprintf(" pc: 0x%08x\n", regs->pc);

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
    thread_info.thread = rt_thread_self();
    thread_info.thread->regs = (rt_ubase_t)regs;
    rt_thread_suspend(thread_info.thread);
    rt_mq_send(gdb_get_server_mq(), (void *)&thread_info, sizeof(gdb_thread_info));

    return 1;
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

static void arch_hw_breakpoint_init(void)
{
    int i;

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
        gdb_hw_breakpoint[idx].state = BP_ACTIVE;
    }
}

static void arch_deactivate_hw_breakpoint(int idx)
{
    uint32_t v = 0;

    if (gdb_hw_breakpoint[idx].state == BP_ACTIVE)
    {
        gdb_hw_breakpoint[idx].state = BP_SET;
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
        gdb_hw_watchpoint[idx].state = BP_ACTIVE;
    }
}

static void arch_deactivate_hw_watchpoint(int idx)
{

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

int arch_set_hw_watch(void *addr, size_t len, int mode)
{
    int i;
    uint32_t wcr;
    void* wvr;

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
    return -1;
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

}

void arch_activate_step(void)
{

}

void arch_deactivate_step(void)
{

}

void arch_clear_step(void)
{

}

void arch_breakpoint_init(void)
{
    volatile unsigned long *base;

    /* enable trace*/
    CoreDebug->DHCSR = (0xA05F << CoreDebug_DHCSR_DBGKEY_Msk);

    base = (unsigned long*)(GDB_DEBUG_REG_BASE + GDB_DEBUG_REG_DEMCR);
     /*
      * Enable the debug monitor. When enabled, the System handler priority
      * register controls its priority level.
      * If disabled, then all debug events go
      * to Hard fault
      */
    *base |= GDB_DEBUG_REG_DEMCR_MON_EN;

    sw_breakpoint_init();

    arch_hw_breakpoint_init();
    hw_breakpoint_init();
    hw_watchpoint_init();
}

void arch_enable_debug(void)
{
    arch_breakpoint_init();
}
