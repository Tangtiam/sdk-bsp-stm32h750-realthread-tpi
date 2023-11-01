/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2023-03-16     liboran       the first version
 */

#ifndef __RT_TRACE_H__
#define __RT_TRACE_H__

#include <string.h>
#include "rthw.h"
#include "rtconfig.h"
#include "rtdef.h"
#include "trace_fmt.h"

#define CTF_PARAM_SIZE(x) + sizeof(x)

#define CTF_PARAM_COPY(x)                       \
    memcpy(ctf_packet_point, &(x), sizeof(x));  \
    ctf_packet_point += sizeof(x);              \

#define VAL0(...)   __VA_ARGS__
#define VAL1(...)   VAL0(VAL0(VAL0(__VA_ARGS__)))
#define VAL2(...)   VAL1(VAL1(VAL1(__VA_ARGS__)))
#define VAL(...)    VAL2(VAL2(VAL2(__VA_ARGS__)))

#define TRANS_END(...)
#define TRANS_OUT

#define TRANS_GET_END()                 0, TRANS_END
#define TRANS_NEXT0(ignore, next, ...)  next TRANS_OUT
#define TRANS_NEXT1(ignore, next)       TRANS_NEXT0(ignore, next, 0)
#define TRANS_NEXT(ignore, next)        TRANS_NEXT1(TRANS_GET_END ignore, next)

#define TRANS1(func, now, next, ...) func(now) TRANS_NEXT(next, TRANS0)(func, next, __VA_ARGS__)
#define TRANS0(func, now, next, ...) func(now) TRANS_NEXT(next, TRANS1)(func, next, __VA_ARGS__)

#define TRANS(func, ...)    VAL(TRANS0(func, __VA_ARGS__, ()))

#define CTF_EVENT_SEND(id, ...)                                                                  \
        uint8_t *ctf_packet = trace_get_raw(0 + sizeof(rt_uint8_t)TRANS(CTF_PARAM_SIZE, ##__VA_ARGS__));          \
        if(ctf_packet)                                                                      \
        {                                                                                   \
            uint8_t *ctf_packet_point = ctf_packet;                                         \
            ctf_packet_point[0] = (rt_uint8_t)id;                                           \
            ctf_packet_point += sizeof(rt_uint8_t);                                         \
            TRANS(CTF_PARAM_COPY, ##__VA_ARGS__)                                            \
            trace_put_pdu(ctf_packet);                                                      \
        }
#define CTF_EVENT(id, ...)                          \
        rt_uint32_t timestamp = 0;                  \
        timestamp = trace_port_get_ts();            \
        CTF_EVENT_SEND(id, timestamp, __VA_ARGS__)

typedef struct
{
    char buf[RT_NAME_MAX];
} ctf_bounded_string_t;

void ctf_event_thread_switch_out(rt_uint8_t pri, ctf_bounded_string_t name);
void ctf_event_thread_switch_in(rt_uint8_t pri, ctf_bounded_string_t name);
void ctf_event_memory_malloc(rt_uint32_t ptr, rt_uint32_t size, ctf_bounded_string_t name);
void ctf_event_memory_free(rt_uint32_t ptr, ctf_bounded_string_t name);
void ctf_event_timer_enter(ctf_bounded_string_t name);
void ctf_event_timer_exit(ctf_bounded_string_t name);
void ctf_event_sem_take(ctf_bounded_string_t name);
void ctf_event_sem_release(ctf_bounded_string_t name);
void ctf_event_mutex_take(ctf_bounded_string_t name);
void ctf_event_mutex_release(ctf_bounded_string_t name);
void ctf_event_event_recv(ctf_bounded_string_t name, rt_uint32_t set);
void ctf_event_event_send(ctf_bounded_string_t name, rt_uint32_t set);
void ctf_event_mb_recv(ctf_bounded_string_t name);
void ctf_event_mb_send(ctf_bounded_string_t name);
void ctf_event_mq_recv(ctf_bounded_string_t name);
void ctf_event_mq_send(ctf_bounded_string_t name);

#endif /* __RT_TRACE_H__ */
