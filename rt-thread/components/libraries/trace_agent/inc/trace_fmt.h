/*
 * COPYRIGHT (C) 2022, Shanghai Real-Thread Technology Co., Ltd
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-06-01     realthread   the first version
 */
#ifndef TRACE_FMT_H__
#define TRACE_FMT_H__

enum trace_hashkey
{
    /* args */
    TH_ARGS        = 0,
    TH_ARGS_NAME,
    TH_ARGS_TIMEOUT,

    /* thread information */
    TH_THREAD_INFO = 0x100,
    TH_PROCESS_INFO = 0x101,

    /* thread switch */
    TH_THREAD_SW   = 0x200,
    TH_INT_ENTER,
    TH_INT_LEAVE,
    TH_TIMER_TIMEOUT,

    /* event */
    TH_TRACE_EVENT,
};

struct trace_args_name
{
    char name[8];
};

struct trace_header
{
    uint16_t hashkey;
    uint16_t length;
};

#endif
