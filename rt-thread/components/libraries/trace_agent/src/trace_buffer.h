/*
 * COPYRIGHT (C) 2022, Shanghai Real-Thread Technology Co., Ltd
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-06-01     realthread   the first version
 */
#ifndef TRACE_BUFFER_H__
#define TRACE_BUFFER_H__

#include <rthw.h>
#include <rtthread.h>
#include <rtservice.h>

#define TB_ALIGN_SIZE 4

struct trace_buffer;

struct trace_block
{
    rt_list_t list;

    uint16_t length;
    uint16_t ref_count;
};

typedef void (*tb_ready_handler_t)(struct trace_buffer *buffer, struct trace_block *block);

struct trace_buffer *trace_buffer_create(uint16_t size, uint16_t number);
int trace_buffer_destroy(struct trace_buffer *buffer);
int trace_buffer_flush(struct trace_buffer *buffer);
int trace_buffer_reset(struct trace_buffer *buffer);

int trace_buffer_get_wcount(struct trace_buffer *buffer);
int trace_buffer_set_ready_handler(struct trace_buffer *buffer, tb_ready_handler_t handler);

uint8_t *trace_buffer_get(struct trace_buffer *buffer, size_t size);
int trace_buffer_put(struct trace_buffer *buffer, uint8_t *mem);

int trace_buffer_release_block(struct trace_buffer *buffer, uint8_t *block);

#endif
