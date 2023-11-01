/*
 * COPYRIGHT (C) 2022, Shanghai Real-Thread Technology Co., Ltd
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-06-01     realthread   the first version
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "trace_buffer.h"

struct trace_buffer
{
    uint16_t block_size;
    uint16_t block_num;

    uint8_t *block_buffer;
    uint8_t *block_end;

    struct trace_block *current_block;
    rt_list_t block_list;

    tb_ready_handler_t block_ready;
    uint32_t wandering_count;
};

rt_inline rt_ubase_t trace_buffer_lock(void)
{
    return rt_hw_interrupt_disable();
}

rt_inline void trace_buffer_unlock(rt_ubase_t status)
{
    rt_hw_interrupt_enable(status);
    return;
}

struct trace_buffer *trace_buffer_create(uint16_t size, uint16_t number)
{
    struct trace_buffer *buffer = NULL;

    if (size < sizeof(struct trace_block))
        goto __exit;

    buffer = (struct trace_buffer *)calloc(1, sizeof(struct trace_buffer));
    if (buffer)
    {
        buffer->block_num = number;
        buffer->block_size = size;
        rt_list_init(&(buffer->block_list));
        buffer->block_ready = NULL;
        buffer->wandering_count = 0;

        buffer->block_buffer = (uint8_t *)malloc(size * number);
        if (buffer->block_buffer == NULL)
        {
            /* allocate block buffer failed */
            free(buffer);
            buffer = NULL;
        }
        else
        {
            int index;

            /* set buffer end */
            buffer->block_end = buffer->block_buffer + buffer->block_size * buffer->block_num;

            /* add to block list */
            for (index = 0; index < number; index++)
            {
                struct trace_block *block;

                block = (struct trace_block *)(buffer->block_buffer + index * buffer->block_size);
                block->length = 0;
                block->ref_count = 0;
                rt_list_init(&(block->list));

                /* add to block list */
                rt_list_insert_before(&(buffer->block_list), &(block->list));
            }

            /* set to the first block */
            buffer->current_block = rt_list_entry(buffer->block_list.next, struct trace_block, list);
            buffer->current_block->ref_count = 0;
            buffer->current_block->length = sizeof(struct trace_block);

            rt_list_remove(&(buffer->current_block->list));
        }
    }

__exit:
    return buffer;
}

int trace_buffer_destroy(struct trace_buffer *buffer)
{
    if (buffer)
    {
        free(buffer->block_buffer);
        free(buffer);
    }

    return 0;
}

int trace_buffer_set_ready_handler(struct trace_buffer *buffer, tb_ready_handler_t handler)
{
    if (buffer)
    {
        buffer->block_ready = handler;
    }

    return 0;
}

uint8_t *trace_buffer_get(struct trace_buffer *buffer, size_t size)
{
    rt_ubase_t level;
    uint8_t *ptr = NULL;
    struct trace_block *block = NULL, *ready_block = NULL;

    /* set to align size */
    if (size == 0)
        return NULL;

    if (buffer)
    {
        level = trace_buffer_lock();
        if (size < buffer->block_size)
        {
            /* the current block */
            block = buffer->current_block;

            /* get a memory block */
            if ((block == NULL) || (block->length + size > buffer->block_size))
            {
                /* move to next block */
                if (!rt_list_isempty(&(buffer->block_list)))
                {
                    /* current block is ready */
                    if ((block->ref_count == 0) && (buffer->block_ready))
                    {
                        ready_block = block;
                        buffer->current_block = NULL;
                    }
                    else
                    {
                        buffer->wandering_count += 1;
                    }

                    /* allocate new block */
                    block = rt_list_entry(buffer->block_list.next, struct trace_block, list);
                    rt_list_remove(&(block->list));

                    block->length = sizeof(struct trace_block);
                    block->ref_count = 0;
                    buffer->current_block = block;
                }
                else
                {
                    block = NULL;
                }
            }

            if (block == NULL)
            {
                trace_buffer_unlock(level);
                goto __exit;
            }

            block->ref_count += 1;
            ptr = (uint8_t *)block + block->length;
            block->length += size;
        }
        trace_buffer_unlock(level);
    }

__exit:
    if (ready_block)
    {
        buffer->block_ready(buffer, ready_block);
    }

    return ptr;
}

int trace_buffer_put(struct trace_buffer *buffer, uint8_t *mem)
{
    int ret = -1;
    rt_ubase_t level;
    struct trace_block *block;

    if (mem > buffer->block_buffer && mem < buffer->block_end)
    {
        block = (struct trace_block *)(buffer->block_buffer + ((mem - buffer->block_buffer) / buffer->block_size) * buffer->block_size);

        level = trace_buffer_lock();
        block->ref_count -= 1;
        if ((block != buffer->current_block) &&
            (block->ref_count == 0) && buffer->block_ready)
        {
            buffer->wandering_count -= 1;
            trace_buffer_unlock(level);

            /* invoke block ready callback */
            buffer->block_ready(buffer, block);
            ret = 0;
        }
        else
        {
            trace_buffer_unlock(level);
        }
    }

    return ret;
}

int trace_buffer_flush(struct trace_buffer *buffer)
{
    rt_ubase_t level;
    struct trace_block *block, *ready_block = NULL;

    level = trace_buffer_lock();
    /* the current block */
    block = buffer->current_block;
    if (block && (block->length > sizeof(struct trace_block))) /* has valid data */
    {
        ready_block = block;
        buffer->current_block = NULL;

        /* move to next block */
        if (!rt_list_isempty(&(buffer->block_list)))
        {
            /* allocate new block */
            block = rt_list_entry(buffer->block_list.next, struct trace_block, list);
            rt_list_remove(&(block->list));

            block->length = sizeof(struct trace_block);
            block->ref_count = 0;
            buffer->current_block = block;
        }
    }
    trace_buffer_unlock(level);

    if (ready_block)
    {
        if ((ready_block->ref_count == 0) && (buffer->block_ready))
        {
            /* invoke block ready callback */
            buffer->block_ready(buffer, ready_block);
        }
    }

    return 0;
}

int trace_buffer_get_wcount(struct trace_buffer *buffer)
{
    int wcount = 0;

    if (buffer)
    {
        wcount = buffer->wandering_count;
    }

    return wcount;
}

int trace_buffer_reset(struct trace_buffer *buffer)
{
    rt_ubase_t level;

    level = trace_buffer_lock();
    rt_list_init(&(buffer->block_list));

    if (buffer->block_buffer)
    {
        int index;

        /* set buffer end */
        buffer->block_end = buffer->block_buffer + buffer->block_size * buffer->block_num;
        buffer->wandering_count = 0;

        /* add to block list */
        for (index = 0; index < buffer->block_num; index++)
        {
            struct trace_block *block;

            block = (struct trace_block *)(buffer->block_buffer + index * buffer->block_size);
            block->length = 0;
            block->ref_count = 0;
            rt_list_init(&(block->list));

            /* add to block list */
            rt_list_insert_before(&(buffer->block_list), &(block->list));
        }

        /* set to the first block */
        buffer->current_block = rt_list_entry(buffer->block_list.next, struct trace_block, list);
        buffer->current_block->ref_count = 0;
        buffer->current_block->length = sizeof(struct trace_block);

        rt_list_remove(&(buffer->current_block->list));
    }
    trace_buffer_unlock(level);

    return 0;
}

int trace_buffer_release_block(struct trace_buffer *buffer, uint8_t *block)
{
    int ret = -1;
    rt_ubase_t level;
    struct trace_block *tb = (struct trace_block *)block;

    if (tb && buffer)
    {
        level = trace_buffer_lock();
        if (block >= buffer->block_buffer && block < buffer->block_end)
        {
            rt_list_insert_before(&(buffer->block_list), &(tb->list));
            ret = 0;
        }
        trace_buffer_unlock(level);
    }

    return ret;
}
