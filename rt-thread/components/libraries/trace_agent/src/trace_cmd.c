/*
 * COPYRIGHT (C) 2022, Shanghai Real-Thread Technology Co., Ltd
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-06-01     realthread   the first version
 */
#include <stdlib.h>
#include <rtthread.h>

#include <trace_agent.h>

#include "trace_buffer.h"
#include "trace_internal.h"
#include <trace_fmt.h>

static struct trace_agent *_agent = NULL;

int trace_event(const char *name)
{
    struct trace_pdu_event *pdu = NULL;

    if (_agent == NULL) return -1;

    pdu = (struct trace_pdu_event *)trace_buffer_get(_agent->tb, sizeof(struct trace_pdu_event));
    if (pdu)
    {
        pdu->hashkey = TH_TRACE_EVENT;
        pdu->length = sizeof(pdu->name);

        rt_strncpy(pdu->name, name, sizeof(pdu->name));

        trace_buffer_put(_agent->tb, (uint8_t *)pdu);
    }

    return 0;
}

int trace_duration(const char *name, int duration)
{
    return 0;
}

/*
 * this API return a basic trace PDU
 */
struct trace_pdu *trace_get_pdu(int hashkey, size_t size)
{
    struct trace_pdu *pdu = NULL;

    if (_agent == NULL) return NULL;

    pdu = (struct trace_pdu *)trace_buffer_get(_agent->tb, sizeof(struct trace_header) + size);
    if (pdu)
    {
        pdu->hashkey = hashkey;
        pdu->length  = size;
    }

    return pdu;
}

uint8_t* trace_get_raw(size_t size)
{
    return trace_buffer_get(_agent->tb, size);
}

int trace_put_pdu(uint8_t *pdu)
{
    trace_buffer_put(_agent->tb, pdu);

    return 0;
}

int trace_release_block(uint8_t *block)
{
    trace_buffer_release_block(_agent->tb, block);
    return 0;
}

int trace_pdu_init(struct trace_agent *ta)
{
    if (ta)
    {
        _agent = ta;
    }

    return 0;
}
