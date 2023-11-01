/*
 * COPYRIGHT (C) 2022, Shanghai Real-Thread Technology Co., Ltd
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-06-01     realthread   the first version
 */
#ifndef TRACE_INTERNAL_H__
#define TRACE_INTERNAL_H__

#define TA_STATUS_RUNNING       0x01
#define TA_STATUS_STOPPING      0x02
#define TA_STATUS_STOPPED       0x03

struct trace_agent
{
    rt_thread_t tid;

    struct rt_event event;
    int event_flag;

    struct trace_buffer *tb;
    rt_list_t tb_list;

    rt_uint32_t status;
};

int trace_port_init(void);
int trace_hook_set_enable(rt_bool_t enable);

int trace_file_open(void);
int trace_file_write(void *buffer, int length);
int trace_file_close(void);

rt_ubase_t trace_lock(void);
void trace_unlock(rt_ubase_t status);

int trace_pdu_init(struct trace_agent *ta);

#endif
