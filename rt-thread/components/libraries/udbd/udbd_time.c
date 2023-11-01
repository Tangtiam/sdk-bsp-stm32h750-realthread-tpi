/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2021-05-19     armink       the first version
 */

#include <udbd.h>
#include <rtthread.h>
#include <rtdevice.h>
#include <sys/time.h>

#define LOG_TAG "udbd.time"
#define LOG_LVL DBG_INFO
#include <rtdbg.h>

#ifdef RT_USING_RTC

static void time_sync(time_t now, int timezone)
{
    rt_device_t device;

    device = rt_device_find("rtc");
    if (device == RT_NULL)
    {
        return;
    }

    LOG_I("Sync timestamp: %d, timezone: %d", now, timezone);
    /* update to RTC device. */
    rt_device_control(device, RT_DEVICE_CTRL_RTC_SET_TIME, &now);

    //TODO timezone …Ë÷√
}
URPC_FFI_FUNC_STATIC_REGISTER_ALIAS(time_sync, time_sync)

#endif /* RT_USING_RTC */
