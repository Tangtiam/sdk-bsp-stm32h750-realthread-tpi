/*
 * Copyright (c) 2011-2020, Shanghai Real-Thread Electronic Technology Co.,Ltd
 *
 * Change Logs:
 * Date           Author       Notes
 * 2020-02-23     Jesven         first version.
 */
#include <rtthread.h>
#include <rthw.h>
#include <board.h>
#include <fcntl.h>
#include <string.h>
#include <gdbserver.h>
#include <dlmodule.h>
#include <msh.h>
#include "hw_breakpoint.h"
#include <sys/errno.h>

#ifdef RT_USING_SAL
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netdev.h>
#endif

#define DBG_TAG           "gdgbserver"
#define DBG_LVL           DBG_INFO
#include <rtdbg.h>

/********************************/

/*
#define GDB_DEBUG
*/

/********************************/

/********************************/

#define BUFMAX 4200
#define GDB_DEBUG

/********************************/

#ifdef GDB_DEBUG
#define gdbprintf rt_kprintf
#else
#define gdbprintf(...)
#endif

enum
{
    GDBSRV_STATE_WAIT_THREAD_CONNECT = 0,
    GDBSRV_STATE_WAIT_GDB_CONNECT,
    GDBSRV_STATE_WAIT_GDB_CMD,
    GDBSRV_STATE_WAIT_THEAD_TRAP,
    GDBSRV_STATE_PROCECE_GDB_CMD,
    GDBSRV_STATE_REPLY_GDB_CMD,
};

enum {
    SET_UNKNOW = 0,
    SET_LISTEN_PORT,
    SET_DEVICE_NAME,
    SET_PID,
    SET_ARGS,
};

enum {
    MODE_UNKNOW = 0,
    MODE_DEVICE,
    MODE_TCP,
};

/********************************************/

#define GDB_NAME "gdbsrv"

/********************************/

static rt_mq_t gdb_mq;
struct rt_semaphore gdb_rx;
static rt_thread_t dbg_thread;
static struct rt_dlmodule *dbg_module;

static int gdb_connect_fd = -1;
static int gdb_connected = 0;
static int gdb_need_resume_program = 0;
static int gdb_inuse = 0;

static char remcom_in_buffer[BUFMAX];
static char remcom_out_buffer[BUFMAX];
static struct rt_hw_exp_stack tregs;
static struct rt_hw_exp_stack *regs;
static char *dbg_device_name;
static rt_device_t dbg_device = 0;
static unsigned long step_addr;
static int dbg_listen_port;
static int sig = 0;

static void wait_all_dbgthread_stop(void);

/*********** interface **********/

static rt_err_t gdb_rx_ind(rt_device_t dev, rt_size_t size)
{
    /* release semaphore to let thread rx data */
    rt_sem_release(&gdb_rx);

    return RT_EOK;
}

#ifdef PKG_USING_VCONSOLE
#include <vconsole.h>
static rt_device_t vconsole_device = NULL;
static rt_size_t vconsole_output(rt_device_t device, rt_uint8_t *buff, rt_size_t size)
{
    return size;
}

void gdb_set_device(const char *device_name)
{
    rt_device_t dev = RT_NULL;

    dev = rt_device_find(device_name);
    if (dev == RT_NULL)
    {
        rt_kprintf("gdb: can not find device: %s\n", device_name);
        return;
    }

    /* check whether it's a same device */
    if ((int)dev == dbg_device) return;

    if(dev == rt_console_get_device())
    {
        vconsole_device = vconsole_create("vconsole", vconsole_output);
        if (vconsole_device)
        {
            vconsole_switch(vconsole_device);
        }
        else
        {
            rt_kprintf("can't create vconsole device\r\n");
            return;
        }
    }

    dbg_device = dev;
}
#endif

static int valid_addr(size_t addr)
{
    return 1;
}

static int gdb_device_open(void)
{
    if (rt_device_open(dbg_device, RT_DEVICE_OFLAG_RDWR | RT_DEVICE_FLAG_INT_RX ) == RT_EOK)
    {
        rt_device_set_rx_indicate(dbg_device, gdb_rx_ind);
    }
    else
    {
        rt_kprintf("open deivce 0x%x failed\n", (int)dbg_device);
        return 0;
    }

    return (int)dbg_device;
}

static int gdb_device_connect(int fd)
{
    return (int)dbg_device;
}

static ssize_t gdb_device_read(void *buff, size_t size, int flags)
{
    int len = 0;

    do
    {
        len = rt_device_read(dbg_device, -1,  buff, size);
        if (len <= 0)
        {
            if (flags & O_NONBLOCK)
            {
                len = -EAGAIN;
                break;
            }

            rt_sem_take(&gdb_rx, RT_WAITING_FOREVER);
        }
    }while (len <= 0);

    return len;
}

static ssize_t gdb_device_write(void *buff, size_t len)
{
    return rt_device_write(dbg_device, -1, buff, len);
}

static void gdb_device_close(void)
{
    rt_device_close(dbg_device);
}

static struct gdb_client_ops gdb_device_ops =
{
    gdb_device_open,
    gdb_device_connect,
    gdb_device_read,
    gdb_device_write,
    gdb_device_close,
};

#ifdef RT_USING_SAL
/* tcp */

static int gdb_tcp_open(void)
{
    int listen_fd;

    if ((listen_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
    {
        rt_kprintf("Debug: Socket create failed.\n");
    }
    return listen_fd;
}

static int gdb_tcp_connect(int fd)
{
    int optval = 1;
    socklen_t sin_size;
    int client_fd = -1;
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(dbg_listen_port);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    bzero(&server_addr.sin_zero, sizeof(server_addr.sin_zero));
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    if (bind(fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        rt_kprintf("Debug: Bind failed.\n");
        goto quit;
    }
    if (listen(fd, 1) == -1)
    {
        rt_kprintf("Debug: Listen failed.\n");
        goto quit;
    }
    rt_kprintf("Debug: On TCP port %d Waiting for GDB client ...\n", dbg_listen_port);
    sin_size = sizeof(struct sockaddr_in);
    if ((client_fd = accept(fd, (struct sockaddr *)&client_addr, &sin_size)) == -1)
    {
        rt_kprintf("Debug: Accept failed.\n");
        goto quit;
    }
    rt_kprintf("Debug: GDB client connect OK.\n", dbg_listen_port);
quit:
    closesocket(fd);
    return client_fd;
}

static ssize_t gdb_tcp_read(void *buff, size_t len, int flags)
{
    if (flags & O_NONBLOCK)
        flags = MSG_DONTWAIT;
    return recv(gdb_connect_fd, buff, len, flags);
}

static ssize_t gdb_tcp_write(void *buff, size_t len)
{
    return send(gdb_connect_fd, buff, len, 0);
}

static void gdb_tcp_close(void)
{
    closesocket(gdb_connect_fd);
}

static struct gdb_client_ops         gdb_tcp_ops =
{
    gdb_tcp_open,
    gdb_tcp_connect,
    gdb_tcp_read,
    gdb_tcp_write,
    gdb_tcp_close,
};

#endif

/*********** interface framework **********/

static struct gdb_client_ops *client_ops;

static int gdb_client_open(void)
{
    if (gdb_connect_fd >= 0)
    {
        return -1;
    }
    gdb_connect_fd = (*client_ops->open)();
    if (dbg_device_name)
    {
        rt_free(dbg_device_name);
        dbg_device_name = RT_NULL;
    }
    return gdb_connect_fd;
}

static void gdb_client_connect(void)
{
    gdb_connect_fd = (*client_ops->connect)(gdb_connect_fd);
}

static void gdb_client_close(void)
{
    if (gdb_connect_fd >= 0)
    {
        (*client_ops->close)();
    }
    gdb_connect_fd = -1;
}

static ssize_t gdb_client_recv(void *buff, size_t len, int flags)
{
    return (*client_ops->recv)(buff, len, flags);
}

static ssize_t gdb_client_send(void *buff, size_t len)
{
    return (*client_ops->send)(buff, len);
}

/********************************************/

#ifdef GDB_DEBUG
void show_regs(struct rt_hw_exp_stack *regs);
#endif

struct rt_hw_exp_stack *gdb_get_arch_regs(void)
{
    return regs;
}

struct rt_dlmodule *gdb_get_dbg_module(void)
{
    return dbg_module;
}

rt_mq_t gdb_get_server_mq(void)
{
    return gdb_mq;
}

static void gdb_switch_current_thread(rt_thread_t thread)
{
    rt_kprintf("gdb_switch_current_thread %s\n", thread->parent.name);

    dbg_thread = thread;

    if (!thread)
    {
        regs = (struct rt_hw_exp_stack *)RT_NULL;
        return;
    }

    regs = (struct rt_hw_exp_stack *)thread->regs;
}

static rt_err_t _thread_suspend(rt_thread_t thread)
{
    rt_base_t stat;
    rt_base_t level;

    /* parameter check */
    RT_ASSERT(thread != RT_NULL);

    rt_kprintf ("thread suspend:  %s, stat %d\n", thread->parent.name, thread->stat);

    /* disable interrupt */
    level = rt_hw_interrupt_disable();

    stat = thread->stat & RT_THREAD_STAT_MASK;
    if ((stat != RT_THREAD_READY) && (stat != RT_THREAD_RUNNING))
    {
        LOG_D("thread suspend: thread disorder, 0x%2x\n", thread->stat);
        return -RT_ERROR;
    }

    /* change thread stat */
    rt_schedule_remove_thread(thread);
    thread->stat = RT_THREAD_SUSPEND_UNINTERRUPTIBLE | (thread->stat & ~RT_THREAD_STAT_MASK);

    /* stop thread timer anyway */
    rt_timer_stop(&(thread->thread_timer));

    /* enable interrupt */
    rt_hw_interrupt_enable(level);

    return RT_EOK;
}

/*******************************************/

static const char hexchars[] = "0123456789abcdef";

static char tohex(char c)
{
    return hexchars[c & 15];
}

static int hex(char ch)
{
    if ((ch >= 'a') && (ch <= 'f'))
        return ch - 'a' + 10;
    if ((ch >= '0') && (ch <= '9'))
        return ch - '0';
    if ((ch >= 'A') && (ch <= 'F'))
        return ch - 'A' + 10;
    return -1;
}

int gdb_mem2hex(char *mem, char *buf, int count)
{
    char *bufbak = buf;
    char *tmp = mem;
    char ch;

    while (count > 0)
    {
        ch = *(tmp++);
        *(buf++) = tohex((ch >> 4) & 0xf);
        *(buf++) = tohex(ch & 0xf);

        count--;
    }
    *buf = 0;
    return buf - bufbak;
}

int gdb_hex2mem(char *buf, char *mem, int count)
{
    char v;
    int i;

    for (i = 0; i < count; i++)
    {
        v = ((hex(buf[0]) << 4) | hex(buf[1]));
        buf += 2;
        *mem++ = v;
    }
    return count;
}

static int gdb_hex2long(char **ptr, size_t *long_val)
{
    int hex_val;
    int num = 0;
    int negate = 0;

    *long_val = 0;

    if (**ptr == '-')
    {
        negate = 1;
        (*ptr)++;
    }
    while (**ptr)
    {
        hex_val = hex(**ptr);
        if (hex_val < 0)
            break;

        *long_val = (*long_val << 4) | hex_val;
        num++;
        (*ptr)++;
    }

    if (negate)
        *long_val = -*long_val;

    return num;
}

static int check_gdb_cmd(char *buff)
{
    int i = 0;
    uint8_t check_sum = 0;
    uint8_t recv_check_sum = 0;
    uint8_t c;

    if (buff[i++] != '$')
    {
        return -1;
    }
    while ((c = buff[i++]) != '#')
    {
        check_sum += c;
    }
    gdb_hex2mem(buff + i, (char *)&recv_check_sum, 1);
    if (recv_check_sum != check_sum)
    {
        return -1;
    }
    return 0;
}

#define GDB_CLIENT_RECV_WITH_CHECK(buff, size) do {if (gdb_client_recv(buff, size, 0) < 0) goto client_err;} while (0)
#define GDB_CLIENT_SEND_WITH_CHECK(buff, size) do {if (gdb_client_send(buff, size) < 0) goto client_err;} while (0)
#define RECV_GDB_CMD_WITH_CHECK(buff, size) do {if (recv_gdb_cmd(buff) < 0) goto client_err;} while (0)
#define REPLY_GDB_WITH_CHECK(buff) do {if (reply_gdb(buff) < 0) goto client_err;} while (0)

static int recv_gdb_cmd(char *buff)
{
    int off = 0;

    while (1)
    {
        GDB_CLIENT_RECV_WITH_CHECK(buff + off++, 1);
        if (buff[0] != '$')
        {
            off = 0;
            continue;
        }
        if (buff[off - 1] == '#')
        {
            GDB_CLIENT_RECV_WITH_CHECK(buff + off++, 1);
            GDB_CLIENT_RECV_WITH_CHECK(buff + off++, 1);
            buff[off] = '\0';
            if (check_gdb_cmd(buff) != 0)
            {
                off = 0;
                continue;
            }
            gdbprintf("\n>> +%s", buff);
            gdbprintf("\n<< +");
            GDB_CLIENT_SEND_WITH_CHECK("+", 1);
            break;
        }
    }
    return off;
client_err:
    return -1;
}

static int reply_gdb(char *buff)
{
    int len;
    int i;
    int off;
    uint8_t check_sum = 0;

    off = 0;
    buff[off++] = '$';
    len = strlen(buff + 1);
    for (i = 0; i < len; i++)
    {
        check_sum += buff[off];
        off++;
    }
    buff[off++] = '#';
    buff[off++] = tohex(check_sum >> 4);
    buff[off++] = tohex(check_sum & 0xf);
    buff[off] = '\0';
    gdbprintf("%s\n", buff);
    GDB_CLIENT_SEND_WITH_CHECK(buff, off);
    return 0;
client_err:
    return -1;
}

static void reply_ok(char *reply_buff)
{
    reply_buff[0] = 'O';
    reply_buff[1] = 'K';
    reply_buff[2] = '\0';
}

static void reply_err(char *reply_buff)
{
    reply_buff[0] = 'E';
    reply_buff[1] = '0';
    reply_buff[2] = '1';
    reply_buff[3] = '\0';
}

#define DBG_THREAD_MAX 256
static int thread_info[DBG_THREAD_MAX];
static int thread_info_nr;
static int thread_info_report;
static struct rt_object_information *info;

static int gdb_cmd_qthread(char *cmd_buff, char *reply_buff)
{
    if (cmd_buff[1] == 'C' && cmd_buff[2] == '#')
    {
#ifdef ARCH_CPU_64BIT
        sprintf(reply_buff, "QC%016lx", (size_t)dbg_thread);
#else
        sprintf(reply_buff, "QC%08x", (size_t)dbg_thread);
#endif
        return 1;
    }

    if (memcmp(cmd_buff + 2, "ThreadInfo", sizeof("ThreadInfo") - 1) == 0)
    {
        switch (cmd_buff[1])
        {
            case 'f':
                thread_info_nr = 0;
                thread_info_report = 0;
                {
                    rt_base_t level;
                    rt_list_t *list;

                    level = rt_hw_interrupt_disable();
                    list = dbg_module->t_grp.next;
                    while (list != &dbg_module->t_grp)
                    {
                        rt_thread_t thread;

                        thread = rt_list_entry(list, struct rt_thread, sibling);
                        if ((thread->stat & RT_THREAD_STAT_MASK) != RT_THREAD_INIT)
                        {
                            thread_info[thread_info_nr++] = (size_t)thread;
                        }
                        list = list->next;
                    }
                    rt_hw_interrupt_enable(level);
                }
            case 's':
                if (thread_info_report < thread_info_nr)
                {
                    sprintf(reply_buff, "m%08x", thread_info[thread_info_report++]);
                }
                else
                {
                    sprintf(reply_buff, "l");
                }
                return 1;
            default:
                break;
        }
    }

    if (memcmp(cmd_buff, "qAttached", sizeof("qAttached") - 1) == 0)
    {
        strcpy(reply_buff, "1");
        return 1;
    }
    return 0;
}

static void gdb_cmd_query(char *cmd_buff, char *reply_buff)
{
    if (gdb_cmd_qthread(cmd_buff, reply_buff))
    {
        return;
    }
    if (arch_cmd_query(cmd_buff, reply_buff))
    {
        return;
    }
}

static int gdb_cmd_vcont(char *cmd_buff, char *reply_buff)
{
    if (memcmp(cmd_buff, "vCont?", sizeof("vCont?") - 1) == 0)
    {
        sprintf(reply_buff, "vCont;c;s");
    }
    return 0;
}

static void gdb_cmd_status(char *cmd_buff, char *reply_buff)
{
#ifdef ARCH_CPU_64BIT
    sprintf(reply_buff, "T05thread:%016lx;", (size_t)dbg_thread);
#else
    sprintf(reply_buff, "T05thread:%08x;", (size_t)dbg_thread);
#endif
}

extern struct arch_reg_info reg_info[];
int arch_get_reg(size_t *v, size_t no)
{
    size_t len = 0;
    size_t offset;

    if (no < REG_NR)
    {
        offset = reg_info[no].offset;
        len = reg_info[no].len;

        if (offset != ARCH_REG_OFFSET_INVALID)
        {
            memcpy(v, (char *)regs + offset, len);
        }
        else
        {
            memset(v, 0, len);
        }
    }
    return len;
}

void arch_reg_check(struct rt_hw_exp_stack *reg, size_t no, size_t *v);
void arch_set_reg(size_t *v, size_t no)
{
    size_t offset;

    if (no < REG_NR)
    {
        offset = reg_info[no].offset;
        if (offset != ARCH_REG_OFFSET_INVALID)
        {
            arch_reg_check(regs, no, v);
            memcpy((char *)regs + offset, v, reg_info[no].len);
        }
    }
}

static void gdb_cmd_getregs(char *cmd_buff, char *reply_buff)
{
    size_t var[8]; /* > max type size */
    int len;

    if (cmd_buff[0] == 'p') /* get one registers*/
    {
        char *p = &cmd_buff[1];
        size_t regno = 0;

        if (gdb_hex2long(&p, &regno))
        {
            len = arch_get_reg(var, regno);
            if (len > 0)
            {
                gdb_mem2hex((char *)var, reply_buff, len);
            }
            else
            {
                reply_err(reply_buff);
            }
            return;
        }
        strcpy(reply_buff, "INVALID");
        return;
    }
    else
    {
        int i;
        char *p = reply_buff;

        if(!regs)
        {
            reply_err(reply_buff);
            return;
        }

        for (i = 0; i < GEN_REG_NR; i++)
        {
            len = arch_get_reg(var, (size_t)i);
            gdb_mem2hex((char *)var, p, len);
            p += (len * 2); /* 1 byte is 2 hex */
        }
    }
}

//static struct rt_hw_exp_stack regs_check;
static void gdb_cmd_setregs(char *cmd_buff, char *reply_buff)
{
    size_t var[8]; /* > max type size */
    int len = 0;

    /*set one registers*/
    if (cmd_buff[0] == 'P')
    {
        char *p = &cmd_buff[1];
        size_t regno = 0;

        if (gdb_hex2long(&p, &regno) && *p++ == '=')
        {
            if (regno < REG_NR)
            {
                len = reg_info[regno].len;
                gdb_hex2mem(p, (char *)var, len);
                arch_set_reg(var, regno);
                reply_ok(reply_buff);
            }
            else
            {
                reply_err(reply_buff);
            }
        }
        return;
    }
    else
    {
        reply_err(reply_buff);
        return;
    }
}

#define MEM_BUFF_SIZE 16
static uint8_t mem_buff[MEM_BUFF_SIZE];

static void gdb_cmd_memread(char *cmd_buff, char *reply_buff)
{
    char *ptr = &cmd_buff[1];
    size_t length;
    size_t addr;

    if (gdb_hex2long(&ptr, &addr) > 0 && *ptr++ == ',' &&
            gdb_hex2long(&ptr, &length) > 0)
    {
        int off = 0;

        if(!valid_addr(addr))
        {
            reply_err(reply_buff);
            return;
        }

        while (length)
        {
            size_t tmplen = length;
            size_t dumplen;

            if (tmplen > MEM_BUFF_SIZE)
            {
                tmplen = MEM_BUFF_SIZE;
            }
            memcpy(mem_buff, (void *)(addr + off), tmplen);
            dumplen = tmplen;
            off += dumplen;
            length -= dumplen;
            tmplen = gdb_mem2hex((char *)mem_buff, reply_buff, dumplen);
            reply_buff += tmplen;
        }
    }
}

static int gdb_mem_write(void *addr, char *src_hex, size_t length, int cache_flush)
{
    int off = 0;

    while (length)
    {
        size_t tmplen = length;
        size_t putlen;

        if (tmplen > MEM_BUFF_SIZE)
        {
            tmplen = MEM_BUFF_SIZE;
        }
        putlen = gdb_hex2mem(src_hex, (char *)mem_buff, tmplen);
        memcpy((void *)((size_t)addr + off), (void *)mem_buff, putlen);
        off += putlen;
        length -= putlen;
        src_hex += (putlen * 2);
    }

    return 0;
}

static void gdb_cmd_memwrite(char *cmd_buff, char *reply_buff)
{
    char *ptr = &cmd_buff[1];
    size_t length;
    size_t addr;

    if (gdb_hex2long(&ptr, &addr) > 0 && *ptr++ == ',' &&
            gdb_hex2long(&ptr, &length) > 0 && *(ptr++) == ':')
    {
        if(!valid_addr(addr))
        {
            reply_err(reply_buff);
            return;
        }

        if (gdb_mem_write((void *)addr, ptr, length, 0) == 0)
        {
            reply_ok(reply_buff);
        }
        else
        {
            reply_err(reply_buff);
        }
    }
}

static void gdb_cmd_binwrite(char *cmd_buff, char *reply_buff)
{
    char *ptr = &cmd_buff[1];
    size_t length;
    size_t addr;

    if (gdb_hex2long(&ptr, &addr) > 0 && *ptr++ == ',' &&
            gdb_hex2long(&ptr, &length) > 0 && *(ptr++) == ':')
    {
        if(!valid_addr(addr))
        {
            reply_err(reply_buff);
            return;
        }

        memcpy((void *)addr, ptr, length);

        reply_ok(reply_buff);
    }
}

static void run_gdb_thread(void)
{
    rt_base_t level;
    rt_list_t *list;

    rt_kprintf("run_gdb_thread\n");
    level = rt_hw_interrupt_disable();
    list = dbg_module->t_grp.next;
    while (list != &dbg_module->t_grp)
    {
        rt_thread_t thread;

        thread = rt_list_entry(list, struct rt_thread, sibling);
        if ((thread->stat & RT_THREAD_STAT_MASK) != RT_THREAD_INIT)
        {
            rt_thread_resume(thread);
        }
        list = list->next;
    }
    rt_hw_interrupt_enable(level);
}

static void gdb_cmd_detachkill(char *cmd_buff, char *reply_buff)
{
    gdb_connected = 0;
    gdb_need_resume_program = 1;
    reply_ok(reply_buff);
}

static void gdb_cmd_breakpoint(char *cmd_buff, char *reply_buff)
{
    /*
     * Since GDB-5.3, it's been drafted that '0' is a software
     * breakpoint, '1' is a hardware breakpoint, so let's do that.
     */
    char *bpt_type = &cmd_buff[1];
    char *ptr = &cmd_buff[2];
    void *addr;
    size_t length;
    int error = -1;

    /*
     * Test if this is a hardware breakpoint, and
     * if we support it:
     */
    if (*(ptr++) != ',')
    {
        reply_err(reply_buff);
        return;
    }
    if (!gdb_hex2long(&ptr, (size_t *)&addr))
    {
        reply_err(reply_buff);
        return;
    }
    if (*(ptr++) != ',' ||
            !gdb_hex2long(&ptr, (size_t *)&length))
    {
        reply_err(reply_buff);
        return;
    }

    switch (*bpt_type)
    {
        case '0': /* Software breakpoint */
            if (cmd_buff[0] == 'Z')
                error = arch_set_sw_break(addr, length);
            else if (cmd_buff[0] == 'z')
                error = arch_remove_sw_break(addr);
            break;
        case '1': /* Hareware breakpoint */
            if (cmd_buff[0] == 'Z')
                error = arch_set_hw_break(addr);
            else if (cmd_buff[0] == 'z')
                error = arch_remove_hw_break(addr);
            break;
        case '2': /* Hareware write watchpint */
            if (cmd_buff[0] == 'Z')
                error = arch_set_hw_watch(addr, length, WATCHPOINT_MODE_WRITE);
            else if (cmd_buff[0] == 'z')
                error = arch_remove_hw_watch(addr, length, WATCHPOINT_MODE_WRITE);
            break;
        case '3': /* Hareware read watchpint */
            if (cmd_buff[0] == 'Z')
                error = arch_set_hw_watch(addr, length, WATCHPOINT_MODE_READ);
            else if (cmd_buff[0] == 'z')
                error = arch_remove_hw_watch(addr, length, WATCHPOINT_MODE_READ);
            break;
        case '4': /* Hareware access watchpint */
            if (cmd_buff[0] == 'Z')
                error = arch_set_hw_watch(addr, length, WATCHPOINT_MODE_ACCESS);
            else if (cmd_buff[0] == 'z')
                error = arch_remove_hw_watch(addr, length, WATCHPOINT_MODE_ACCESS);
            break;
        default:
            /*Error*/
            break;
    }

    if (error == 0)
        reply_ok(reply_buff);
    else
        reply_err(reply_buff);
}

static int found_thread(rt_thread_t thread)
{
    int found = 0;
    rt_base_t level;
    rt_list_t *list;

    level = rt_hw_interrupt_disable();
    list = dbg_module->t_grp.next;
    while (list != &dbg_module->t_grp)
    {
        rt_thread_t iter_thread;

        iter_thread = rt_list_entry(list, struct rt_thread, sibling);
        if (thread == iter_thread)
        {
            found = 1;
            break;
        }
        list = list->next;
    }
    rt_hw_interrupt_enable(level);
    return found;
}

static void gdb_cmd_thread_subsequent(char *cmd_buff, char *reply_buff)
{
    char *ptr = &cmd_buff[1];
    size_t tid;
    rt_thread_t thread;

    switch (*ptr)
    {
        case 'g':
        case 'c':
            ptr++;
            if (gdb_hex2long(&ptr, (size_t *)&tid))
            {
                if (tid == -1)
                {
                    reply_ok(reply_buff);
                    return;
                }
                else
                {
                    thread = (rt_thread_t)tid;
                    if (thread == 0)
                    {
                        reply_ok(reply_buff);
                        return;
                    }
                    if (found_thread((rt_thread_t)thread))
                    {
                        if(thread != dbg_thread)
                        {
                            arch_get_stack_regs(thread->sp, &tregs);
                            thread->regs = regs = &tregs;
                        }
                        reply_ok(reply_buff);
                        return;
                    }
                }
            }
            reply_err(reply_buff);
            break;
        default:
            break;
    }
    return;
}

static void gdb_cmd_thread(char *cmd_buff, char *reply_buff)
{
    char *ptr = &cmd_buff[1];
    size_t tid;
    rt_thread_t thread;

    if (gdb_hex2long(&ptr, &tid))
    {
        thread = (rt_thread_t)tid;
        if (found_thread((rt_thread_t)thread))
        {
            reply_ok(reply_buff);
        }
        else
        {
            reply_err(reply_buff);
        }
    }
}

static int do_gdb_cmd(void)
{
    char *cmd_buff;
    char *reply_buff;
    int length;
    struct rt_hw_exp_stack *s;
    char *ptr;

    cmd_buff = remcom_in_buffer + 1; //$
    reply_buff = remcom_out_buffer + 1; //$

    remcom_out_buffer[1] = 0;
    switch (cmd_buff[0])
    {
        case '?':/* gdbserial status */
            gdb_cmd_status(cmd_buff, reply_buff);
            break;
        case 'q':/* query command */
            gdb_cmd_query(cmd_buff, reply_buff);
            break;
        case 'p':       /* return the value of  a single CPU register */
        case 'g':       /* return the value of the CPU registers */
            gdb_cmd_getregs(cmd_buff, reply_buff);
            break;
        case 'P':       /* set the value of a single CPU registers - return OK */
        case 'G':       /* set the value of the CPU registers - return OK */
            gdb_cmd_setregs(cmd_buff, reply_buff);
            break;
        case 'm': /* mAA..AA,LLLL  Read LLLL bytes at address AA..AA */
            gdb_cmd_memread(cmd_buff, reply_buff);
            break;
        case 'X':/* XAA..AA,LLLL: Write LLLL escaped binary bytes at address AA.AA*/
            gdb_cmd_binwrite(cmd_buff, reply_buff);
            break;
        case 'M':/* MAA..AA,LLLL: Write LLLL bytes at address AA.AA return OK */
            gdb_cmd_memwrite(cmd_buff, reply_buff);
            break;
        case 'D': /* Debugger detach */
        case 'k': /* Debugger detach via kill */
            gdb_cmd_detachkill(cmd_buff, reply_buff);
            break;
        case 'C':/* Exception passing */
            if (cmd_buff[1] == '1' && cmd_buff[2] == '5')
            {
                gdb_cmd_detachkill(cmd_buff, reply_buff);
            }
            else if (cmd_buff[1] == '0' && cmd_buff[2] == '9')
            {
                return 1;
            }
            break;
        case 'z':/* Break point remove */
        case 'Z':/* Break point set */
            gdb_cmd_breakpoint(cmd_buff, reply_buff);
            break;
        case 'H':/* task related */
            gdb_cmd_thread_subsequent(cmd_buff, reply_buff);
            break;
        case 'T':/* Query thread status */
            gdb_cmd_thread(cmd_buff, reply_buff);
            break;
        case 'b': /* bBB...  Set baud rate to BB... */
            break;
        case 'v': /* bBB...  Set baud rate to BB... */
            if (gdb_cmd_vcont(cmd_buff, reply_buff))
            {
                return 1;
            }
            break;
        case 's': /* sAA..AA    step form address AA..AA (optional) */
            //arch_set_step((void *)regs->pc);
            s = (struct rt_hw_exp_stack *)dbg_thread->regs;
            //arch_get_next_pc(s->pc, &step_addr, &length);
            ptr = &cmd_buff[1];
            gdb_hex2long(&ptr, &step_addr);
            rt_kprintf("step_addr 0x%x\n");
            if(step_addr <= HEAP_BEGIN)
            {
                /* hard code, to do */
                length = 4;
                step_addr = s->pc + length;
            }
            arch_set_sw_break((void*)step_addr, length);
        case 'c': /* cAA..AA    Continue at address AA..AA (optional) */
            return 1;
        default:
            break;
    }
    return 0;
}

static void notify_dbgthread_stop(void)
{
    rt_base_t level;
    rt_list_t *list;

    level = rt_hw_interrupt_disable();
    list = dbg_module->t_grp.next;
    while (list != &dbg_module->t_grp)
    {
        rt_thread_t thread;

        thread = rt_list_entry(list, struct rt_thread, sibling);
        if((thread->stat & RT_THREAD_STAT_MASK) == RT_THREAD_SUSPEND)
        {
            rt_timer_stop(&thread->thread_timer);
        }
        else
        {
            _thread_suspend(thread);
        }
        list = list->next;
    }
    rt_hw_interrupt_enable(level);
    return;
}

static int is_all_dbgthread_stopped(void)
{
    rt_base_t level;
    rt_list_t *list;
    int ret = 1;

    level = rt_hw_interrupt_disable();
    list = dbg_module->t_grp.next;
    while (list != &dbg_module->t_grp)
    {
        rt_thread_t thread;

        thread = rt_list_entry(list, struct rt_thread, sibling);
        if ((thread->stat & RT_THREAD_SUSPEND_MASK) != RT_THREAD_SUSPEND
                && (thread->stat & RT_THREAD_STAT_MASK) != RT_THREAD_INIT)
        {
            ret = 0;
            break;
        }
        list = list->next;
    }
    rt_hw_interrupt_enable(level);
    return ret;
}

static rt_thread_t find_ready_thread(void)
{
    rt_base_t level;
    rt_list_t *list;

    level = rt_hw_interrupt_disable();
    list = dbg_module->t_grp.next;
    while (list != &dbg_module->t_grp)
    {
        rt_thread_t thread;

        thread = rt_list_entry(list, struct rt_thread, sibling);
        if ((thread->stat & RT_THREAD_STAT_MASK) == RT_THREAD_READY)
        {
            rt_hw_interrupt_enable(level);
            return thread;
        }
        list = list->next;
    }
    rt_hw_interrupt_enable(level);
    return dbg_module->t_main;
}

static void wait_all_dbgthread_stop(void)
{
    while (1)
    {
        notify_dbgthread_stop();
        if (is_all_dbgthread_stopped())
        {
            break;
        }
        rt_thread_mdelay(10);
    }
}

static void gdb_restart(void)
{
    arch_breakpoint_init();
    //arch_clear_step();
    gdb_connected = 0;
    if (gdb_need_resume_program)
    {
        dbg_module->debug = 0;
        run_gdb_thread();
    }
    gdb_need_resume_program = 0;
    gdb_switch_current_thread(0);
    dbg_module = RT_NULL;
    gdb_client_close();
    gdb_inuse = 0;
}

static void gdb_thread(void *parameter)
{
    gdb_thread_info thread_info;

    arch_enable_debug();

    while (1)
    {
        int state = GDBSRV_STATE_WAIT_THREAD_CONNECT;
        char c; //+

        while (1)
        {
            switch (state)
            {
            case GDBSRV_STATE_WAIT_THREAD_CONNECT:
                if (rt_mq_recv(gdb_mq, &thread_info, sizeof(thread_info), RT_WAITING_FOREVER) != RT_EOK)
                {
                    continue;
                }

                gdb_switch_current_thread(thread_info.thread);
                gdb_inuse = 1;
                gdb_need_resume_program = 1;
                gdbprintf("trap thread is %p\n", dbg_thread);
                dbg_module = (struct rt_dlmodule*)dbg_thread->module;
                gdbprintf("module is %p\n", dbg_module);
                gdbprintf("thread->pc is %p\n", (void *)regs->pc);
                if (arch_is_connect_breakpoint(thread_info.abt_ins))
                {
                    rt_kprintf("arch_is_connect_breakpoint\n");
                    state = GDBSRV_STATE_WAIT_GDB_CONNECT;
                    memcpy((void *)regs->pc, (void *)&dbg_module->bak_first_ins, BREAK_INSTR_SIZE);
                }
                break;
            case GDBSRV_STATE_WAIT_GDB_CONNECT:
                rt_kprintf("Waiting for GDB client ...\n");
                gdb_client_connect();
                if (gdb_connect_fd < 0)
                {
                    goto client_err;
                }
                while (1)
                {

                    GDB_CLIENT_RECV_WITH_CHECK(&c, 1); //+
                    if (c == '+')
                    {
                        state = GDBSRV_STATE_WAIT_GDB_CMD;
                        gdb_connected = 1;
                        break;
                    }
                }
                break;
            case GDBSRV_STATE_WAIT_GDB_CMD:
                recv_gdb_cmd(remcom_in_buffer);
                state = GDBSRV_STATE_PROCECE_GDB_CMD;
                break;
            case GDBSRV_STATE_WAIT_THEAD_TRAP:
                {
                    do
                    {
                        char *reply_buff = remcom_out_buffer + 1; //$

                        while(1)
                        {
                            ssize_t ret;

                            ret = gdb_client_recv(&c, 1, O_NONBLOCK);
                            if(ret == 1 && c == 0x3)
                            {
                                rt_thread_t t_ready;
                                gdbprintf("Received interrupt 0x%d\n", c);

                                t_ready = find_ready_thread();
                                wait_all_dbgthread_stop();
                                arch_get_stack_regs(t_ready->sp, &tregs);
                                t_ready->regs = &tregs;
                                gdb_switch_current_thread(t_ready);
                                sig = 1;
                                break;
                            }

                            if (rt_mq_recv(gdb_mq, &thread_info, sizeof(thread_info), RT_TICK_PER_SECOND) == RT_EOK)
                            {
                                break;
                            }
                        }

                        if(sig)
                        {
                            step_addr = bt(regs);
                            arch_set_sw_break((void*)step_addr, 4);
                            arch_activate_breakpoints();
                            run_gdb_thread();
                            sig = 0;
                            break;
                        }

                        if (thread_info.thread == RT_NULL)
                        {
                            /* program finished */
                            gdb_connected = 0;
                            gdb_need_resume_program = 0;
                            strcpy(reply_buff, "W00");
                        }
                        else
                        {
                            wait_all_dbgthread_stop();
                            gdb_switch_current_thread(thread_info.thread);

                            //arch_deactivate_step();
                            if(step_addr)
                            {
                                arch_remove_sw_break((void*)step_addr);
                                step_addr = 0;
                            }

                            switch (thread_info.notify_type)
                            {
                            case GDB_NOTIFIY_BREAKPOINT:
#ifdef ARCH_CPU_64BIT
                                sprintf(reply_buff, "T05thread:%016lx;", (size_t)dbg_thread);
#else
                                sprintf(reply_buff, "T05thread:%08x;", (size_t)dbg_thread);
#endif
                                break;
                            case GDB_NOTIFIY_WATCHPOINT:
                                {
                                    int amode;
                                    void *hit_addr;

                                    amode = arch_hw_watch_get_amode(thread_info.watch_addr, thread_info.rw, &hit_addr);
                                    switch (amode)
                                    {
                                    case WATCHPOINT_MODE_WRITE:
#ifdef ARCH_CPU_64BIT
                                        sprintf(reply_buff, "T05watch:%016lx;thread:%016lx;", (size_t)hit_addr, (size_t)dbg_thread);
#else
                                        sprintf(reply_buff, "T05watch:%08x;thread:%08x;", (size_t)hit_addr, (size_t)dbg_thread);
#endif
                                        break;
                                    case WATCHPOINT_MODE_READ:
#ifdef ARCH_CPU_64BIT
                                        sprintf(reply_buff, "T05rwatch:%016lx;thread:%016lx;", (size_t)hit_addr, (size_t)dbg_thread);
#else
                                        sprintf(reply_buff, "T05rwatch:%08x;thread:%08x;", (size_t)hit_addr, (size_t)dbg_thread);
#endif
                                        break;
                                    case WATCHPOINT_MODE_ACCESS:
#ifdef ARCH_CPU_64BIT
                                        sprintf(reply_buff, "T05awatch:%016lx;thread:%016lx;", (size_t)hit_addr, (size_t)dbg_thread);
#else
                                        sprintf(reply_buff, "T05awatch:%08x;thread:%08x;", (size_t)hit_addr, (size_t)dbg_thread);
#endif
                                        break;
                                    default:
                                        strcpy(reply_buff, "T05");
                                        break;
                                    }
                                }
                                break;
                            case GDB_NOTIFIY_CMD:
                                break;
                            default:
                                break;
                            }
                        }
                        state = GDBSRV_STATE_REPLY_GDB_CMD;
                    } while (0);
                }
                break;
            case GDBSRV_STATE_PROCECE_GDB_CMD:
                {
                    int wait_thread_trap;

                    wait_thread_trap = do_gdb_cmd();
                    if (wait_thread_trap)
                    {
                        state = GDBSRV_STATE_WAIT_THEAD_TRAP;
                        arch_activate_breakpoints();
                        //arch_activate_step();
                        run_gdb_thread();
                    }
                    else
                    {
                        state = GDBSRV_STATE_REPLY_GDB_CMD;
                    }
                }
                break;
            case GDBSRV_STATE_REPLY_GDB_CMD:
                REPLY_GDB_WITH_CHECK(remcom_out_buffer);
                GDB_CLIENT_RECV_WITH_CHECK(&c, 1); //+
                if (gdb_connected == 0)
                {
                    /* debug finish */
                    state = GDBSRV_STATE_WAIT_THREAD_CONNECT;
                    gdb_restart();
                }
                else
                {
                    state = GDBSRV_STATE_WAIT_GDB_CMD;
                }
                break;
            default:
                break;
            }
        }
client_err:
        gdbprintf("gdb client error!\n");
        arch_deactivate_breakpoints();
        gdb_restart();
    }
}

int gdbserver(void)
{
    rt_thread_t tid;

    info = rt_object_get_information(RT_Object_Class_Thread);
    gdb_mq = rt_mq_create(GDB_NAME, 32, 16, RT_IPC_FLAG_FIFO);
    if (!gdb_mq)
    {
        goto err;
    }

    tid = rt_thread_create(GDB_NAME, gdb_thread, RT_NULL, 1024 * 8, 1, 200);
    if (tid == RT_NULL)
    {
        goto err;
    }

#ifdef RT_USING_SMP
    rt_thread_control(tid, RT_THREAD_CTRL_BIND_CPU, (void*)0);
#endif

    rt_thread_startup(tid);
    return 0;

err:
    if (gdb_mq)
    {
        rt_mq_delete(gdb_mq);
    }
    return -1;
}
INIT_APP_EXPORT(gdbserver);

static int gdb(int argc, char **argv)
{
    char *cmd_buff = RT_NULL;
    char *p;
    size_t args_size;
    size_t i;
    int ret;

    args_size = 0;
    /* rebuild command buffer */
    for (i = 1; i < argc; i++)
    {
        args_size += (rt_strlen(argv[i]) + 1);
    }
    cmd_buff = (char *)rt_malloc(args_size);
    if (!cmd_buff)
    {
        rt_kprintf("DBG: Out of memory.\n");
        return -RT_ENOMEM;
    }
    p = cmd_buff;
    for (i = 1; i < argc; i++)
    {
        size_t len = rt_strlen(argv[i]);
        rt_memcpy(p, argv[i], len);
        p[len] = ' ';
        p += (len + 1);
    }
    cmd_buff[args_size - 1] = '\0';
    ret = msh_exec_module(1, cmd_buff, args_size);
    if (ret < 0)
    {
        rt_kprintf("DBG: Execute %s Error.\n", argv[1]);
    }
    rt_free(cmd_buff);
    return ret;
}

static int atoi_check(char *arg, int *err)
{
    int len;
    int ret;

    len = rt_strlen(arg);
    while (len--)
    {
        if (arg[len] < '0' || arg[len] > '9')
        {
            *err = -1;
            return 0;
        }
    }
    ret = atoi(arg);
    *err = 0;
    return ret;
}

int dbg(int argc, char **argv)
{
    /* get opts */
    rt_base_t level;
    int argc_no;
    int opt = SET_UNKNOW;
    int origin_gdb_inuse;
    int err;

    int device_mode = MODE_UNKNOW;
    pid_t pid = 0;
#ifdef RT_USING_SAL
    int listen_port = 0;
#endif
    char *dbg_file = RT_NULL;
    char *device_name = RT_NULL;
    int dbg_argc;
    int dbg_args_start = 0;
    char **dbg_argv = RT_NULL;

    level = rt_hw_interrupt_disable();
    origin_gdb_inuse = gdb_inuse;
    gdb_inuse = 1;
    rt_hw_interrupt_enable(level);
    if (origin_gdb_inuse)
    {
        rt_kprintf("Debug: The gdbserver is already in use.\n");
        return RT_EOK;
    }

    argc_no = 1;
    while (argc_no < argc)
    {
        switch (opt)
        {
        case SET_UNKNOW:
#ifdef RT_USING_SAL
            if (strcmp(argv[argc_no], "--port") == 0)
            {
                if (device_mode != MODE_UNKNOW)
                {
                    goto usage;
                }
                device_mode = MODE_TCP;
                opt = SET_LISTEN_PORT;
            }
            else
#endif
            if (strcmp(argv[argc_no], "--device") == 0)
            {
                if (device_mode != MODE_UNKNOW)
                {
                    goto usage;
                }
                device_mode = MODE_DEVICE;
                opt = SET_DEVICE_NAME;
            }
            else if (strcmp(argv[argc_no], "--args") == 0)
            {
                /* only support args with debug file */
                if (pid != 0)
                {
                    rt_kprintf("Debug: The option --args not support in attach mode.\n");
                    goto usage;
                }
                if (!dbg_file)
                {
                    rt_kprintf("Debug: The option --args must be specified after FILE.\n");
                    goto usage;
                }
                if (argc_no + 1 == argc)
                {
                    rt_kprintf("Debug: No args with option --args.\n");
                    goto usage;
                }
                opt = SET_ARGS;
                dbg_args_start = argc_no + 1;
            }
            else
            {
                /* debug file */
                if (dbg_file)
                {
                    goto usage;
                }
                dbg_file = rt_strdup(argv[argc_no]);
                if (!dbg_file)
                {
                    rt_kprintf("Debug: Out of memory.\n");
                    goto quit;
                }
            }
            break;
#ifdef RT_USING_SAL
        case SET_LISTEN_PORT:
            listen_port = atoi_check(argv[argc_no], &err);
            if (err != 0)
            {
                rt_kprintf("Debug: port is not a number.\n");
                goto usage;
            }
            opt = SET_UNKNOW;
            break;
#endif
        case SET_DEVICE_NAME:
            device_name = argv[argc_no];
            opt = SET_UNKNOW;
            break;
#ifndef RT_USING_SMP
        case SET_PID:
            pid = atoi_check(argv[argc_no], &err);
            if (err != 0)
            {
                rt_kprintf("Debug: pid is not a number.\n");
                goto usage;
            }
            opt = SET_UNKNOW;
            break;
#endif
        case SET_ARGS:
            if (argc_no + 1 == argc)
            {
                opt = SET_UNKNOW;
            }
            break;
        default:
            break;
        }
        argc_no++;
    }
    /* check args */
    if (opt != SET_UNKNOW)
    {
        goto usage;
    }
    if (device_mode == MODE_UNKNOW)
    {
        goto usage;
    }
    if (device_mode == MODE_DEVICE && device_name == RT_NULL)
    {
        goto usage;
    }
#ifdef RT_USING_SAL
    if (device_mode == MODE_TCP && listen_port == 0)
    {
        goto usage;
    }
#endif
    if (pid == 0 && dbg_file == RT_NULL)
    {
        goto usage;
    }
    if (pid != 0 && dbg_file != RT_NULL)
    {
        goto usage;
    }

    client_ops = RT_NULL;
    if (device_mode == MODE_DEVICE)
    {
        /* device */
        client_ops = &gdb_device_ops;
    }
#ifdef RT_USING_SAL
    else if (device_mode == MODE_TCP)
    {
        /* tcp */
        client_ops = &gdb_tcp_ops;
    }
#endif
    else
    {
        rt_kprintf("Debug: device mode not support.\n");
    }
    if (!client_ops)
    {
        goto usage;
    }

    /* args ok */
    err = -RT_ERROR;
    if (device_mode == MODE_DEVICE)
    {
        gdbprintf("Debug device: %s\n", device_name);
        dbg_device_name = rt_strdup(device_name);
        if (!dbg_device_name)
        {
            rt_kprintf("Debug: Out of memory.\n");
            goto quit;
        }
#ifdef PKG_USING_VCONSOLE        
        gdb_set_device(dbg_device_name);
#endif
        rt_sem_init(&gdb_rx, "grx", 0, 0);
    }
#ifdef RT_USING_SAL
    else
    {
        gdbprintf("Debug device: tcp port %d\n", listen_port);
        dbg_listen_port = listen_port;
    }
#endif

    gdb_client_open();
    if (gdb_connect_fd < 0)
    {
        rt_kprintf("Debug: GDB client connect failed.\n");
        goto quit;
    }
    gdbprintf("gdb_connect_fd = %d\n", gdb_connect_fd);

    dbg_argc = 2;
    if (dbg_args_start)
    {
        dbg_argc += argc - dbg_args_start;
    }
    dbg_argv = (char **)rt_malloc(dbg_argc * sizeof(char *));
    if (!dbg_argv)
    {
        rt_kprintf("Debug: Out of memory.\n");
        goto quit;
    }
    dbg_argv[0] = "dbg";
    if (dbg_file) /* debug a file */
    {
        int i;
        int run_pid = -1;

        dbg_argv[1] = dbg_file;
        for (i = 2; i < dbg_argc; i++)
        {
            dbg_argv[i] = argv[dbg_args_start + i - 2];
        }
#ifdef GDB_DEBUG
        gdbprintf("Debug args:\n");
        for (i = 0; i < dbg_argc; i++)
        {
            gdbprintf("arg[%d]: %s\n", i, dbg_argv[i]);
        }
#endif
        run_pid = gdb(dbg_argc, dbg_argv);
        if (run_pid >= 0)
        {
            err = RT_EOK;
        }
    }
quit:
    if (err != RT_EOK)
    {
        gdb_client_close();

#ifdef PKG_USING_VCONSOLE        
        if (vconsole_device)
        {
            vconsole_switch(dbg_device_name);
            if (vconsole_delete(vconsole_device) == RT_EOK)
            {
                vconsole_device = NULL;
            }
        }
#endif        
        if (dbg_device_name)
        {
            rt_free(dbg_device_name);
            dbg_device_name = RT_NULL;
        }
        if (dbg_file)
        {
            rt_free(dbg_file);
            dbg_file = RT_NULL;
        }
        dbg_listen_port = 0;
        gdb_inuse = 0;
        rt_kprintf("Debug: Debug failed.\n");
    }
    if (dbg_argv)
    {
        rt_free(dbg_argv);
    }
    return RT_EOK;
usage:
    gdb_inuse = 0;
    rt_kprintf("Usage:\n");
    rt_kprintf("dbg FILE --device dev [--args ... [&]]\n");
#ifdef RT_USING_SAL
    rt_kprintf("dbg FILE --port port [--args ... [&]]\n");
#endif
    return RT_EOK;
}
MSH_CMD_EXPORT(dbg, dbg);

uint32_t dbg_get_ins(void)
{
    return INS_BREAK_CONNECT;
}

int dbg_check_event(struct rt_hw_exp_stack *regs, unsigned long esr)
{
    return arch_check_debug_event(regs, esr);
}


