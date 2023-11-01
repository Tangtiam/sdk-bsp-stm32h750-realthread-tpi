/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <dfs_file.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <board.h>
#include <dlmodule.h>
#include "rtgmon.h" 
#include "gmoncfg.h"

#define RTGMON_VERSION  "v1.1.0" 

/* utils */ 
#define ROUNDDOWN(x,y)  (((x)/(y))*(y))
#define ROUNDUP(x,y)    ((((x)+(y)-1)/(y))*(y))

/* labels */
#define GMON_MAGIC      "gmon"  /* magic cookie */
#define GMON_VERSION    1       /* version number */

#define GMON_PROF_ON    0 
#define GMON_PROF_BUSY  1 
#define GMON_PROF_ERROR 2 
#define GMON_PROF_OFF   3

enum gmon_record_tag
{
    GMON_TAG_TIME_HIST = 0,
    GMON_TAG_CG_ARC = 1,
    GMON_TAG_BB_COUNT = 2
}; 
typedef rt_uint8_t gmon_record_tag_t; 

/* gmon\sys\gmon_out.h */ 
/* gmon\gmon.c */ 
struct gmon_hdr
{
    char cookie[4];         /* gmon */ 
    rt_uint32_t version;    /* 1 */ 
    char spare[3*4];
};

/* gmon\sys\gmon_out.h */ 
/* gmon\gmon.c */
struct gmon_hist_hdr
{
    rt_ubase_t low_pc;     /* base pc address of sample buffer */
    rt_ubase_t high_pc;    /* max pc address of sampled buffer */
    rt_uint32_t hist_size;  /* size of sample buffer */
    rt_uint32_t prof_rate;  /* profiling clock rate */
    char dimen[15];         /* phys. dim., usually "seconds" */
    char dimen_abbrev;      /* usually 's' for "seconds" */
};

struct gmon_cg_arc_record
{
    char from_pc[sizeof (char *)];	/* address within caller's body */
    char self_pc[sizeof (char *)];	/* address within callee's body */
    char count[4];			        /* number of arc traversals */
};

struct arc
{
    char *frompc;
    char *selfpc;
    int32_t count;
};

struct tostruct
{
    rt_ubase_t selfpc;
    rt_int32_t count;
    ARCINDEX link; 
};

/* gmon\gmon.h */ 
struct gmonparam
{
    long int state;

    /* Histogram Information */
    unsigned short *kcount;
    unsigned long kcountsize;

    /* Call-graph Information */
    ARCINDEX *froms;
    unsigned long fromssize;
    struct tostruct *tos;
    unsigned long tossize;
    long tolimit;

    rt_ubase_t lowpc;
    rt_ubase_t highpc;
    unsigned long textsize;

    unsigned long hashfraction;
    long log_hashfraction;

    void* buffer;
}; 

// static struct gmon_hdr _ghdr; 
static struct gmonparam _gmonparam;
rt_ubase_t prof_pc = 0; 

const char *rtgmon_get_version(void)
{
    return RTGMON_VERSION; 
}

/* 控制 GPROF 开启与关闭 */ 
void rtgmon_control(int mode)
{
    struct gmonparam *p = &_gmonparam;

    if (p->state == GMON_PROF_ERROR)
    {
        return;
    }

    if (mode) 
    {
        /* start */
        p->state = GMON_PROF_ON; 
    }
    else
    {
        /* stop */
        p->state = GMON_PROF_OFF;
    }
}

static int lx_ffs(int i)
{
    static const unsigned char table[] =
    {
        0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
        8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
        8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
        8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8
    };
    unsigned int a;
    unsigned int x = i & -i; //这一步直接得到最低位1的整数值

    a = x <= 0xffff ? (x <= 0xff ? 0 : 8) : (x <= 0xffffff ? 16 : 24);

    return table[x >> a] + a;
}

void _mcount(rt_ubase_t frompc, rt_ubase_t selfpc)
{
    struct gmonparam *p = &_gmonparam;
    int i;
    ARCINDEX *frompcindex;
    ARCINDEX toindex;
    struct tostruct *top, *prevtop;

    frompc -= p->lowpc;
    if (frompc > p->textsize)
        goto done;

    i = frompc >> p->log_hashfraction;
    printf("frompc = %p, selfpc = %p, i = %d\n", (void *)frompc, (void *)selfpc, i);

    frompcindex = &p->froms[i];
    printf("frompcindex = %p\r\n", frompcindex);
    toindex = *frompcindex;
    printf("toindex = %ld\r\n", toindex);
    if (toindex == 0) 
    {
        toindex = ++p->tos[0].link;
        if (toindex >= p->tolimit)
            goto overflow;

        *frompcindex = toindex;
        top = &p->tos[toindex];
        top->selfpc = selfpc;
        top->count = 1;
        top->link = 0;
        goto done;
    }

    top = &p->tos[toindex];
    if (top->selfpc == selfpc) 
    {
        top->count++;
        goto done;
    }

    for (; /* goto done */; )
    {
        if (top->link == 0) 
        {
            toindex = ++p->tos[0].link;
            if (toindex >= p->tolimit)
                goto overflow;

            top = &p->tos[toindex];
            top->selfpc = selfpc;
            top->count = 1;
            top->link = *frompcindex;
            *frompcindex = toindex;
            goto done;
        }

        prevtop = top;
        top = &p->tos[top->link];
        if (top->selfpc == selfpc)
        {
            top->count++;
            toindex = prevtop->link;
            prevtop->link = top->link;
            top->link = *frompcindex;
            *frompcindex = toindex;
            goto done;
        }
    }

done:
    p->state = GMON_PROF_ON;
    return;
overflow:
    p->state = GMON_PROF_ERROR;
    rt_kprintf("mcount: call graph buffer size limit exceeded, gmon.out will not be generated\n");
    return;
} 

static void write_gmon(int fd)
{
    struct gmon_hdr ghdr; 

    memcpy(&ghdr.cookie[0], GMON_MAGIC, sizeof(ghdr.cookie));
    ghdr.version = GMON_VERSION;
    memset(ghdr.spare, '\0', sizeof(ghdr.spare));

    write(fd, &ghdr, sizeof(ghdr));
}

static void write_hist(int fd)
{
    gmon_record_tag_t tag = GMON_TAG_TIME_HIST; 
    struct gmon_hist_hdr hhdr; 

    if (_gmonparam.kcountsize > 0)
    {
        hhdr.low_pc = _gmonparam.lowpc;
        hhdr.high_pc = _gmonparam.highpc;
        hhdr.hist_size = _gmonparam.kcountsize / sizeof(HISTCOUNTER);
        hhdr.prof_rate = GMON_PROFRATE; 
        strncpy(hhdr.dimen, GMON_DIMEN, sizeof(hhdr.dimen)); 
        hhdr.dimen_abbrev = 's';

        /* tag */ 
        write(fd, &tag, sizeof(tag));

        /* struct gmon_hist_hdr */ 
        write(fd, &hhdr, sizeof(hhdr));

        /* kcount kcountsize */ 
        write(fd, _gmonparam.kcount, _gmonparam.kcountsize);
    }
}

static void write_call_graph(int fd)
{
    gmon_record_tag_t tag = GMON_TAG_CG_ARC; 
    ARCINDEX from_index, to_index;
    rt_uint32_t from_len;
    rt_ubase_t frompc;
    static struct gmon_cg_arc_record raw_arc;
    struct arc arc;

    from_len = _gmonparam.fromssize / sizeof(*_gmonparam.froms); 
    for (from_index = 0; from_index < from_len; ++from_index)
    {
        if (_gmonparam.froms[from_index] == 0)
            continue;

        rt_kprintf("from_index = %d\n", from_index);

        frompc = _gmonparam.lowpc;
        frompc += (from_index * _gmonparam.hashfraction * sizeof(*_gmonparam.froms));
        for (to_index = _gmonparam.froms[from_index];
             to_index != 0;
             to_index = _gmonparam.tos[to_index].link)
        {
            arc.frompc = (char *)frompc - _gmonparam.lowpc;
            arc.selfpc = (char *)_gmonparam.tos[to_index].selfpc  - _gmonparam.lowpc;
            arc.count = _gmonparam.tos[to_index].count;
            rt_kprintf("frompc = 0x%x, selfpc = 0x%x, count = %d\n", arc.frompc, arc.selfpc, arc.count);
            rt_kprintf("&raw_arc = 0x%x, &arc = 0x%x\n", &raw_arc, &arc);
            memcpy(&raw_arc, &arc, sizeof(raw_arc));
            rt_kprintf("after memcpy\n");

            /* tag */ 
            write(fd, &tag, sizeof(tag));

            /* struct gmon_hist_hdr */ 
            write(fd, &raw_arc, sizeof(raw_arc));
        }
    }
}

void rtgmon_profile(void)
{
    size_t i = 0; 
    rt_uint32_t pc_scale = 0x10000;

    // printf("prof_pc = %p\n", prof_pc); 
    if((prof_pc >= (_gmonparam.lowpc)) && (prof_pc < (_gmonparam.highpc)))
    {
        i = (prof_pc - _gmonparam.lowpc) / 4; 
        i = i / 65536 * pc_scale + i % 65536 * pc_scale / 65536;
        _gmonparam.kcount[i]++;
    }
}

int moncleanup(void)
{
    int fd = -1;

    if ((fd = open(GMON_OUTFILE, O_RDWR)) < 0)
    {
        rt_kprintf("file open error\n");
        return -RT_EINVAL;
    }

    write_gmon(fd);

    /* write PC histogram */
    write_hist(fd);

    /* write call_graph */
    write_call_graph(fd);

    close(fd);

    rt_free_align(_gmonparam.buffer);

    return 0; 
}

int monstartup(rt_ubase_t lowpc, rt_ubase_t highpc)
{
    struct gmonparam *p = &_gmonparam;
    char *cp;

    p->lowpc = ROUNDDOWN(lowpc, HISTFRACTION * sizeof(HISTCOUNTER));
    p->highpc = ROUNDUP(highpc, HISTFRACTION * sizeof(HISTCOUNTER));
    p->textsize = p->highpc - p->lowpc; 

    /* Hist */ 
    // p->kcountsize = p->textsize / HISTFRACTION;
    p->kcountsize = ROUNDUP(p->textsize / HISTFRACTION, sizeof(*p->froms));

    #define HASHFRACTION 2
    #define ARCDENSITY   3

    p->hashfraction = HASHFRACTION;
    p->log_hashfraction = lx_ffs(p->hashfraction * sizeof(*p->froms)) - 1;
    printf("log_hashfraction = %ld\n", p->log_hashfraction); 

    p->fromssize = p->textsize / HASHFRACTION; 
    p->tolimit = p->textsize * ARCDENSITY / 100;
    if (p->tolimit < MINARCS)
        p->tolimit = MINARCS;
    else if (p->tolimit > MAXARCS)
        p->tolimit = MAXARCS;
    p->tossize = p->tolimit * sizeof(struct tostruct);

    cp = rt_malloc_align(p->kcountsize + p->fromssize + p->tossize, 8);
    rt_memset(cp, 0x00, p->kcountsize + p->fromssize + p->tossize); 
    p->buffer = (void*)cp;

    p->tos = (struct tostruct *)cp;
    cp += p->tossize;
    p->kcount = (HISTCOUNTER *)cp; 
    cp += p->kcountsize;
    p->froms = (ARCINDEX *)cp;
    p->tos[0].link = 0;

    /* Dump */ 
    printf("RT-Gmon Prof Init called....\r\n"); 
    printf("Program addr range: %p-%p(0x%16.16lx, %ld)\n", (void *)p->lowpc, (void *)p->highpc, p->textsize, p->textsize); 
    printf("kcountsize = %p, fromssize = %ld, tossize = %ld\r\n", (void *)p->kcountsize, p->fromssize, p->tossize); 
    printf("tos = %p, kcount %p, froms = %p\r\n", (void *)p->froms, (void *)p->fromssize, (void *)p->froms); 

    /* start */ 
    rtgmon_control(1); 

    return 0; 
} 

extern void __gnu_mcount_nc();
RTM_EXPORT(__gnu_mcount_nc);

