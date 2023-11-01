/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2021-01-18     tyx          first implementation
 */

#include <stdlib.h>
#include <string.h>
#include "ubj.h"

#include <rtthread.h>
#if RT_VER_NUM <= 0x40003
#include <dfs_posix.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

union ubj_object
{
    ubjsonw_t wubj;
    ubjsonr_t rubj;
};

struct ubj_file_private
{
    union ubj_object obj;
    int fd;
};

static void wfileclose(ubjsonw_t *ubj, void *private)
{
    struct ubj_file_private *ubj_file = private;

    if (ubj_file->fd >= 0)
    {
        close(ubj_file->fd);
        //TODO: fd 置空
    }
    free(ubj_file);
}

static void rfileclose(ubjsonr_t *ubj, void *private)
{
    struct ubj_file_private *ubj_file = private;

    if (ubj_file->fd >= 0)
    {
        close(ubj_file->fd);
        //TODO: fd 置空
    }
    free(ubj_file);
}

static size_t filewrite(ubjsonw_t *ubj, const void *data, size_t size, void *private)
{
    struct ubj_file_private *ubj_file = private;
    return write(ubj_file->fd, data, size);
}

static size_t fileread(ubjsonr_t *ubj, void *data, size_t size, void *private)
{
    struct ubj_file_private *ubj_file = private;
    return read(ubj_file->fd, data, size);
}

ubjsonw_t *ubj_write_file(const char *path)
{
    ubjsonw_t *ubj;
    struct ubj_file_private *ubj_file;

    ubj_file = realloc(NULL, sizeof(struct ubj_file_private));
    if (ubj_file == NULL)
    {
        return NULL;
    }
    ubj = &ubj_file->obj.wubj;
    /* open file */
    ubj_file->fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0);
    if (ubj_file->fd < 0)
    {
        printf("open file:%s failed\n", path);
        free(ubj_file);
        return NULL;
    }
    /* init ubj object */
    ubj_write_init(ubj, filewrite, wfileclose, ubj_file);
    return ubj;
}

ubjsonr_t *ubj_read_file(const char *path)
{
    ubjsonr_t *ubj;
    struct ubj_file_private *ubj_file;

    ubj_file = realloc(NULL, sizeof(struct ubj_file_private));
    if (ubj_file == NULL)
    {
        return NULL;
    }
    ubj = &ubj_file->obj.rubj;
    /* open file */
    ubj_file->fd = open(path, O_RDONLY, 0);
    if (ubj_file->fd < 0)
    {
        printf("open file:%s failed\n", path);
        free(ubj_file);
        return NULL;
    }
    /* init ubj object */
    ubj_read_init(ubj, fileread, rfileclose, ubj_file);
    return ubj;
}
