/*
 * Copyright (c) 2011-2020, Shanghai Real-Thread Electronic Technology Co.,Ltd
 *
 * Change Logs:
 * Date           Author       Notes
 * 2020-02-23     Jesven         first version.
 */
#ifndef  GDBSERVER_H__
#define  GDBSERVER_H__

#include <rtthread.h>

enum
{
    GDB_NOTIFIY_BREAKPOINT = 0,
    GDB_NOTIFIY_WATCHPOINT,
    GDB_NOTIFIY_CMD,
    GDB_NOTIFIY_NR,
};

struct gdb_client_ops
{
    int     (*open)(void);
    int     (*connect)(int fd);
    ssize_t (*recv)(void *buff, size_t len, int flags);
    ssize_t (*send)(void *buff, size_t len);
    void    (*close)(void);
};

typedef struct
{
    rt_thread_t thread;
    uint32_t notify_type;
    uint32_t abt_ins;
    void *watch_addr;
    uint32_t rw;
} gdb_thread_info;

void set_process_id(int id);
struct rt_hw_exp_stack *gdb_get_arch_regs(void);
struct rt_dlmodule *gdb_get_dbg_module(void);

int gdbserver(void);
rt_mq_t gdb_get_server_mq(void);

int gdb_com_open(void);
void gdb_com_close(int fd);
ssize_t gdb_com_read(int fd, void *buff, size_t len);
ssize_t gdb_com_write(int fd, void *buff, size_t len);
uint32_t dbg_get_ins(void);
int dbg_check_event(struct rt_hw_exp_stack *regs, unsigned long esr);

#define ARCH_REG_OFFSET_INVALID (-1UL)

struct arch_reg_info {
    size_t offset;
    size_t len;
};

int dbg(int argc, char **argv);

#endif

