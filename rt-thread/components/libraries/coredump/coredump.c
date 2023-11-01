/*
 * Copyright (c) 2006-2020, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2020-02-17     Jesven       first version
 */

#include <rtthread.h>
#include <dlmodule.h>
#include <dlelf.h>
#include <dfs_file.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cpuport.h>
#include "coredump.h"

#define DBG_TAG    "COREDUMP"
#define DBG_LVL    DBG_INFO
#include <rtdbg.h>

#define COREDUMP_DIR            ""
#define COREDUMP_FILE_NAME_LEN  16

#define ARCH_PAGE_SHIFT     12
#define ARCH_PAGE_SIZE      (1 << ARCH_PAGE_SHIFT)

#define COREDUMP_TO_FILE

#ifdef COREDUMP_TO_FILE
static int cfd = -1;
static int core_dump_open(void)
{
    struct rt_dlmodule *module;
    char *fname;

    fname = (char*)rt_malloc(COREDUMP_FILE_NAME_LEN);
    if (!fname)
    {
        return -1;
    }
    module = rt_thread_self()->module;
    rt_snprintf(fname, COREDUMP_FILE_NAME_LEN, "%s/%s.core", COREDUMP_DIR, module->parent.name);
    LOG_I("Core file name: %s", fname);
    cfd = open(fname, O_WRONLY | O_CREAT | O_TRUNC);
    rt_free(fname);
    if (cfd >= 0)
    {
        return 0;
    }
    else
    {
        LOG_E("Open core file failed!");
        return -1;
    }
}

static void core_dump_close(void)
{
    if (cfd >= 0)
    {
        close(cfd);
        cfd = -1;
    }
}

static int core_dump_write(void *buf, int size)
{
    int len, size_ori = size;
    int off = 0;

    if (cfd < 0)
    {
        return 0;
    }
    while (size_ori)
    {
        len = write(cfd, buf + off, size);
        if (!len)
        {
            return len;
        }
        size_ori -= len;
        off += len;
    }
    return size;
}

#else

static rt_uint8_t *cbuff = RT_NULL;
static int cbuff_offset;

static int core_dump_open(void)
{
    cbuff_offset = 0;
    cbuff = (rt_uint8_t *)rt_malloc(COREDUMP_LEN_LIMIT);
    if (!cbuff)
    {
        LOG_E("Alloc core dump failed!");
        return -1;
    }
    return 0;
}

static void core_dump_close(void)
{
    int i;
    rt_uint8_t *p = cbuff;
    char *disp_buff = RT_NULL;
    rt_int32_t disp_off;

    if (!p)
    {
        return;
    }
    disp_buff = (char *)rt_malloc(8 + 1 + 1 + 16 * 3 + 1);
    if (!disp_buff)
    {
        goto end;
    }
    LOG_I(" Dump limit : 0x%08x", COREDUMP_LEN_LIMIT);
    LOG_I("---------------------------------------------------------");
    LOG_I(" offset   00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F");
    LOG_I("---------------------------------------------------------");
    for (i = 0; i < cbuff_offset; i++)
    {
        if ((i & 0xf) == 0x0)
        {
            disp_off = 0;
            disp_off += rt_sprintf(disp_buff + disp_off, "%08x: ", i);
        }
        disp_off += rt_sprintf(disp_buff + disp_off, "%02x ", *p++);
        if ((i & 0xf) == 0xf)
        {
            LOG_I(disp_buff);
        }
    }
    LOG_I("---------------------------------------------------------");
end:
    if (disp_buff)
    {
        rt_free(disp_buff);
    }
    if (cbuff)
    {
        rt_free(cbuff);
        cbuff = RT_NULL;
    }
}

static int core_dump_write(void *buf, int size)
{
    if (!cbuff)
    {
        return 0;
    }
    if (cbuff_offset + size > COREDUMP_LEN_LIMIT)
    {
        size = COREDUMP_LEN_LIMIT - cbuff_offset;
    }
    rt_memcpy(cbuff + cbuff_offset, buf, size);
    cbuff_offset += size;
    return size;
}

#endif

enum
{
    NHDR_PRSTATUS = 0,
    NHDR_AUXV,
    NHDR_VFP,
    NHDR_NUM,
};

typedef long long   loff_t;

#define ELF_CLASS   ELFCLASS32
#define ELF_OSABI ELFOSABI_NONE

struct pt_regs
{
    unsigned long uregs[18];
};

typedef unsigned long elf_greg_t;

#define ELF_NGREG (sizeof (struct pt_regs) / sizeof(elf_greg_t))
typedef elf_greg_t elf_gregset_t[ELF_NGREG];

#define ARM_cpsr    16
#define ARM_pc      15
#define ARM_lr      14
#define ARM_sp      13
#define ARM_ip      12
#define ARM_fp      11
#define ARM_r10     10
#define ARM_r9      9
#define ARM_r8      8
#define ARM_r7      7
#define ARM_r6      6
#define ARM_r5      5
#define ARM_r4      4
#define ARM_r3      3
#define ARM_r2      2
#define ARM_r1      1
#define ARM_r0      0
#define ARM_ORIG_r0 17

#define ARM_VFPREGS_SIZE ( 32 * 8 /*fpregs*/ + 4 /*fpscr*/ )
struct user_vfp
{
    unsigned long long fpregs[32];
    unsigned long fpscr;
};

#define HWCAP_SWP   (1 << 0)
#define HWCAP_HALF  (1 << 1)
#define HWCAP_THUMB (1 << 2)
#define HWCAP_26BIT (1 << 3)    /* Play it safe */
#define HWCAP_FAST_MULT (1 << 4)
#define HWCAP_FPA   (1 << 5)
#define HWCAP_VFP   (1 << 6)
#define HWCAP_EDSP  (1 << 7)
#define HWCAP_JAVA  (1 << 8)
#define HWCAP_IWMMXT    (1 << 9)
#define HWCAP_CRUNCH    (1 << 10)
#define HWCAP_THUMBEE   (1 << 11)
#define HWCAP_NEON  (1 << 12)
#define HWCAP_VFPv3 (1 << 13)
#define HWCAP_VFPv3D16  (1 << 14)   /* also set for VFPv4-D16 */
#define HWCAP_TLS   (1 << 15)
#define HWCAP_VFPv4 (1 << 16)
#define HWCAP_IDIVA (1 << 17)
#define HWCAP_IDIVT (1 << 18)
#define HWCAP_VFPD32    (1 << 19)   /* set if VFP has 32 regs (not 16) */
#define HWCAP_IDIV  (HWCAP_IDIVA | HWCAP_IDIVT)
#define HWCAP_LPAE  (1 << 20)
#define HWCAP_EVTSTRM   (1 << 21)

struct auxv_item
{
    rt_uint32_t tag;
    rt_uint32_t val;
};

#define AT_HWCAP 0x10
#define AT_NULL  0

struct auxv_item auxv[] =
{
    {AT_HWCAP, HWCAP_NEON | HWCAP_HALF | HWCAP_THUMB | HWCAP_FAST_MULT \
               | HWCAP_VFP | HWCAP_EDSP | HWCAP_NEON | HWCAP_VFPv3 | HWCAP_TLS | HWCAP_VFPD32},
    {AT_NULL, 0},
};

struct elf_siginfo
{
    int si_signo;           /* signal number */
    int si_code;            /* extra code */
    int si_errno;           /* errno */
};

struct elf_prstatus
{
    struct elf_siginfo pr_info; /* Info associated with signal */
    short   pr_cursig;      /* Current signal */
    unsigned long pr_sigpend;   /* Set of pending signals */
    unsigned long pr_sighold;   /* Set of held signals */

    pid_t   pr_pid;
    pid_t   pr_ppid;
    pid_t   pr_pgrp;
    pid_t   pr_sid;

    struct timeval pr_utime;    /* User time */
    struct timeval pr_stime;    /* System time */
    struct timeval pr_cutime;   /* Cumulative user time */
    struct timeval pr_cstime;   /* Cumulative system time */

    elf_gregset_t pr_reg;   /* GP registers */
    int pr_fpvalid;     /* True if math co-processor being used.  */
};

struct fill_note_arg
{
    loff_t offset;
    elf_phdr *p;
};

static void fill_elf_header(elfhdr *elf, int segs,
                        rt_uint16_t machine, rt_uint32_t flags)
{
    rt_memset(elf, 0, sizeof(*elf));

    rt_memcpy(elf->e_ident, ELFMAG, SELFMAG);
    elf->e_ident[EI_CLASS] = ELF_CLASS;
    elf->e_ident[EI_DATA] = ELFDATA2LSB;
    elf->e_ident[EI_VERSION] = EV_CURRENT;
    elf->e_ident[EI_OSABI] = ELF_OSABI;

    elf->e_type = ET_CORE;
    elf->e_machine = machine;
    elf->e_version = EV_CURRENT;
    elf->e_phoff = sizeof(elfhdr);
    elf->e_flags = flags;
    elf->e_ehsize = sizeof(elfhdr);
    elf->e_phentsize = sizeof(elf_phdr);
    elf->e_phnum = segs;

    return;
}

static void fill_elf_note_phdr(elf_phdr *p, int sz, loff_t offset)
{
    p->p_type = PT_NOTE;
    p->p_offset = offset;
    p->p_vaddr = 0;
    p->p_paddr = 0;
    p->p_filesz = sz;
    p->p_memsz = 0;
    p->p_flags = 0;
    p->p_align = 0;
    return;
}

rt_inline int roundup(int x, int y)
{
    return ((x + y - 1) / y) * y;
}

static char* alloc_note_name(char *name, int *sz, int *sz_roundup)
{
    int len, len_roundup;
    char *str;

    len = rt_strlen(name) + 1;
    len_roundup = roundup(len, 4);

    str = (char*)rt_malloc(len);
    if (str)
    {
        rt_memcpy(str, name, len);
        rt_memset(str + len, 0, len_roundup - len);
        *sz = len;
        *sz_roundup = len_roundup;
    }
    return str;
}

static void free_note_name(char *buf)
{
    rt_free(buf);
}

static void fill_phdr_fun(void *arg)
{
    struct fill_note_arg *fna = (struct fill_note_arg *)arg;
    elf_phdr *p = fna->p;
    struct rt_dlmodule *module = 0;

    module = rt_thread_self()->module;
    p->p_type = PT_LOAD;
    p->p_offset = fna->offset;
    p->p_vaddr = (Elf32_Addr)module->mem_space;
    p->p_paddr = 0;
    p->p_filesz = (Elf32_Addr)module->mem_size;
    p->p_memsz = (Elf32_Addr)module->mem_size;
    p->p_align = ARCH_PAGE_SIZE;
    p->p_flags = PF_X | PF_W | PF_R;

    fna->offset += (Elf32_Addr)module->mem_size;
    fna->p++;
}

#define CHECK_PTR(p) \
    do\
    {\
        if (!p)\
        {\
            goto end;\
        }\
    } while (0)

int check_vfp(void);
void get_vfp(struct user_vfp *uvfp){}

void coredump(struct rt_hw_exp_stack *regs)
{
    int i;
    int ret;
    int phdr_num = 0;
    int seg_num = 0;
    loff_t offset = 0;
    elfhdr *ehdr = 0;
    elf_phdr *p = 0, *tphdr;
    struct elf_prstatus *prstatus = 0;
    struct user_vfp *prvfp = 0;
    char *page = 0;
    struct fill_note_arg fna;
    int note_len;
    Elf32_Nhdr note_hdr[NHDR_NUM];
    char *name[NHDR_NUM];
    int name_sz[NHDR_NUM], name_sz_ru[NHDR_NUM];
    int use_vfp = 0;
    struct rt_dlmodule *module = 0;

    module = rt_thread_self()->module;
    CHECK_PTR(module);
    seg_num = 1;
    if (seg_num)
    {
        /* for NOTE*/
        phdr_num = seg_num + 1;
    }
    else
    {
        return;
    }

    rt_memset(name, 0, sizeof name);

    page = (char*)rt_malloc(ARCH_PAGE_SIZE);
    CHECK_PTR(page);
    rt_memset(page, 0, ARCH_PAGE_SIZE);

    ehdr = (elfhdr *)rt_malloc(sizeof *ehdr);
    CHECK_PTR(ehdr);

    note_len = 0;

    prstatus = (struct elf_prstatus *)rt_malloc(sizeof *prstatus);
    CHECK_PTR(prstatus);

    memset(prstatus, 0, sizeof *prstatus);
    if(regs)
    {
        prstatus->pr_reg[ARM_cpsr] = regs->cpsr;
        prstatus->pr_reg[ARM_pc]   = regs->pc;
        prstatus->pr_reg[ARM_lr]   = regs->lr;
        prstatus->pr_reg[ARM_sp]   = regs->sp;
        prstatus->pr_reg[ARM_ip]   = regs->ip;
        prstatus->pr_reg[ARM_fp]   = regs->fp;
        prstatus->pr_reg[ARM_r10]  = regs->r10;
        prstatus->pr_reg[ARM_r9]   = regs->r9;
        prstatus->pr_reg[ARM_r8]   = regs->r8;
        prstatus->pr_reg[ARM_r7]   = regs->r7;
        prstatus->pr_reg[ARM_r6]   = regs->r6;
        prstatus->pr_reg[ARM_r5]   = regs->r5;
        prstatus->pr_reg[ARM_r4]   = regs->r4;
        prstatus->pr_reg[ARM_r3]   = regs->r3;
        prstatus->pr_reg[ARM_r2]   = regs->r2;
        prstatus->pr_reg[ARM_r1]   = regs->r1;
        prstatus->pr_reg[ARM_r0]   = regs->r0;
        prstatus->pr_reg[ARM_ORIG_r0] = regs->r0;
    }
    //use_vfp = check_vfp();
    use_vfp = 0;
    prstatus->pr_fpvalid = use_vfp;

    ret = core_dump_open();
    if (ret != 0)
    {
        goto end;
    }

    fill_elf_header(ehdr, phdr_num, EM_ARM, 0);
    core_dump_write(ehdr, sizeof *ehdr);

    offset += sizeof(*ehdr);
    offset += phdr_num * sizeof(elf_phdr);

    p = (elf_phdr *)rt_malloc(phdr_num * sizeof *p);
    CHECK_PTR(p);
    rt_memset(p, 0, phdr_num * sizeof *p);

    name[NHDR_PRSTATUS] = alloc_note_name("CORE", &name_sz[NHDR_PRSTATUS], &name_sz_ru[NHDR_PRSTATUS]);
    CHECK_PTR(name[NHDR_PRSTATUS]);
    note_hdr[NHDR_PRSTATUS].n_namesz = name_sz[NHDR_PRSTATUS]; /* CORE */
    note_hdr[NHDR_PRSTATUS].n_descsz = sizeof *prstatus;
    note_hdr[NHDR_PRSTATUS].n_type = NT_PRSTATUS;
    note_len += sizeof note_hdr[0] + name_sz_ru[NHDR_PRSTATUS] + sizeof *prstatus;

    name[NHDR_AUXV] = alloc_note_name("CORE", &name_sz[NHDR_AUXV], &name_sz_ru[NHDR_AUXV]);
    CHECK_PTR(name[NHDR_AUXV]);
    note_hdr[NHDR_AUXV].n_namesz = name_sz[NHDR_AUXV]; /* CORE */
    note_hdr[NHDR_AUXV].n_descsz = sizeof(auxv);
    note_hdr[NHDR_AUXV].n_type = NT_AUXV;
    note_len += sizeof note_hdr[0] + name_sz_ru[NHDR_AUXV] + sizeof(auxv);

    if (use_vfp)
    {
        prvfp = (struct user_vfp *)rt_malloc(sizeof *prvfp);
        CHECK_PTR(prvfp);
        get_vfp(prvfp);
        name[NHDR_VFP] = alloc_note_name("LINUX", &name_sz[NHDR_VFP], &name_sz_ru[NHDR_VFP]);
        CHECK_PTR(name[NHDR_VFP]);
        note_hdr[NHDR_VFP].n_namesz = name_sz[NHDR_VFP]; /* CORE */
        note_hdr[NHDR_VFP].n_descsz = ARM_VFPREGS_SIZE;
        note_hdr[NHDR_VFP].n_type = NT_ARM_VFP;
        note_len += sizeof note_hdr[0] + name_sz_ru[NHDR_VFP] + ARM_VFPREGS_SIZE;
    }

    fill_elf_note_phdr(p, note_len, offset);

    fna.offset = roundup(offset, ARCH_PAGE_SIZE);
    fna.p = p + 1;

    fill_phdr_fun(&fna);

    core_dump_write(p, phdr_num * sizeof *p);

    /* out put note seg */
    core_dump_write(&note_hdr[NHDR_PRSTATUS], sizeof note_hdr[0]);
    offset += sizeof note_hdr[0];
    core_dump_write(name[NHDR_PRSTATUS], name_sz_ru[NHDR_PRSTATUS]);
    offset += name_sz_ru[NHDR_PRSTATUS];
    core_dump_write(prstatus, note_hdr[NHDR_PRSTATUS].n_descsz);
    offset += note_hdr[NHDR_PRSTATUS].n_descsz;

    core_dump_write(&note_hdr[NHDR_AUXV], sizeof note_hdr[0]);
    offset += sizeof note_hdr[0];
    core_dump_write(name[NHDR_AUXV], name_sz_ru[NHDR_AUXV]);
    offset += name_sz_ru[NHDR_AUXV];
    core_dump_write(auxv, note_hdr[NHDR_AUXV].n_descsz);
    offset += note_hdr[NHDR_AUXV].n_descsz;

    if (use_vfp)
    {
        core_dump_write(&note_hdr[NHDR_VFP], sizeof note_hdr[0]);
        offset += sizeof note_hdr[0];
        core_dump_write(name[NHDR_VFP], name_sz_ru[NHDR_VFP]);
        offset += name_sz_ru[NHDR_VFP];
        core_dump_write(prvfp, note_hdr[NHDR_VFP].n_descsz);
        offset += note_hdr[NHDR_VFP].n_descsz;
    }

    /* out put load seg */
    tphdr = p + 1;
    for (i = 0; i < seg_num; i++, tphdr++)
    {
        if (tphdr->p_offset != offset)
        {
            core_dump_write(page, tphdr->p_offset - offset);
        }
        core_dump_write((void*)tphdr->p_vaddr, tphdr->p_filesz);
        offset = tphdr->p_offset + tphdr->p_filesz;
    }

    LOG_I("Segmentation fault (core dumped)");
end:
    core_dump_close();

    for (i = 0; i < NHDR_NUM; i++)
    {
        if (name[i])
        {
            free_note_name(name[i]);
        }
    }
    if (page)
    {
        rt_free(page);
    }
    if (ehdr)
    {
        rt_free(ehdr);
    }
    if (p)
    {
        rt_free(p);
    }
    if (prvfp)
    {
        rt_free(prvfp);
    }
    if (prstatus)
    {
        rt_free(prstatus);
    }
}

