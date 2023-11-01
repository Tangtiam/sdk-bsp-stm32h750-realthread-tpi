/*
 * COPYRIGHT (C) 2022, Shanghai Real-Thread Technology Co., Ltd
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-06-01     realthread   the first version
 */
#include <rtthread.h>

rt_weak rt_uint32_t trace_port_get_ts(void)
{
    return rt_tick_get();
}

rt_weak int trace_port_init(void)
{
    return 0;
}
