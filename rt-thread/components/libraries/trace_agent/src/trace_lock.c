/*
 * COPYRIGHT (C) 2022, Shanghai Real-Thread Technology Co., Ltd
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-06-01     realthread   the first version
 */
#include <rthw.h>
#include <rtthread.h>

rt_ubase_t trace_lock(void)
{
    return rt_hw_interrupt_disable();
}

void trace_unlock(rt_ubase_t status)
{
    rt_hw_interrupt_enable(status);

    return;
}
