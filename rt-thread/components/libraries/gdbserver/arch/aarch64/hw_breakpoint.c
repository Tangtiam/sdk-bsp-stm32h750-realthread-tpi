/*
 * Copyright (c) 2011-2020, Shanghai Real-Thread Electronic Technology Co.,Ltd
 *
 * Change Logs:
 * Date           Author       Notes
 * 2020-02-23     Jesven         first version.
 */
#include <rtthread.h>

#include <lwp_arch.h>
#include <lwp_gdbserver.h>
#include <hw_breakpoint.h>
#include <string.h>
#include <lwp.h>
#include <mmu.h>
#include <lwp_user_mm.h>

gdb_thread_info wp_thread_info = {
    RT_NULL,
    GDB_NOTIFIY_NR,
    0, RT_NULL, 0
};

int check_debug_event(struct rt_hw_exp_stack *regs, unsigned long esr)
{
    uint32_t elx = regs->cpsr & 0x1fUL;
    unsigned char ec;

    ec = (unsigned char)((esr >> 26) & 0x3fU);
    if (elx == 0x00) /* is EL0 */
    {
        struct rt_channel_msg msg;
        gdb_thread_info thread_info;
        int ret;

        if (ec == 0x3c || ec == 0x30 /* breakpoint event */
                || ec == 0x32) /* step event */
        {
            if (wp_thread_info.notify_type == GDB_NOTIFIY_WATCHPOINT)
            {
                thread_info.watch_addr = (uint32_t *)regs->pc;
                thread_info.rw = wp_thread_info.rw;
                thread_info.notify_type = GDB_NOTIFIY_WATCHPOINT;
                wp_thread_info.notify_type = GDB_NOTIFIY_NR;
                ret = 2;
            }
            else
            {
                /* is breakpoint event */
                do {
                    struct rt_lwp *gdb_lwp = gdb_get_dbg_lwp();
                    struct rt_lwp *lwp;

                    if (!gdb_lwp)
                    {
                        break;
                    }
                    lwp = lwp_self();
                    if (lwp == gdb_lwp)
                    {
                        break;
                    }
                    *(uint32_t *)regs->pc = lwp->bak_first_ins;
                    rt_hw_cpu_dcache_ops(RT_HW_CACHE_FLUSH, (void *)regs->pc, 4);
                    rt_hw_icache_invalidate_all();
                    lwp->debug = 0;
                    return 1;
                } while (0);

                thread_info.notify_type = GDB_NOTIFIY_BREAKPOINT;
                thread_info.abt_ins = *(uint32_t *)regs->pc;
                ret = 1;
            }
        }
        else if (ec == 0x35 || ec == 0x34) /* watchpoint event */
        {
            /* is watchpoint event */
            arch_deactivate_breakpoints();
            arch_activate_step();
            wp_thread_info.rw = (esr >> 6) & 1UL;
            wp_thread_info.notify_type = GDB_NOTIFIY_WATCHPOINT;
            return 3;
        }
        else
        {
            return 0; /* not support */
        }
        thread_info.thread = rt_thread_self();
        thread_info.thread->regs = regs;
        msg.u.d = (void *)&thread_info;
        rt_hw_dmb();
        thread_info.thread->debug_suspend = 1;
        rt_hw_dsb();
        rt_thread_suspend_with_flag(thread_info.thread, RT_UNINTERRUPTIBLE);
        //rt_raw_channel_send(gdb_get_server_channel(), &msg);
        rt_schedule();
        while (thread_info.thread->debug_suspend)
        {
            rt_thread_suspend_with_flag(thread_info.thread, RT_UNINTERRUPTIBLE);
            rt_schedule();
        }
        return ret;
    }
    return 0;
}

struct gdb_bkpt
{
    void *bpt_addr;
    int  state;
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

/*********************************************/
struct arch_reg_info reg_info[] = {
    {(size_t)&((struct rt_hw_exp_stack *)0)->x0, 8},
    {(size_t)&((struct rt_hw_exp_stack *)0)->x1, 8},
    {(size_t)&((struct rt_hw_exp_stack *)0)->x2, 8},
    {(size_t)&((struct rt_hw_exp_stack *)0)->x3, 8},
    {(size_t)&((struct rt_hw_exp_stack *)0)->x4, 8},
    {(size_t)&((struct rt_hw_exp_stack *)0)->x5, 8},
    {(size_t)&((struct rt_hw_exp_stack *)0)->x6, 8},
    {(size_t)&((struct rt_hw_exp_stack *)0)->x7, 8},
    {(size_t)&((struct rt_hw_exp_stack *)0)->x8, 8},
    {(size_t)&((struct rt_hw_exp_stack *)0)->x9, 8},
    {(size_t)&((struct rt_hw_exp_stack *)0)->x10, 8},
    {(size_t)&((struct rt_hw_exp_stack *)0)->x11, 8},
    {(size_t)&((struct rt_hw_exp_stack *)0)->x12, 8},
    {(size_t)&((struct rt_hw_exp_stack *)0)->x13, 8},
    {(size_t)&((struct rt_hw_exp_stack *)0)->x14, 8},
    {(size_t)&((struct rt_hw_exp_stack *)0)->x15, 8},
    {(size_t)&((struct rt_hw_exp_stack *)0)->x16, 8},
    {(size_t)&((struct rt_hw_exp_stack *)0)->x17, 8},
    {(size_t)&((struct rt_hw_exp_stack *)0)->x18, 8},
    {(size_t)&((struct rt_hw_exp_stack *)0)->x19, 8},
    {(size_t)&((struct rt_hw_exp_stack *)0)->x20, 8},
    {(size_t)&((struct rt_hw_exp_stack *)0)->x21, 8},
    {(size_t)&((struct rt_hw_exp_stack *)0)->x22, 8},
    {(size_t)&((struct rt_hw_exp_stack *)0)->x23, 8},
    {(size_t)&((struct rt_hw_exp_stack *)0)->x24, 8},
    {(size_t)&((struct rt_hw_exp_stack *)0)->x25, 8},
    {(size_t)&((struct rt_hw_exp_stack *)0)->x26, 8},
    {(size_t)&((struct rt_hw_exp_stack *)0)->x27, 8},
    {(size_t)&((struct rt_hw_exp_stack *)0)->x28, 8},
    {(size_t)&((struct rt_hw_exp_stack *)0)->x29, 8},
    {(size_t)&((struct rt_hw_exp_stack *)0)->x30, 8},
    {(size_t)&((struct rt_hw_exp_stack *)0)->sp_el0, 8},
    {(size_t)&((struct rt_hw_exp_stack *)0)->pc, 8},
    {(size_t)&((struct rt_hw_exp_stack *)0)->cpsr, 4},
    {(size_t)&((struct rt_hw_exp_stack *)0)->fpu[0], 16},
    {(size_t)&((struct rt_hw_exp_stack *)0)->fpu[1], 16},
    {(size_t)&((struct rt_hw_exp_stack *)0)->fpu[2], 16},
    {(size_t)&((struct rt_hw_exp_stack *)0)->fpu[3], 16},
    {(size_t)&((struct rt_hw_exp_stack *)0)->fpu[4], 16},
    {(size_t)&((struct rt_hw_exp_stack *)0)->fpu[5], 16},
    {(size_t)&((struct rt_hw_exp_stack *)0)->fpu[6], 16},
    {(size_t)&((struct rt_hw_exp_stack *)0)->fpu[7], 16},
    {(size_t)&((struct rt_hw_exp_stack *)0)->fpu[8], 16},
    {(size_t)&((struct rt_hw_exp_stack *)0)->fpu[9], 16},
    {(size_t)&((struct rt_hw_exp_stack *)0)->fpu[10], 16},
    {(size_t)&((struct rt_hw_exp_stack *)0)->fpu[11], 16},
    {(size_t)&((struct rt_hw_exp_stack *)0)->fpu[12], 16},
    {(size_t)&((struct rt_hw_exp_stack *)0)->fpu[13], 16},
    {(size_t)&((struct rt_hw_exp_stack *)0)->fpu[14], 16},
    {(size_t)&((struct rt_hw_exp_stack *)0)->fpu[15], 16},
    {(size_t)ARCH_REG_OFFSET_INVALID /* &((struct rt_hw_exp_stack *)0)->fpu[16] */, 16},
    {(size_t)ARCH_REG_OFFSET_INVALID /* &((struct rt_hw_exp_stack *)0)->fpu[17] */, 16},
    {(size_t)ARCH_REG_OFFSET_INVALID /* &((struct rt_hw_exp_stack *)0)->fpu[18] */, 16},
    {(size_t)ARCH_REG_OFFSET_INVALID /* &((struct rt_hw_exp_stack *)0)->fpu[19] */, 16},
    {(size_t)ARCH_REG_OFFSET_INVALID /* &((struct rt_hw_exp_stack *)0)->fpu[20] */, 16},
    {(size_t)ARCH_REG_OFFSET_INVALID /* &((struct rt_hw_exp_stack *)0)->fpu[21] */, 16},
    {(size_t)ARCH_REG_OFFSET_INVALID /* &((struct rt_hw_exp_stack *)0)->fpu[22] */, 16},
    {(size_t)ARCH_REG_OFFSET_INVALID /* &((struct rt_hw_exp_stack *)0)->fpu[23] */, 16},
    {(size_t)ARCH_REG_OFFSET_INVALID /* &((struct rt_hw_exp_stack *)0)->fpu[24] */, 16},
    {(size_t)ARCH_REG_OFFSET_INVALID /* &((struct rt_hw_exp_stack *)0)->fpu[25] */, 16},
    {(size_t)ARCH_REG_OFFSET_INVALID /* &((struct rt_hw_exp_stack *)0)->fpu[26] */, 16},
    {(size_t)ARCH_REG_OFFSET_INVALID /* &((struct rt_hw_exp_stack *)0)->fpu[27] */, 16},
    {(size_t)ARCH_REG_OFFSET_INVALID /* &((struct rt_hw_exp_stack *)0)->fpu[28] */, 16},
    {(size_t)ARCH_REG_OFFSET_INVALID /* &((struct rt_hw_exp_stack *)0)->fpu[29] */, 16},
    {(size_t)ARCH_REG_OFFSET_INVALID /* &((struct rt_hw_exp_stack *)0)->fpu[30] */, 16},
    {(size_t)ARCH_REG_OFFSET_INVALID /* &((struct rt_hw_exp_stack *)0)->fpu[31] */, 16},
    {(size_t)&((struct rt_hw_exp_stack *)0)->fpsr, 4},
    {(size_t)&((struct rt_hw_exp_stack *)0)->fpcr, 4},
};

void arch_reg_check(struct rt_hw_exp_stack *reg, size_t no, size_t *v)
{
    uint32_t old_cpsr, new_cpsr;

    if (no == 33) /* cpsr */
    {
        old_cpsr = *(uint32_t *)((char *)reg + (size_t)&((struct rt_hw_exp_stack *)0)->cpsr);
        new_cpsr = *(uint32_t *)v;
        old_cpsr &= ~(0xfUL << 28);
        new_cpsr &= (0xfUL << 28);
        *v = (new_cpsr | old_cpsr);
    }
}
/*********************************************/

#define DBG_MDSCR_MDE       (1UL << 15)
static void mdscr_write(uint32_t mdscr)
{
    asm volatile("msr mdscr_el1, %0" :: "r" (mdscr));
}

#define ARM_MAX_BRP     16
#define ARM_MAX_WRP     16

/* Virtual debug register bases. */
#define AARCH64_DBG_REG_BVR 0
#define AARCH64_DBG_REG_BCR (AARCH64_DBG_REG_BVR + ARM_MAX_BRP)
#define AARCH64_DBG_REG_WVR (AARCH64_DBG_REG_BCR + ARM_MAX_BRP)
#define AARCH64_DBG_REG_WCR (AARCH64_DBG_REG_WVR + ARM_MAX_WRP)

/* Debug register names. */
#define AARCH64_DBG_REG_NAME_BVR    "bvr"
#define AARCH64_DBG_REG_NAME_BCR    "bcr"
#define AARCH64_DBG_REG_NAME_WVR    "wvr"
#define AARCH64_DBG_REG_NAME_WCR    "wcr"

/* Accessor macros for the debug registers. */
#define AARCH64_DBG_READ(N, REG, VAL) do {\
        asm volatile("mrs %0, dbg" REG #N "_el1" : "=r" (VAL));\
} while (0)

#define AARCH64_DBG_WRITE(N, REG, VAL) do {\
        asm volatile("msr dbg" REG #N "_el1, %0" :: "r" (VAL));\
} while (0)

#define READ_WB_REG_CASE(OFF, N, REG, VAL)  \
    case (OFF + N):             \
        AARCH64_DBG_READ(N, REG, VAL);  \
        break

#define WRITE_WB_REG_CASE(OFF, N, REG, VAL) \
    case (OFF + N):             \
        AARCH64_DBG_WRITE(N, REG, VAL); \
        break

#define GEN_READ_WB_REG_CASES(OFF, REG, VAL)    \
    READ_WB_REG_CASE(OFF,  0, REG, VAL);    \
    READ_WB_REG_CASE(OFF,  1, REG, VAL);    \
    READ_WB_REG_CASE(OFF,  2, REG, VAL);    \
    READ_WB_REG_CASE(OFF,  3, REG, VAL);    \
    READ_WB_REG_CASE(OFF,  4, REG, VAL);    \
    READ_WB_REG_CASE(OFF,  5, REG, VAL);    \
    READ_WB_REG_CASE(OFF,  6, REG, VAL);    \
    READ_WB_REG_CASE(OFF,  7, REG, VAL);    \
    READ_WB_REG_CASE(OFF,  8, REG, VAL);    \
    READ_WB_REG_CASE(OFF,  9, REG, VAL);    \
    READ_WB_REG_CASE(OFF, 10, REG, VAL);    \
    READ_WB_REG_CASE(OFF, 11, REG, VAL);    \
    READ_WB_REG_CASE(OFF, 12, REG, VAL);    \
    READ_WB_REG_CASE(OFF, 13, REG, VAL);    \
    READ_WB_REG_CASE(OFF, 14, REG, VAL);    \
    READ_WB_REG_CASE(OFF, 15, REG, VAL)

#define GEN_WRITE_WB_REG_CASES(OFF, REG, VAL)   \
    WRITE_WB_REG_CASE(OFF,  0, REG, VAL);   \
    WRITE_WB_REG_CASE(OFF,  1, REG, VAL);   \
    WRITE_WB_REG_CASE(OFF,  2, REG, VAL);   \
    WRITE_WB_REG_CASE(OFF,  3, REG, VAL);   \
    WRITE_WB_REG_CASE(OFF,  4, REG, VAL);   \
    WRITE_WB_REG_CASE(OFF,  5, REG, VAL);   \
    WRITE_WB_REG_CASE(OFF,  6, REG, VAL);   \
    WRITE_WB_REG_CASE(OFF,  7, REG, VAL);   \
    WRITE_WB_REG_CASE(OFF,  8, REG, VAL);   \
    WRITE_WB_REG_CASE(OFF,  9, REG, VAL);   \
    WRITE_WB_REG_CASE(OFF, 10, REG, VAL);   \
    WRITE_WB_REG_CASE(OFF, 11, REG, VAL);   \
    WRITE_WB_REG_CASE(OFF, 12, REG, VAL);   \
    WRITE_WB_REG_CASE(OFF, 13, REG, VAL);   \
    WRITE_WB_REG_CASE(OFF, 14, REG, VAL);   \
    WRITE_WB_REG_CASE(OFF, 15, REG, VAL)

static void write_wb_reg(int reg, int n, uint64_t val)
{
    switch (reg + n) {
        GEN_WRITE_WB_REG_CASES(AARCH64_DBG_REG_BVR, AARCH64_DBG_REG_NAME_BVR, val);
        GEN_WRITE_WB_REG_CASES(AARCH64_DBG_REG_BCR, AARCH64_DBG_REG_NAME_BCR, val);
        GEN_WRITE_WB_REG_CASES(AARCH64_DBG_REG_WVR, AARCH64_DBG_REG_NAME_WVR, val);
        GEN_WRITE_WB_REG_CASES(AARCH64_DBG_REG_WCR, AARCH64_DBG_REG_NAME_WCR, val);
    default:
        rt_kprintf("attempt to write to unknown breakpoint register %d\n", n);
    }
    rt_hw_isb();
}

#define read_cpuid(reg) ({                      \
        uint64_t __val;                          \
        asm("mrs    %0, " #reg : "=r" (__val));         \
        __val;                              \
        })

static int get_num_brps(void)
{
    return ((read_cpuid(ID_AA64DFR0_EL1) >> 12) & 0xf) + 1;
}

/* Determine number of WRP registers available. */
static int get_num_wrps(void)
{
    return ((read_cpuid(ID_AA64DFR0_EL1) >> 20) & 0xf) + 1;
}

/****************************************************/

#define ARCH_MAX_HW_BREAKPOINTS   (ARM_MAX_BRP - 2)
#define ARCH_MAX_HW_WATCHPOINTS   (ARM_MAX_WRP)

static struct gdb_bkpt gdb_sw_breakpoint[GDB_MAX_BREAKPOINTS];
static struct gdb_bkpt gdb_hw_breakpoint[ARCH_MAX_HW_BREAKPOINTS];
static struct gdb_bkpt gdb_hw_watchpoint[ARCH_MAX_HW_WATCHPOINTS];

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

static void arch_write_hw_bvr_reg(int n, uint64_t v)
{
    write_wb_reg(AARCH64_DBG_REG_BVR, n, v);
}

static void arch_write_hw_bcr_reg(int n, uint64_t v)
{
    write_wb_reg(AARCH64_DBG_REG_BCR, n, v);
}

static void arch_write_hw_wvr_reg(int n, uint64_t v)
{
    write_wb_reg(AARCH64_DBG_REG_WVR, n, v);
}

static void arch_write_hw_wcr_reg(int n, uint64_t v)
{
    write_wb_reg(AARCH64_DBG_REG_WCR, n, v);
}

void unsed_fun(void)
{
    arch_write_hw_wvr_reg(0, 0);
    arch_write_hw_wcr_reg(0, 0);
}

static int arch_hw_bk_nr = 0;
static int arch_hw_wp_nr = 0;
static int arch_hw_wp_max_len = 0;
static int arch_hw_bk_context = 0;

static const char support_str[] = "PacketSize=1000;qXfer:features:read+";

static const char target_xml[] = "<?xml version=\"1.0\"?><!DOCTYPE target SYSTEM \"gdb-target.dtd\"><target><architecture>aarch64</architecture><xi:include href=\"aarch64-core.xml\"/><xi:include href=\"aarch64-fpu.xml\"/></target>";

static const char aarch64_core_xml[] = "<?xml version=\"1.0\"?>\n\
<!-- Copyright (C) 2009-2012 Free Software Foundation, Inc.\n\
     Contributed by ARM Ltd.\n\
\n\
     Copying and distribution of this file, with or without modification,\n\
     are permitted in any medium without royalty provided the copyright\n\
     notice and this notice are preserved.  -->\n\
\n\
<!DOCTYPE feature SYSTEM \"gdb-target.dtd\">\n\
<feature name=\"org.gnu.gdb.aarch64.core\">\n\
  <reg name=\"x0\" bitsize=\"64\"/>\n\
  <reg name=\"x1\" bitsize=\"64\"/>\n\
  <reg name=\"x2\" bitsize=\"64\"/>\n\
  <reg name=\"x3\" bitsize=\"64\"/>\n\
  <reg name=\"x4\" bitsize=\"64\"/>\n\
  <reg name=\"x5\" bitsize=\"64\"/>\n\
  <reg name=\"x6\" bitsize=\"64\"/>\n\
  <reg name=\"x7\" bitsize=\"64\"/>\n\
  <reg name=\"x8\" bitsize=\"64\"/>\n\
  <reg name=\"x9\" bitsize=\"64\"/>\n\
  <reg name=\"x10\" bitsize=\"64\"/>\n\
  <reg name=\"x11\" bitsize=\"64\"/>\n\
  <reg name=\"x12\" bitsize=\"64\"/>\n\
  <reg name=\"x13\" bitsize=\"64\"/>\n\
  <reg name=\"x14\" bitsize=\"64\"/>\n\
  <reg name=\"x15\" bitsize=\"64\"/>\n\
  <reg name=\"x16\" bitsize=\"64\"/>\n\
  <reg name=\"x17\" bitsize=\"64\"/>\n\
  <reg name=\"x18\" bitsize=\"64\"/>\n\
  <reg name=\"x19\" bitsize=\"64\"/>\n\
  <reg name=\"x20\" bitsize=\"64\"/>\n\
  <reg name=\"x21\" bitsize=\"64\"/>\n\
  <reg name=\"x22\" bitsize=\"64\"/>\n\
  <reg name=\"x23\" bitsize=\"64\"/>\n\
  <reg name=\"x24\" bitsize=\"64\"/>\n\
  <reg name=\"x25\" bitsize=\"64\"/>\n\
  <reg name=\"x26\" bitsize=\"64\"/>\n\
  <reg name=\"x27\" bitsize=\"64\"/>\n\
  <reg name=\"x28\" bitsize=\"64\"/>\n\
  <reg name=\"x29\" bitsize=\"64\"/>\n\
  <reg name=\"x30\" bitsize=\"64\"/>\n\
  <reg name=\"sp\" bitsize=\"64\" type=\"data_ptr\"/>\n\
  <reg name=\"pc\" bitsize=\"64\" type=\"code_ptr\"/>\n\
  <reg name=\"cpsr\" bitsize=\"32\"/>\n\
</feature>\n";

static const char aarch64_fpu_xml[] = "<?xml version=\"1.0\"?>\n\
<!-- Copyright (C) 2009-2012 Free Software Foundation, Inc.\n\
     Contributed by ARM Ltd.\n\
\n\
     Copying and distribution of this file, with or without modification,\n\
     are permitted in any medium without royalty provided the copyright\n\
     notice and this notice are preserved.  -->\n\
\n\
<!DOCTYPE feature SYSTEM \"gdb-target.dtd\">\n\
<feature name=\"org.gnu.gdb.aarch64.fpu\">\n\
  <vector id=\"v2d\" type=\"ieee_double\" count=\"2\"/>\n\
  <vector id=\"v2u\" type=\"uint64\" count=\"2\"/>\n\
  <vector id=\"v2i\" type=\"int64\" count=\"2\"/>\n\
  <vector id=\"v4f\" type=\"ieee_single\" count=\"4\"/>\n\
  <vector id=\"v4u\" type=\"uint32\" count=\"4\"/>\n\
  <vector id=\"v4i\" type=\"int32\" count=\"4\"/>\n\
  <vector id=\"v8u\" type=\"uint16\" count=\"8\"/>\n\
  <vector id=\"v8i\" type=\"int16\" count=\"8\"/>\n\
  <vector id=\"v16u\" type=\"uint8\" count=\"16\"/>\n\
  <vector id=\"v16i\" type=\"int8\" count=\"16\"/>\n\
  <vector id=\"v1u\" type=\"uint128\" count=\"1\"/>\n\
  <vector id=\"v1i\" type=\"int128\" count=\"1\"/>\n\
  <union id=\"vnd\">\n\
    <field name=\"f\" type=\"v2d\"/>\n\
    <field name=\"u\" type=\"v2u\"/>\n\
    <field name=\"s\" type=\"v2i\"/>\n\
  </union>\n\
  <union id=\"vns\">\n\
    <field name=\"f\" type=\"v4f\"/>\n\
    <field name=\"u\" type=\"v4u\"/>\n\
    <field name=\"s\" type=\"v4i\"/>\n\
  </union>\n\
  <union id=\"vnh\">\n\
    <field name=\"u\" type=\"v8u\"/>\n\
    <field name=\"s\" type=\"v8i\"/>\n\
  </union>\n\
  <union id=\"vnb\">\n\
    <field name=\"u\" type=\"v16u\"/>\n\
    <field name=\"s\" type=\"v16i\"/>\n\
  </union>\n\
  <union id=\"vnq\">\n\
    <field name=\"u\" type=\"v1u\"/>\n\
    <field name=\"s\" type=\"v1i\"/>\n\
  </union>\n\
  <union id=\"aarch64v\">\n\
    <field name=\"d\" type=\"vnd\"/>\n\
    <field name=\"s\" type=\"vns\"/>\n\
    <field name=\"h\" type=\"vnh\"/>\n\
    <field name=\"b\" type=\"vnb\"/>\n\
    <field name=\"q\" type=\"vnq\"/>\n\
  </union>\n\
  <reg name=\"v0\" bitsize=\"128\" type=\"aarch64v\" regnum=\"34\"/>\n\
  <reg name=\"v1\" bitsize=\"128\" type=\"aarch64v\" />\n\
  <reg name=\"v2\" bitsize=\"128\" type=\"aarch64v\" />\n\
  <reg name=\"v3\" bitsize=\"128\" type=\"aarch64v\" />\n\
  <reg name=\"v4\" bitsize=\"128\" type=\"aarch64v\" />\n\
  <reg name=\"v5\" bitsize=\"128\" type=\"aarch64v\" />\n\
  <reg name=\"v6\" bitsize=\"128\" type=\"aarch64v\" />\n\
  <reg name=\"v7\" bitsize=\"128\" type=\"aarch64v\" />\n\
  <reg name=\"v8\" bitsize=\"128\" type=\"aarch64v\" />\n\
  <reg name=\"v9\" bitsize=\"128\" type=\"aarch64v\" />\n\
  <reg name=\"v10\" bitsize=\"128\" type=\"aarch64v\"/>\n\
  <reg name=\"v11\" bitsize=\"128\" type=\"aarch64v\"/>\n\
  <reg name=\"v12\" bitsize=\"128\" type=\"aarch64v\"/>\n\
  <reg name=\"v13\" bitsize=\"128\" type=\"aarch64v\"/>\n\
  <reg name=\"v14\" bitsize=\"128\" type=\"aarch64v\"/>\n\
  <reg name=\"v15\" bitsize=\"128\" type=\"aarch64v\"/>\n\
  <reg name=\"v16\" bitsize=\"128\" type=\"aarch64v\"/>\n\
  <reg name=\"v17\" bitsize=\"128\" type=\"aarch64v\"/>\n\
  <reg name=\"v18\" bitsize=\"128\" type=\"aarch64v\"/>\n\
  <reg name=\"v19\" bitsize=\"128\" type=\"aarch64v\"/>\n\
  <reg name=\"v20\" bitsize=\"128\" type=\"aarch64v\"/>\n\
  <reg name=\"v21\" bitsize=\"128\" type=\"aarch64v\"/>\n\
  <reg name=\"v22\" bitsize=\"128\" type=\"aarch64v\"/>\n\
  <reg name=\"v23\" bitsize=\"128\" type=\"aarch64v\"/>\n\
  <reg name=\"v24\" bitsize=\"128\" type=\"aarch64v\"/>\n\
  <reg name=\"v25\" bitsize=\"128\" type=\"aarch64v\"/>\n\
  <reg name=\"v26\" bitsize=\"128\" type=\"aarch64v\"/>\n\
  <reg name=\"v27\" bitsize=\"128\" type=\"aarch64v\"/>\n\
  <reg name=\"v28\" bitsize=\"128\" type=\"aarch64v\"/>\n\
  <reg name=\"v29\" bitsize=\"128\" type=\"aarch64v\"/>\n\
  <reg name=\"v30\" bitsize=\"128\" type=\"aarch64v\"/>\n\
  <reg name=\"v31\" bitsize=\"128\" type=\"aarch64v\"/>\n\
  <reg name=\"fpsr\" bitsize=\"32\"/>\n\
  <reg name=\"fpcr\" bitsize=\"32\"/>\n\
</feature>\n\
";

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
    else if (memcmp(cmd_buff, "qXfer:features:read:aarch64-core.xml", sizeof("qXfer:features:read:aarch64-core.xml") - 1) == 0)
    {
        reply_buff[0] = 'l';
        strcpy(reply_buff + 1, aarch64_core_xml);
        return 1;
    }
    else if (memcmp(cmd_buff, "qXfer:features:read:aarch64-fpu.xml:0", sizeof("qXfer:features:read:aarch64-fpu.xml:0") - 1) == 0)
    {
        reply_buff[0] = 'm';
        strncpy(reply_buff + 1, aarch64_fpu_xml, 0x7fd);
        return 1;
    }
    else if (memcmp(cmd_buff, "qXfer:features:read:aarch64-fpu.xml:7fd", sizeof("qXfer:features:read:aarch64-fpu.xml:7fd") - 1) == 0)
    {
        reply_buff[0] = 'l';
        strcpy(reply_buff + 1, aarch64_fpu_xml + 0x7fd);
        return 1;
    }

    return 0;
}

/***************************************************************/

static void arch_hw_breakpoint_init(void)
{
    int i;
    uint64_t v;

    arch_hw_wp_nr = get_num_wrps();
    arch_hw_bk_nr = get_num_brps() - 1; //-2 for context id and step

    arch_hw_bk_context = arch_hw_bk_nr;

    /* test wp watch size */
    v = (0xffUL << 5);
    asm volatile ("msr dbgwcr0_el1, %0"::"r"(v));
    asm volatile ("mrs %0, dbgwcr0_el1":"=r"(v));
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
    size_t v;

    if (gdb_hw_breakpoint[idx].state == BP_SET)
    {
        arch_write_hw_bvr_reg(idx, (size_t)gdb_hw_breakpoint[idx].bpt_addr);
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
    size_t v = 0;

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
        arch_write_hw_wvr_reg(idx, (size_t)gdb_hw_watchpoint[idx].bpt_addr);
        arch_write_hw_wcr_reg(idx, (size_t)gdb_hw_watchpoint[idx].u.wcr);
        gdb_hw_watchpoint[idx].state = BP_ACTIVE;
        rt_hw_isb();
    }
}

static void arch_deactivate_hw_watchpoint(int idx)
{
    size_t v = 0;

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

static int watchpoint_get_cv(void *addr, size_t len, int mode, size_t *wcr, void **wvr)
{
    size_t wcr_value = 0;
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
    size_t wcr;
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
    size_t wcr;
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
    size_t wcr;
    size_t rw;
    size_t offset_start;
    int type;

    for (pass = 0; pass < 2; pass++)
    {
        for (i = 0; i < arch_hw_wp_nr; i++)
        {
            size_t len_mask, align_mask;

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
    arch_write_hw_bvr_reg(arch_hw_bk_context, context);
    rt_hw_isb();
}

void arch_clear_context(void)
{
    size_t v = 0;

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
        struct rt_lwp *lwp = gdb_get_dbg_lwp();
        void* addr;
        size_t ins = INS_BREAK_SOFTWARE;

        addr = gdb_sw_breakpoint[idx].bpt_addr;
        lwp_data_put(&lwp->mmu_info, addr, &ins, BREAK_INSTR_SIZE);
        lwp_data_cache_flush(&lwp->mmu_info, addr, BREAK_INSTR_SIZE);
        gdb_sw_breakpoint[idx].state = BP_ACTIVE;
        rt_hw_icache_invalidate_all();
    }
}

static void arch_deactivate_sw_breakpoint(int idx)
{
    if (gdb_sw_breakpoint[idx].state == BP_ACTIVE)
    {
        struct rt_lwp *lwp = gdb_get_dbg_lwp();
        void* addr;

        addr = gdb_sw_breakpoint[idx].bpt_addr;
        lwp_data_put(&lwp->mmu_info, addr, gdb_sw_breakpoint[idx].u.saved_instr, BREAK_INSTR_SIZE);
        lwp_data_cache_flush(&lwp->mmu_info, addr, BREAK_INSTR_SIZE);
        gdb_sw_breakpoint[idx].state = BP_SET;
        rt_hw_icache_invalidate_all();
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

int arch_set_sw_break(void *addr)
{
    struct rt_lwp *lwp = gdb_get_dbg_lwp();
    int i;
    size_t len;

    for (i = 0; i < GDB_MAX_BREAKPOINTS; i++)
    {
        if (gdb_sw_breakpoint[i].state == BP_UNDEFINED)
        {
            len = lwp_data_get(&lwp->mmu_info, gdb_sw_breakpoint[i].u.saved_instr, (void*)addr, BREAK_INSTR_SIZE);
            if (len < BREAK_INSTR_SIZE)
            {
                return -1;
            }
            gdb_sw_breakpoint[i].bpt_addr = addr;
            gdb_sw_breakpoint[i].state = BP_SET;
            return 0;
        }
    }
    return -1;
}

int arch_remove_sw_break(void *addr)
{
    int i;

    for (i = 0; i < GDB_MAX_BREAKPOINTS; i++)
    {
        /* Now, the state must be BP_SET or BP_UNDEFINED */
        if (gdb_sw_breakpoint[i].state != BP_SET)
            continue;

        if (gdb_sw_breakpoint[i].bpt_addr != addr)
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
    uint32_t mdscr;

    asm volatile("mrs %0, mdscr_el1" :"=r" (mdscr));
    mdscr |= 1UL;
    asm volatile("msr mdscr_el1, %0" :: "r" (mdscr));
    rt_hw_isb();
}

void arch_activate_step(void)
{
    uint32_t mdscr;

    asm volatile("mrs %0, mdscr_el1" :"=r" (mdscr));
    mdscr |= 1UL;
    asm volatile("msr mdscr_el1, %0" :: "r" (mdscr));
    rt_hw_isb();
}

void arch_deactivate_step(void)
{
    uint32_t mdscr;

    asm volatile("mrs %0, mdscr_el1" :"=r" (mdscr));
    mdscr &= ~1UL;
    asm volatile("msr mdscr_el1, %0" :: "r" (mdscr));
    rt_hw_isb();
}

void arch_clear_step(void)
{
    uint32_t mdscr;

    asm volatile("mrs %0, mdscr_el1" :"=r" (mdscr));
    mdscr &= ~1UL;
    asm volatile("msr mdscr_el1, %0" :: "r" (mdscr));
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
    arch_breakpoint_init();

    asm volatile("msr oslar_el1, %0" : : "r" (0));
    rt_hw_isb();
    mdscr_write(DBG_MDSCR_MDE);
    rt_hw_isb();
}

void rt_hw_show_register(struct rt_hw_exp_stack *regs);
void show_regs(struct rt_hw_exp_stack *regs)
{
    rt_hw_show_register(regs);
}

/******************************************/

void check_debug_attach_req(void *pc);
int check_debug_suspend(void);

uint32_t arch_dbg_get_ins(void)
{
    return INS_BREAK_CONNECT;
}

struct dbg_ops_t aarch64_dbg_ops =
{
    lwp_dbg,
    arch_dbg_get_ins,
    arch_activate_step,
    arch_deactivate_step,
    check_debug_event,
    gdb_get_server_channel,
    gdb_get_step_type,
    check_debug_attach_req,
    check_debug_suspend,
};

int aarch64_dbg_register(void)
{
    dbg_register(&aarch64_dbg_ops);
    return 0;
}
INIT_BOARD_EXPORT(aarch64_dbg_register);
