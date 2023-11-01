/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2010-11-17     yi.qiu       first version
 */

#include <rtthread.h>
#include <rtm.h>

#include "dlmodule.h"

int dlclose(void *handle)
{
    struct rt_dlmodule *module;

    RT_ASSERT(handle != RT_NULL);

    module = (struct rt_dlmodule *)handle;

    dlmodule_delete(module);

    return RT_TRUE;
}
RTM_EXPORT(dlclose)
