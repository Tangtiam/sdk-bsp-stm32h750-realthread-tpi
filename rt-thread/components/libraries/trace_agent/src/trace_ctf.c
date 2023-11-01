/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2023-03-16     liboran       the first version
 */

#include "trace_ctf.h"
#include "trace_agent.h"

typedef enum {
    CTF_EVENT_THREAD_SWITCHED_OUT   = 0x10,
    CTF_EVENT_THREAD_SWITCHED_IN    = 0x11,
    CTF_EVENT_TIMER_ENTER           = 0x12,
    CTF_EVENT_TIMER_EXIT            = 0x13,
    CTF_EVENT_SEM_TAKE              = 0x14,
    CTF_EVENT_SEM_RELEASE           = 0x15,
    CTF_EVENT_MUTEX_TAKE            = 0x16,
    CTF_EVENT_MUTEX_RELEASE         = 0x17,
    CTF_EVENT_EVENT_RECV            = 0x18,
    CTF_EVENT_EVENT_SEND            = 0x19,
    CTF_EVENT_MB_RECV               = 0x1A,
    CTF_EVENT_MB_SEND               = 0x1B,
    CTF_EVENT_MQ_RECV               = 0x1C,
    CTF_EVENT_MQ_SEND               = 0x1D
} ctf_event_t;

void ctf_event_thread_switch_out(rt_uint8_t pri, ctf_bounded_string_t name)
{
    CTF_EVENT(CTF_EVENT_THREAD_SWITCHED_OUT, pri, name);
}

void ctf_event_thread_switch_in(rt_uint8_t pri, ctf_bounded_string_t name)
{
    CTF_EVENT(CTF_EVENT_THREAD_SWITCHED_IN, pri, name);
}

void ctf_event_timer_enter(ctf_bounded_string_t name)
{
    CTF_EVENT(CTF_EVENT_TIMER_ENTER, name);
}

void ctf_event_timer_exit(ctf_bounded_string_t name)
{
    CTF_EVENT(CTF_EVENT_TIMER_EXIT, name);
}

void ctf_event_sem_take(ctf_bounded_string_t name)
{
    CTF_EVENT(CTF_EVENT_SEM_TAKE, name);
}

void ctf_event_sem_release(ctf_bounded_string_t name)
{
    CTF_EVENT(CTF_EVENT_SEM_RELEASE, name);
}

void ctf_event_mutex_take(ctf_bounded_string_t name)
{
    CTF_EVENT(CTF_EVENT_MUTEX_TAKE, name);
}

void ctf_event_mutex_release(ctf_bounded_string_t name)
{
    CTF_EVENT(CTF_EVENT_MUTEX_RELEASE, name);
}

void ctf_event_event_recv(ctf_bounded_string_t name, rt_uint32_t set)
{
    CTF_EVENT(CTF_EVENT_EVENT_RECV, name, set);
}

void ctf_event_event_send(ctf_bounded_string_t name, rt_uint32_t set)
{
    CTF_EVENT(CTF_EVENT_EVENT_SEND, name, set);
}

void ctf_event_mb_recv(ctf_bounded_string_t name)
{
    CTF_EVENT(CTF_EVENT_MB_RECV, name);
}

void ctf_event_mb_send(ctf_bounded_string_t name)
{
    CTF_EVENT(CTF_EVENT_MB_SEND, name);
}

void ctf_event_mq_recv(ctf_bounded_string_t name)
{
    CTF_EVENT(CTF_EVENT_MQ_RECV, name);
}

void ctf_event_mq_send(ctf_bounded_string_t name)
{
    CTF_EVENT(CTF_EVENT_MQ_SEND, name);
}

