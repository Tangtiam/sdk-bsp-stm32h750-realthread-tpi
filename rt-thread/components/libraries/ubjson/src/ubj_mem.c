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

union ubj_object
{
    ubjsonw_t wubj;
    ubjsonr_t rubj;
};

#define UBJ_MEM_NO_FREE (0x1 << 0)
#define UBJ_MEM_STATIC  (0x1 << 1)

struct ubj_mem_private
{
    union ubj_object obj;
    uint32_t flags;
    uint8_t *begin;
    uint8_t *current;
    uint8_t *end;
};

static void ubj_mem_free(struct ubj_mem_private *ubj_mem)
{
    if ((ubj_mem->flags & UBJ_MEM_NO_FREE) == 0)
    {
        if ((ubj_mem->flags & UBJ_MEM_STATIC) == 0)
        {
            ubj_realloc(ubj_mem->begin, 0);
            ubj_mem->begin = NULL;
        }
        ubj_realloc(ubj_mem, 0);
    }
}

static void wmemclose(ubjsonw_t *ubj, void *private)
{
    ubj_mem_free(private);
}

static void rmemclose(ubjsonw_t *ubj, void *private)
{
    ubj_mem_free(private);
}

static size_t memwrite(ubjsonw_t *ubj, const void *data, size_t size, void *private)
{
    struct ubj_mem_private *ubj_mem = private;
    uint8_t *buff;
    size_t rsize;
    int wsize = size;

    if (wsize < 0 || (ubj_mem->flags & UBJ_MEM_STATIC))
        return 0;

    if (ubj_mem->end - ubj_mem->current < wsize)
    {
        rsize = (ubj_mem->end - ubj_mem->begin + wsize + 1023) & ~1023;
        buff = ubj_realloc(ubj_mem->begin, rsize);
        if (buff == NULL)
        {
            return 0;
        }
        ubj_mem->current = buff + (ubj_mem->current - ubj_mem->begin);
        ubj_mem->begin = buff;
        ubj_mem->end = buff + rsize;
    }
    memcpy(ubj_mem->current, data, wsize);
    ubj_mem->current += wsize;
    return wsize;
}

static size_t memread(ubjsonr_t *ubj, void *data, size_t size, void *private)
{
    struct ubj_mem_private *ubj_mem = private;
    int wsize = size;

    if (wsize < 0)
        return 0;

    if (ubj_mem->end - ubj_mem->current < wsize)
    {
        wsize = ubj_mem->end - ubj_mem->current;
    }
    if (wsize)
    {
        memcpy(data, ubj_mem->current, wsize);
        ubj_mem->current += wsize;
    }
    return size;
}

ubjsonw_t *ubj_write_memory(void)
{
    ubjsonw_t *ubj;
    struct ubj_mem_private *ubj_mem;
    uint8_t *buff;

    ubj_mem = ubj_realloc(NULL, sizeof(struct ubj_mem_private));
    if (ubj_mem == NULL)
    {
        return NULL;
    }
    ubj = &ubj_mem->obj.wubj;
    /* alloc mem */
    buff = ubj_realloc(NULL, 1024);
    if (buff == NULL)
    {
        ubj_realloc(ubj_mem, 0);
        return NULL;
    }
    /* init ubj object */
    ubj_write_init(ubj, memwrite, wmemclose, ubj_mem);
    ubj_mem->flags = 0;
    ubj_mem->begin = buff;
    ubj_mem->current = buff;
    ubj_mem->end = (uint8_t *)buff + 1024;
    return ubj;
}

uint8_t *ubj_get_memory(ubjsonw_t *ubj)
{
    return ((struct ubj_mem_private *)ubj)->begin;
}

uint8_t *ubj_get_memory_and_close(ubjsonw_t *ubj)
{
    struct ubj_mem_private *ubj_mem = (struct ubj_mem_private *)ubj;
    uint8_t *buff;

    ubj_mem->flags |= UBJ_MEM_NO_FREE;
    ubj_write_end(ubj);
    buff = ubj_mem->begin;
    ubj_realloc(ubj_mem, 0);
    return buff;
}

static ubjsonr_t *_ubj_read_memory(const void *buff, size_t len, uint32_t flags)
{
    ubjsonr_t *ubj;
    struct ubj_mem_private *ubj_mem;

    ubj_mem = ubj_realloc(NULL, sizeof(struct ubj_mem_private));
    if (ubj_mem == NULL)
    {
        return NULL;
    }
    ubj = &ubj_mem->obj.rubj;
    /* init ubj object */
    ubj_read_init(ubj, memread, (void (*)(ubjsonr_t *, void *))rmemclose, ubj_mem);
    ubj_mem->flags = flags;
    ubj_mem->begin = (uint8_t *)buff;
    ubj_mem->current = (uint8_t *)buff;
    ubj_mem->end = (uint8_t *)buff + len;
    return ubj;
}

ubjsonr_t *ubj_read_memory(const void *buff, size_t len)
{
    uint8_t *cpbuff;
    ubjsonr_t *ubj;

    /* alloc mem */
    cpbuff = ubj_realloc(NULL, len);
    if (cpbuff)
    {
        memcpy(cpbuff, buff, len);
        ubj = _ubj_read_memory(cpbuff, len, 0);
        if (ubj == NULL)
        {
            ubj_free(cpbuff);
            return NULL;
        }
        else
        {
            return ubj;
        }
    }
    else
    {
        return NULL;
    }
}

ubjsonr_t *ubj_read_static_memory(const void *buff, size_t len)
{
    return _ubj_read_memory(buff, len, UBJ_MEM_STATIC);
}
