/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2021-01-18     tyx          first implementation
 */

#include "rtthread.h"
#include <stdio.h>

rt_weak void *ubj_malloc(unsigned int size)
{
    return rt_malloc(size);
}

rt_weak void *ubj_realloc(void *rmem, unsigned int newsize)
{
    return rt_realloc(rmem, newsize);
}

rt_weak void ubj_free(void *rmem)
{
    rt_free(rmem);
}

rt_weak int ubj_printf(const char *format, ...)
{
    char buffer[128];
    int fm_len = 0;
    va_list ap;

    va_start(ap, format);
    fm_len = vsnprintf(buffer, 128, format, ap);
    va_end(ap);
    printf("%s", buffer);
    return fm_len;
}
