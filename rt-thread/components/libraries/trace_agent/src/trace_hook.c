/*
 * COPYRIGHT (C) 2022, Shanghai Real-Thread Technology Co., Ltd
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-06-01     realthread   the first version
 */
#include <stdlib.h>
#include <rtthread.h>

#include <trace_agent.h>
#include <trace_fmt.h>

#include "trace_buffer.h"
#include "trace_internal.h"
#include "trace_ctf.h"

struct trace_hook
{
    uint32_t schedule_hook_enable : 1;
    uint32_t int_hook_enable : 1;
    uint32_t reserved : 30;

    rt_thread_t last_tid;
    rt_bool_t enable;
};
volatile struct trace_hook _hook = {0};

void int_enter_hook(void)
{
    if (_hook.enable == RT_TRUE)
    {
        TRACE_EVENT("INT");
    }

    return;
}

void int_leave_hook(void)
{
    if (_hook.enable == RT_TRUE)
    {
        TRACE_EVENT("INT");
    }

    return;
}

static void rt_scheduler_hook(struct rt_thread *from, struct rt_thread *to)
{
    ctf_bounded_string_t name_to = {"unknown"};
    ctf_bounded_string_t name_from = {"unknown"};

    if (_hook.enable == RT_FALSE) return;
    memcpy(&name_from, from->parent.name, RT_NAME_MAX);
    memcpy(&name_to, to->parent.name, RT_NAME_MAX);

    ctf_event_thread_switch_in(from->current_priority, name_from);
    ctf_event_thread_switch_out(to->current_priority, name_to);
}

static void rt_timer_enter_hook(struct rt_timer *timer)
{
    ctf_bounded_string_t name = {"unknown"};

    if (_hook.enable == RT_FALSE) return;

    memcpy(&name, timer->parent.name, RT_NAME_MAX);
    ctf_event_timer_enter(name);
}

static void rt_timer_exit_hook(struct rt_timer *timer)
{
    ctf_bounded_string_t name = {"unknown"};

    if (_hook.enable == RT_FALSE) return;

    memcpy(&name, timer->parent.name, RT_NAME_MAX);
    ctf_event_timer_exit(name);
}

static void rt_object_take_hook(struct rt_object *object)
{
    ctf_bounded_string_t name = {"unknown"};

    if (_hook.enable == RT_FALSE) return;
    memcpy(&name, object->name, RT_NAME_MAX);
    switch (object->type & (~RT_Object_Class_Static))
    {
        case RT_Object_Class_Semaphore:
            ctf_event_sem_take(name);
            break;
        case RT_Object_Class_Mutex:
            ctf_event_mutex_take(name);
            break;
        case RT_Object_Class_Event:
            ctf_event_event_recv(name, ((rt_event_t)object)->set);
            break;
        case RT_Object_Class_MailBox:
            ctf_event_mb_recv(name);
            break;
        case RT_Object_Class_MessageQueue:
            ctf_event_mq_recv(name);
            break;
        default:
            break;
    }
}

static void rt_object_put_hook(struct rt_object *object)
{
    ctf_bounded_string_t name = {"unknown"};

    if (_hook.enable == RT_FALSE) return;
    memcpy(&name, object->name, RT_NAME_MAX);
    switch (object->type & (~RT_Object_Class_Static))
    {
        case RT_Object_Class_Semaphore:
            ctf_event_sem_release(name);
            break;
        case RT_Object_Class_Mutex:
            ctf_event_mutex_release(name);
            break;
        case RT_Object_Class_Event:
            ctf_event_event_send(name, ((rt_event_t)object)->set);
            break;
        case RT_Object_Class_MailBox:
            ctf_event_mb_send(name);
            break;
        case RT_Object_Class_MessageQueue:
            ctf_event_mq_send(name);
            break;
        default:
            break;
    }
}

int trace_hook_set_enable(rt_bool_t enable)
{
    if (enable == RT_TRUE)
    {
        _hook.enable = RT_TRUE;

        rt_scheduler_sethook(rt_scheduler_hook);
        rt_timer_enter_sethook(rt_timer_enter_hook);
        rt_timer_exit_sethook(rt_timer_exit_hook);
        rt_object_take_sethook(rt_object_take_hook);
        rt_object_put_sethook(rt_object_put_hook);

#ifdef CTF_USE_TIMESTAMP
    _   rt_trace_timestamp_set(rt_trace_get_time);
#endif /* CTF_USE_TIMESTAMP */
        // rt_interrupt_enter_sethook(int_enter_hook);
        // rt_interrupt_leave_sethook(int_leave_hook);
    }
    else
    {
        _hook.enable = RT_FALSE;
        rt_scheduler_sethook(RT_NULL);
        rt_timer_enter_sethook(RT_NULL);
        rt_timer_exit_sethook(RT_NULL);
        rt_object_take_sethook(RT_NULL);
        rt_object_put_sethook(RT_NULL);        
    }

    return 0;
}
