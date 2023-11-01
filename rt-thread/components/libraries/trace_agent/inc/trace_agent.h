/*
 * COPYRIGHT (C) 2022, Shanghai Real-Thread Technology Co., Ltd
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-06-01     realthread   the first version
 */
#ifndef TRACE_AGENT_H__
#define TRACE_AGENT_H__

struct trace_pdu
{
    uint16_t hashkey;
    uint16_t length;
};

struct trace_pdu_event
{
    uint16_t hashkey;
    uint16_t length;

    char name[8];
};

#include "trace_cmd.h"

int trace_agent_init(void);

int trace_agent_start(void);
int trace_agent_stop(void);

uint64_t trace_port_get_ts(void);

int trace_put_pdu(uint8_t *pdu);
struct trace_pdu *trace_get_pdu(int hashkey, size_t size);
uint8_t* trace_get_raw(size_t size);
int trace_release_block(uint8_t *block);

int trace_event(const char *name);

#endif
