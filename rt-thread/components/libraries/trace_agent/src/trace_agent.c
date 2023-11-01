/*
 * COPYRIGHT (C) 2022, Shanghai Real-Thread Technology Co., Ltd
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-06-01     realthread   the first version
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include <rtthread.h>

#include <trace_agent.h>
#include "trace_buffer.h"
#include "trace_internal.h"

#define BUFSZ               (1024 * 8)
#define TRACE_EVENT_STOP    0x01
#define TRACE_EVENT_START   0x02
#define TRACE_EVENT_SAVE    0x04

static struct trace_agent ta = {0};

static void tb_ready_handler(struct trace_buffer *tb, struct trace_block *block)
{
    rt_ubase_t level;

    level = trace_lock();
    rt_list_remove(&(block->list));
    rt_list_insert_before(&(ta.tb_list), &(block->list));
    ta.event_flag = 1;
    trace_unlock(level);

    return;
}

static void trace_agent(void *parameter)
{
    rt_err_t ret;
    rt_ubase_t level;
    rt_uint32_t recved;

    while (1)
    {
        ret = rt_event_recv(&ta.event, 0xff, RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR,
                            RT_WAITING_FOREVER, &recved);
        if (ret == RT_EOK)
        {
            if (recved & TRACE_EVENT_STOP)
            {
                level = trace_lock();
                /* stopping record */
                if (ta.status == TA_STATUS_RUNNING)
                {
                    ta.status = TA_STATUS_STOPPING;
                    trace_hook_set_enable(RT_FALSE);
                    trace_buffer_flush(ta.tb);
                }
                trace_unlock(level);
            }
            else if (recved & TRACE_EVENT_START)
            {
                /* reset trace buffer */
                trace_buffer_reset(ta.tb);

                /* start record */
                trace_file_open();

                level = trace_lock();
                ta.status = TA_STATUS_RUNNING;
                trace_hook_set_enable(RT_TRUE);
                trace_unlock(level);
            }

            if (recved & TRACE_EVENT_SAVE)
            {
                struct trace_block *block, *next;

                /* save to file */
                level = trace_lock();
                rt_list_for_each_entry_safe(block, next, &ta.tb_list, list)
                {
                    rt_list_remove(&(block->list));
                    trace_unlock(level);

                    /* skip block header */
                    trace_file_write((uint8_t*)(block + 1), block->length - sizeof(struct trace_block));
                    trace_release_block((uint8_t*)block);

                    level = trace_lock();
                }

                if ((ta.status == TA_STATUS_STOPPING) &&
                    rt_list_isempty(&(block->list)) &&
                    trace_buffer_get_wcount(ta.tb) == 0)
                {
                    ta.status = TA_STATUS_STOPPED;
                    trace_unlock(level);

                    trace_file_close();
                }
                else
                {
                    trace_unlock(level);
                }
            }
        }
    }
}

static void trace_event_idle_hook(void)
{
    if (ta.event_flag)
    {
        ta.event_flag = 0;
        rt_event_send(&ta.event, TRACE_EVENT_SAVE);
    }
}

int trace_agent_init(void)
{
    trace_port_init();
    /* set idle hook */
    rt_thread_idle_sethook(trace_event_idle_hook);

    rt_list_init(&(ta.tb_list));
    rt_event_init(&ta.event, "trace", RT_IPC_FLAG_FIFO);

    ta.tb = trace_buffer_create(1024 * 8, 4);
    trace_buffer_set_ready_handler(ta.tb, tb_ready_handler);

    trace_pdu_init(&ta);

    ta.tid = rt_thread_create("trace", trace_agent, NULL, 4096, 28, 20);
    if (ta.tid)
    {
        rt_thread_startup(ta.tid);
    }

    return 0;
}
INIT_APP_EXPORT(trace_agent_init);

/*
 * trace --desc fn  generate description json file
 * trace start      to start trace
 * trace stop       to stop trace
 */
int trace(int argc, char **argv)
{
    if (argc != 2 && argc != 3)
    {
        printf("RT-Thread Trace Event Agent\n");
        printf("Usage:\n");
        printf("trace start       to start trace\n");
        printf("trace stop        to stop trace\n");
        return 0;
    }

    if (argc == 2)
    {
        if (strcmp(argv[1], "start") == 0)
        {
            rt_event_send(&ta.event, TRACE_EVENT_START);
        }
        else if (strcmp(argv[1], "stop") == 0)
        {
            rt_event_send(&ta.event, TRACE_EVENT_STOP);
        }
    }

    return 0;
}
MSH_CMD_EXPORT(trace, trace event);
