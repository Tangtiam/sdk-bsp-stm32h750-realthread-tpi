/*
 * COPYRIGHT (C) 2022, Shanghai Real-Thread Technology Co., Ltd
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-06-01     realthread   the first version
 */
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#define TRACE_FILE_FN "/trace.bin"

static int _trace_fd = -1;
int trace_file_open(void)
{
    int ret = 0;

    _trace_fd = open(TRACE_FILE_FN, O_RDWR | O_TRUNC);
    if (_trace_fd < 0)
    {
        ret = -1;
        printf("open file failed!\n");
    }
    else
    {
        printf("open trace file OK!");
    }

    return ret;
}

int trace_file_write(void *buffer, int length)
{
    write(_trace_fd, buffer, length);

    return 0;
}

int trace_file_close(void)
{
    if (_trace_fd >= 0)
    {
        close(_trace_fd);
        _trace_fd = -1;

        printf("close trace file OK!");
    }

    return 0;
}
