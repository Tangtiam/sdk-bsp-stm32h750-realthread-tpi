/*
 * Copyright (c) 2006-2020, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2020-05-25     chenyong     first version
 */

#include <mcf.h>
#include <mcf_trans.h>

#define DBG_TAG               "mcf.pkt"
#ifdef MCF_USING_DEBUG
#define DBG_LVL               DBG_LOG
#else
#define DBG_LVL               DBG_INFO
#endif /* MCF_DEBUG */
#include <rtdbg.h>

/**
 * static create MCF packet object.
 *
 * @param pkt MCF packet address
 * @param buf MCF packet buffer
 * @param len current packet buffer length
 * @param size maximum packet buffer size
 *
 * @return MCF packet object address
 */
struct mcf_pkt *mcf_pkt_make(struct mcf_pkt *pkt, void *buf, size_t len, size_t size)
{
    RT_ASSERT(pkt != RT_NULL);
    RT_ASSERT(buf != RT_NULL);
    RT_ASSERT(size && size <= MCF_PKT_MAX_SIZE);
    RT_ASSERT(len <= size);

    pkt->pbuf.buf = buf;
    pkt->pbuf.len = len;
    pkt->pbuf.size = size;

    return pkt;
}

/**
 * allocate MCF packet object.
 *
 * @param sid MCF session ID
 * @param size maximum packet buffer size
 *
 * @return != RT_NULL: MCF packet object
 *          = RT_NULL: allocate failed
 */
struct mcf_pkt *mcf_pkt_alloc(int sid, size_t size)
{
    struct mcf_pkt *pkt = RT_NULL;

    RT_ASSERT(sid >= 0);
    RT_ASSERT(size && size <= MCF_PKT_MAX_SIZE);

    pkt = rt_calloc(1, sizeof(struct mcf_pkt));
    if (pkt == RT_NULL)
    {
        LOG_E("no memory for MCF packet allocate");
        return RT_NULL;
    }

    pkt->pbuf.buf = rt_calloc(1, size);
    if (pkt->pbuf.buf == RT_NULL)
    {
        LOG_E("no memory for MCF packet buffer allocate.");
        rt_free(pkt);
        return RT_NULL;
    }

    pkt->pbuf.len = 0;
    pkt->pbuf.size = size;

    return pkt;
}

/**
 * free MCF packet object.
 *
 * @param pkt MCF packet object
 *
 * @return free success
 */
int mcf_pkt_free(struct mcf_pkt *pkt)
{
    if (pkt && pkt->pbuf.buf)
    {
        rt_free(pkt->pbuf.buf);
    }

    if (pkt)
    {
        rt_free(pkt);
    }

    return MCF_OK;
}

/**
 * set MCF packet object information.
 *
 * @param pkt MCF packet object
 * @param type packet type (0: MCF_PKT_TYPE_REQ, 1: MCF_PKT_TYPE_RSP)
 * @param flag packet flag (1: MCF_PKT_FLAG_NEED_RSP, 2: MCF_PKT_FLAG_NEED_ACK )
 *
 * @return MCF packet object
 */
struct mcf_pkt *mcf_pkt_info_set(struct mcf_pkt *pkt, uint8_t type, uint16_t flag)
{
    RT_ASSERT(pkt != RT_NULL);

    pkt->info.pkt_type = type;
    pkt->info.need_ack = flag & MCF_PKT_FLAG_NEED_ACK ? 1 : 0;
    pkt->info.need_rsp = flag & MCF_PKT_FLAG_NEED_RSP ? 1 : 0;

    return pkt;
}

/**
 * get MCF packet object information.
 *
 * @param pkt MCF packet object
 * @param type packet type address (0: MCF_PKT_TYPE_REQ, 1: MCF_PKT_TYPE_RSP)
 * @param flag packet flag address (1: MCF_PKT_FLAG_NEED_RSP, 2: MCF_PKT_FLAG_NEED_ACK )
 *
 * @return get information success
 */
int mcf_pkt_info_get(struct mcf_pkt *pkt, uint8_t *type, uint16_t *flag)
{
    RT_ASSERT(pkt != RT_NULL);

    if (type)
    {
        *type = pkt->info.pkt_type;
    }
    if (flag)
    {
        *flag = (pkt->info.need_ack) << 1 | pkt->info.need_rsp;
    }

    return MCF_OK;
}
