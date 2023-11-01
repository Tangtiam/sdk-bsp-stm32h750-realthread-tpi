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
#include <mcf_link.h>

#if defined(MCF_USING_REQ_EXEC) && defined(PKG_USING_THREAD_POOL)
#define MCF_USING_THREAD_POOL
#include <thread_pool.h>
#endif

#ifdef MCF_SECURITY_FASTLZ
#include <fastlz.h>
#endif

#define DBG_TAG               "mcf.trans"
#ifdef MCF_USING_DEBUG
#define DBG_LVL               DBG_LOG
#else
#define DBG_LVL               DBG_INFO
#endif /* MCF_DEBUG */
#include <rtdbg.h>

/* send ACK packet timeout */
#ifndef MCF_ACK_TIMOUT
#define MCF_ACK_TIMOUT                (RT_TICK_PER_SECOND / 2)
#endif
/* send response data timeout 1KB/s */
#define MCF_PKT_SEND_TIMEOUT          (rt_tick_from_millisecond(MCF_PKT_MAX_SIZE))

/* request packet retry transmission times */
#ifndef MCF_REQ_RERTY_TIMES
#define MCF_REQ_RERTY_TIMES            2
#endif

#ifndef MCF_REQ_THREAD_POOL_SIZE
#define MCF_REQ_THREAD_POOL_SIZE       2
#endif
#ifndef MCF_REQ_THREAD_POOL_STACK_SIZE
#define MCF_REQ_THREAD_POOL_STACK_SIZE 2048
#endif
#ifndef MCF_REQ_THREAD_POOL_PRIORITY
#define MCF_REQ_THREAD_POOL_PRIORITY   13
#endif
#ifndef MCF_REQ_THREAD_POOL_TASK_MAX
#define MCF_REQ_THREAD_POOL_TASK_MAX   20
#endif
#ifndef MCF_REQ_ADD_TASK_TIMEOUT
#define MCF_REQ_ADD_TASK_TIMEOUT       100
#endif

#define MCF_PARSER_STACK_SIZE          4096
#define MCF_PARSER_PRIORITY            11

#define MCF_RESULT_INDEX_LEN           31
#define MCF_RESULT_EVENT_DISCONNECT    (1U << 31)

#define PKT_INDEX(pkt_id)              (pkt_id % MCF_RESULT_INDEX_LEN)

/* MCF object */
struct mcf_object
{
    uint8_t id;                        /* device ID */

    char *recv_buf;                    /* receive buffer */
    size_t recv_bufsz;                 /* maximum receive buffer size */
    size_t cur_recv_bufsz;             /* current receive buffer size */
    rt_mutex_t recv_lock;              /* receive data lock */
    rt_mutex_t send_lock;              /* send data lock */

    rt_thread_t parser;                /* receive packet receive and parser */
#ifdef MCF_USING_REQ_EXEC
    mcf_req_exec_fn req_fn;            /* request execute function */
#ifdef MCF_USING_THREAD_POOL
    thread_pool req_pool;              /* request parser thread pool */
#endif /* MCF_USING_THREAD_POOL */
#endif /* MCF_USING_REQ_EXEC */

    void *user_data;                   /* user-specific data */
};

/* MCF packet session object */
struct mcf_session
{
    uint8_t sid;                       /* MCF session ID */
    uint8_t dst_id;                    /* destination device ID */
    rt_slist_t list;                   /* session list */
};

/* MCF request result object */
struct mcf_req_result
{
    rt_event_t event;                               /* result event notice */
    uint8_t pid_arry[MCF_RESULT_INDEX_LEN];         /* packet ID array */
    uint8_t did_arry[MCF_RESULT_INDEX_LEN];         /* destiny did array */
    struct mcf_d2d_pkt *pkt[MCF_RESULT_INDEX_LEN];  /* MCF D2D packet object array  */
};

/* MCF ack result object */
struct mcf_ack_result
{
    rt_event_t event;                             /* result event notice */
    uint8_t pid_arry[MCF_RESULT_INDEX_LEN];       /* packet ID array */
    uint8_t did_arry[MCF_RESULT_INDEX_LEN];       /* destiny did array */
};

static rt_bool_t g_init_ok = RT_FALSE, g_start_ok = RT_FALSE;
/* the global MCF object  */
static struct mcf_object *g_mcf_obj = RT_NULL;
/* the global request result object */
static struct mcf_req_result g_req_result = {0};
/* the global ack result object */
static struct mcf_ack_result g_ack_result = {0};
/* the global MCF session list */
static rt_slist_t g_mcf_session_list = RT_SLIST_OBJECT_INIT(&g_mcf_session_list);

static struct mcf_session *mcf_session_get(int sid);
static mcf_err_t mcf_d2d_pkt_send(uint8_t dst_id, struct mcf_d2d_pkt *pkt, int32_t timeout);
#ifdef MCF_USING_ACK
static mcf_err_t mcf_d2d_ack_pkt_send(struct mcf_d2d_pkt *d2d_pkt);
#endif

/* allocate MCF D2D packet object */
static struct mcf_d2d_pkt *mcf_d2d_pkt_alloc(uint8_t src_id, uint8_t dst_id, uint8_t pkt_id,
                                      uint8_t type, uint8_t flag, struct mcf_pbuf *pbuf)
{
    struct mcf_d2d_pkt *d2d_pkt = RT_NULL;

    d2d_pkt = rt_calloc(1, sizeof(struct mcf_d2d_pkt));
    if (d2d_pkt == RT_NULL)
    {
        return RT_NULL;
    }

    /* set D2D packet information */
    d2d_pkt->src_id    = src_id;
    d2d_pkt->dst_id    = dst_id;
    d2d_pkt->pkt_id    = pkt_id;
    d2d_pkt->pkt_type  = type;
    d2d_pkt->need_ack  = flag & MCF_PKT_FLAG_NEED_ACK ? 1 : 0;
    d2d_pkt->need_rsp  = flag & MCF_PKT_FLAG_NEED_RSP ? 1 : 0;
    d2d_pkt->len       = MCF_D2D_PKT_HEAD_LEN;

    /* set D2D packet buffer information */
    if (pbuf)
    {
        d2d_pkt->pbuf = pbuf;
        d2d_pkt->len += d2d_pkt->pbuf->len;
    }

    return d2d_pkt;
}

/* MCF D2D packet free */
static void mcf_d2d_pkt_free(struct mcf_d2d_pkt *pkt)
{
    rt_free(pkt);
}

/* get raw packet buffer by input D2D packet */
static mcf_err_t mcf_d2d_pkt_to_pbuf(struct mcf_d2d_pkt *pkt, struct mcf_pbuf *pbuf)
{
    char *buffer = RT_NULL;

    RT_ASSERT(pkt != RT_NULL);
    RT_ASSERT(pbuf != RT_NULL);
    RT_ASSERT(pbuf->size && (pbuf->size <= MCF_D2D_PKT_MAX_SIZE));

    /* fill packet buffer by D2D packet information */
    buffer = pbuf->buf;
    buffer[MCF_D2D_PKT_INDEX_SRC_ID]  = pkt->src_id;
    buffer[MCF_D2D_PKT_INDEX_DST_ID]  = pkt->dst_id;
    buffer[MCF_D2D_PKT_INDEX_PKT_ID]  = pkt->pkt_id;
    buffer[MCF_D2D_PKT_INDEX_PKT_INFO]   = (pkt->pkt_type << 6) | (pkt->need_ack << 5) | \
        (pkt->need_rsp << 4) | (pkt->priority << 2) | pkt->reserve;
    pbuf->len = MCF_D2D_PKT_HEAD_LEN;

#ifdef MCF_USING_SECURITY
    /* encrypt and ecompress receive packet data */
    if (pkt->pbuf && pkt->pbuf->len)
    {
        char *out_buffer = RT_NULL;
        size_t out_buf_len = 0;
        mcf_err_t result = 0;

        /* encrypt or compress packet buffer */
        result = mcf_security_pack(pkt->pbuf->buf, pkt->pbuf->len, (void **)&out_buffer, &out_buf_len);
        if (result != MCF_OK)
        {
            LOG_E("MCF packet buffer security pack failed(%d)", result);
            return result;
        }

        /* output packet buffer */
        rt_memcpy((char *)pbuf->buf + MCF_D2D_PKT_HEAD_LEN, out_buffer, out_buf_len);
        pbuf->len = MCF_D2D_PKT_HEAD_LEN + out_buf_len;

        rt_free(out_buffer);
    }
#else
    if (pkt->pbuf)
    {
        rt_memcpy(buffer + MCF_D2D_PKT_HEAD_LEN, pkt->pbuf->buf, pkt->pbuf->len);
        pbuf->len = MCF_D2D_PKT_HEAD_LEN + pkt->pbuf->len;
    }
#endif /* MCF_USING_SECURITY */

    return MCF_OK;
}

/* get MCF D2D packet by input raw packet buffer */
static mcf_err_t mcf_pbuf_to_d2d_pkt(struct mcf_pbuf *pbuf, struct mcf_d2d_pkt *pkt)
{
    char *buffer = RT_NULL;
    size_t buf_len = 0;

    RT_ASSERT(pkt != RT_NULL);
    RT_ASSERT(pkt->pbuf != RT_NULL);
    RT_ASSERT(pbuf != RT_NULL);

    buffer  = pbuf->buf;
    buf_len = pbuf->len;

    /* get D2D packet information by input raw packet buffer */
    pkt->src_id    = buffer[MCF_D2D_PKT_INDEX_SRC_ID];
    pkt->dst_id    = buffer[MCF_D2D_PKT_INDEX_DST_ID];
    pkt->pkt_id    = buffer[MCF_D2D_PKT_INDEX_PKT_ID];
    pkt->pkt_type  = (buffer[MCF_D2D_PKT_INDEX_PKT_INFO] >> 6);
    pkt->need_ack  = (buffer[MCF_D2D_PKT_INDEX_PKT_INFO] >> 5) & 0x1;
    pkt->need_rsp  = (buffer[MCF_D2D_PKT_INDEX_PKT_INFO] >> 4) & 0x1;
    pkt->priority  = (buffer[MCF_D2D_PKT_INDEX_PKT_INFO] >> 2) & 0x3 ;
    pkt->reserve   = (buffer[MCF_D2D_PKT_INDEX_PKT_INFO] & 0x3);
    pkt->len       = MCF_D2D_PKT_HEAD_LEN;

#ifdef MCF_USING_SECURITY
    /* dencrypt and decompress receive packet data */
    if (pbuf->len > MCF_D2D_PKT_HEAD_LEN)
    {
        char *out_buffer = RT_NULL;
        size_t out_buf_len = 0;
        mcf_err_t result = MCF_OK;

        /* unpack input raw packet buffer to D2D packet */
        result = mcf_security_unpack(buffer + MCF_D2D_PKT_HEAD_LEN,
                buf_len - MCF_D2D_PKT_HEAD_LEN, (void **)&out_buffer, &out_buf_len);
        if (result != MCF_OK)
        {
            LOG_E("MCF packet buffer security unpack failed(%d)", result);
            return result;
        }

        /* input D2D packet buffer filling */
        rt_memcpy(pkt->pbuf->buf, out_buffer, out_buf_len);
        pkt->pbuf->len = out_buf_len;
        pkt->len += pkt->pbuf->len;

        rt_free(out_buffer);
    }
#else
    /* input D2D packet buffer filling */
    rt_memcpy(pkt->pbuf->buf, buffer + MCF_D2D_PKT_HEAD_LEN, buf_len - MCF_D2D_PKT_HEAD_LEN);
    pkt->pbuf->len = buf_len - MCF_D2D_PKT_HEAD_LEN;
    pkt->len += pkt->pbuf->len;
#endif /* MCF_USING_SECURITY */

    return MCF_OK;
}

/* get MCF D2D packet type */
static enum mcf_d2d_pkt_type mcf_d2d_pkt_type_get(struct mcf_d2d_pkt *pkt)
{
    RT_ASSERT(pkt != RT_NULL);

    /* D2D packet is broadcast packet */
    if (pkt->dst_id == 0xFF)
    {
        return MCF_D2D_PKT_TYPE_BROAD;
    }
    /* D2D packet is proxy packet */
    else if (pkt->dst_id != mcf_object_get()->id)
    {
        return MCF_D2D_PKT_TYPE_PROXY;
    }

    return (enum mcf_d2d_pkt_type)pkt->pkt_type;
}

static rt_bool_t mcf_req_result_match(uint8_t pkt_id, uint8_t did)
{
    uint8_t pid_idx = PKT_INDEX(pkt_id);
    return g_req_result.pid_arry[pid_idx] == pkt_id &&
           g_req_result.did_arry[pid_idx] == did;
}

/* empty request result packet object */
static void mcf_req_result_empty(uint8_t pid_index)
{
    /* empty request result object infomation*/
    g_req_result.pid_arry[pid_index] = 0xFF;
    g_req_result.did_arry[pid_index] = 0xFF;

    if (g_req_result.pkt[pid_index])
    {
        g_req_result.pkt[pid_index]->pbuf = RT_NULL;
        g_req_result.pkt[pid_index] = RT_NULL;
    }
}

/* free request result packet object */
static void mcf_req_result_free(uint8_t pid_idx)
{
    struct mcf_req_result *result = &g_req_result;
    struct mcf_d2d_pkt *pkt;
    rt_base_t level;

    level = rt_hw_interrupt_disable();
    mcf_req_result_empty(pid_idx);
    pkt = result->pkt[pid_idx];
    rt_hw_interrupt_enable(level);
    if (pkt)
    {
        mcf_pbuf_free(pkt->pbuf);
        mcf_d2d_pkt_free(pkt);
    }
}

static rt_bool_t mcf_ack_result_match(uint8_t pkt_id, uint8_t did)
{
    uint8_t pid_idx = PKT_INDEX(pkt_id);
    return g_ack_result.pid_arry[pid_idx] == pkt_id &&
           g_ack_result.did_arry[pid_idx] == did;
}

static void mcf_ack_result_free(uint8_t pid_idx)
{
    g_ack_result.pid_arry[pid_idx] = 0xFF;
    g_ack_result.did_arry[pid_idx] = 0xFF;
}

static void mcf_link_status_callback(struct mcf_link *link, enum mcf_link_status_cb_type type)
{
    struct mcf_req_result *req_result = &g_req_result;
    struct mcf_ack_result *ack_result = &g_ack_result;

    if (type == MCF_LINK_STATUS_CB_TYPE_LINK_DOWN)
    {
        /* disconnect all waiting session */
        rt_event_send(ack_result->event, MCF_RESULT_EVENT_DISCONNECT);
        rt_event_send(req_result->event, MCF_RESULT_EVENT_DISCONNECT);
    }
}

/* wait response packet data or ACK within timeout */
static mcf_err_t mcf_rsp_event_wait(uint8_t did, uint8_t pkt_id, uint8_t pkt_type, struct mcf_d2d_pkt **out_pkt,
        int32_t timeout)
{
    struct mcf_req_result *req_result = &g_req_result;
    struct mcf_ack_result *ack_result = &g_ack_result;
    rt_base_t level = 0;
    rt_uint32_t rsp_event = 0;
    uint8_t pkt_index = 0xFF;
    mcf_err_t ret = MCF_OK;
    struct mcf_link *link = mcf_link_get_by_did(did);

    /* receive data with specified session ID */
    pkt_index = PKT_INDEX(pkt_id);

    RT_ASSERT(timeout != RT_WAITING_FOREVER);

__retry:

    if (did == MCF_DEV_ID_RECEIVER)
    {
        /* receive all session packet data in receive mode */
        if (pkt_type == MCF_D2D_PKT_TYPE_ACK)
        {
            if (rt_event_recv(ack_result->event, 0x7FFF,
                              RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR, timeout, &rsp_event) != RT_EOK)
            {
                ret = -MCF_RECV_TIMEOUT;
                goto _exit;
            }
        }
        else
        {
            if (rt_event_recv(req_result->event, 0x7FFF,
                              RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR, timeout, &rsp_event) != RT_EOK)
            {
                ret = -MCF_RECV_TIMEOUT;
                goto _exit;
            }

            // TODO receive multiple events
            /* get session packet ID by receive event notice */
            for (pkt_index = 0; (rsp_event = rsp_event >> 1) != 0; pkt_index++);

            /* get the request result packet */
            *out_pkt = req_result->pkt[pkt_index];
            return 0;
        }
    }
    else
    {
        mcf_link_set_status_callback(link, mcf_link_status_callback);

        if (pkt_type == MCF_D2D_PKT_TYPE_ACK)
        {
            if ((rt_event_recv(ack_result->event, (1 << pkt_index) | MCF_RESULT_EVENT_DISCONNECT,
                               RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR, timeout, &rsp_event) != RT_EOK) ||
                (rsp_event == MCF_RESULT_EVENT_DISCONNECT))
            {
                ret = -MCF_RECV_TIMEOUT;
                goto _exit;
            }

            /* request result packet ID check */
            level = rt_hw_interrupt_disable();
            if (!mcf_ack_result_match(pkt_id, did))
            {
                mcf_ack_result_free(pkt_index);
                rt_hw_interrupt_enable(level);
                goto __retry;
            }
            rt_hw_interrupt_enable(level);
        }
        else
        {
            RT_ASSERT(out_pkt != RT_NULL);
            if ((rt_event_recv(req_result->event, (1 << pkt_index) | MCF_RESULT_EVENT_DISCONNECT,
                               RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR, timeout, &rsp_event) != RT_EOK) ||
                (rsp_event == MCF_RESULT_EVENT_DISCONNECT))
            {
                ret = -MCF_RECV_TIMEOUT;
                goto _exit;
            }

            /* request result packet ID check */
            level = rt_hw_interrupt_disable();
            if (!mcf_req_result_match(pkt_id, did))
            {
                rt_hw_interrupt_enable(level);
                mcf_req_result_free(pkt_index);
                goto __retry;
            }

            /* get the request result packet */
            *out_pkt = req_result->pkt[pkt_index];
            rt_hw_interrupt_enable(level);

            mcf_link_set_status_callback(link, RT_NULL);
            return ret;
        }
    }

_exit:
    mcf_link_set_status_callback(link, RT_NULL);

    //level = rt_hw_interrupt_disable();
    if (pkt_type == MCF_D2D_PKT_TYPE_ACK)
        mcf_ack_result_free(pkt_index);
    else
        mcf_req_result_free(pkt_index);
    //rt_hw_interrupt_enable(level);

    return ret;
}

#ifdef MCF_USING_REQ_EXEC
/* execute request callback and send response data */
static void req_data_task(void *param)
{
    struct mcf_d2d_pkt *pkt = (struct mcf_d2d_pkt *)param;
    struct mcf_pkt in_pkt = {0}, out_pkt = {0};
    char *out_buff = RT_NULL;

    /* initialize input packet object */
    mcf_pkt_make(&in_pkt, pkt->pbuf->buf, pkt->pbuf->len, pkt->pbuf->len);
    mcf_pkt_info_set(&in_pkt, pkt->pkt_type, (pkt->need_ack << 1) | pkt->need_rsp);

    out_buff = rt_calloc(1, MCF_PKT_MAX_SIZE);
    if (out_buff == RT_NULL)
    {
        LOG_E("no memory for request task out buffer allocate.");
        goto _exit;
    }
    
    /* fill out packet information */
    out_pkt.info.dst_id = pkt->src_id;
    out_pkt.info.src_id = pkt->dst_id;
    out_pkt.info.pkt_id = pkt->pkt_id;
    mcf_pkt_make(&out_pkt, out_buff, 0, MCF_PKT_MAX_SIZE);
    mcf_pkt_info_set(&out_pkt, MCF_D2D_PKT_TYPE_RSP, pkt->need_ack << 1);
    /* save pkt info */
    rt_thread_self()->user_data = (rt_ubase_t)&out_pkt.info;
    /* execute request callback function */
    if (g_mcf_obj->req_fn)
    {
        g_mcf_obj->req_fn(&in_pkt, &out_pkt);
    }

    /* check need response */
    if (pkt->need_rsp)
    {
        struct mcf_d2d_pkt *rsp_pkt = RT_NULL;
        mcf_err_t err;

        rsp_pkt = mcf_d2d_pkt_alloc(pkt->dst_id, pkt->src_id,
                pkt->pkt_id, MCF_D2D_PKT_TYPE_RSP, pkt->need_ack << 1, &out_pkt.pbuf);
        if (rsp_pkt == RT_NULL)
        {
            LOG_E("create request task response packet failed.");
            goto _exit;
        }

        err = mcf_d2d_pkt_send(rsp_pkt->dst_id, rsp_pkt, MCF_PKT_SEND_TIMEOUT);
        mcf_d2d_pkt_free(rsp_pkt);
        if (err != MCF_OK)
        {
            LOG_E("request task send response packet failed. err:%d", err);
        }
    }

_exit:
    mcf_pbuf_free(pkt->pbuf);
    mcf_d2d_pkt_free(pkt);

    if (out_buff)
    {
        rt_free(out_buff);
    }
}

/* copy MCF D2D packet to the new one */
static struct mcf_d2d_pkt *mcf_d2d_pkt_dup(struct mcf_d2d_pkt *pkt)
{
    struct mcf_d2d_pkt *new_pkt = RT_NULL;

    new_pkt = rt_calloc(1, sizeof(struct mcf_d2d_pkt));
    if (new_pkt == RT_NULL)
    {
        return RT_NULL;
    }

    new_pkt->src_id   = pkt->src_id;
    new_pkt->dst_id   = pkt->dst_id;
    new_pkt->pkt_id   = pkt->pkt_id;
    new_pkt->pkt_type = new_pkt->pkt_type;
    new_pkt->need_ack = pkt->need_ack;
    new_pkt->need_rsp = pkt->need_rsp;
    new_pkt->len      = pkt->len;

    if (pkt->pbuf)
    {
        extern struct mcf_pbuf *mcf_pbuf_strdup(struct mcf_pbuf *pbuf);
        new_pkt->pbuf = mcf_pbuf_strdup(pkt->pbuf);
        if (new_pkt->pbuf == RT_NULL)
        {
            rt_free(new_pkt);
            return RT_NULL;
        }
    }

    return new_pkt;
}

/* receive packet type is request, execute request feature and send response data */
static mcf_err_t mcf_req_exec(struct mcf_d2d_pkt *pkt)
{
    struct mcf_d2d_pkt *new_pkt = RT_NULL;

    RT_ASSERT(pkt);

    new_pkt = mcf_d2d_pkt_dup(pkt);
    if (new_pkt == RT_NULL)
    {
        LOG_E("no memory for request execute allocate.");
        return -MCF_MEM_FULL;
    }

#ifdef MCF_USING_ACK
    if (new_pkt->need_ack)
    {
        if (mcf_d2d_ack_pkt_send(new_pkt) != MCF_OK)
        {
            LOG_E("request task send ACK packet failed.");
            return -MCF_FAILD;
        }
    }
#endif /* MCF_USING_ACK */

#ifdef PKG_USING_THREAD_POOL
    {
        thread_pool_t req_pool = &g_mcf_obj->req_pool;

        if (req_pool->add_task(req_pool, req_data_task, new_pkt, MCF_REQ_ADD_TASK_TIMEOUT) != THREAD_POOL_NO_ERR)
        {
            LOG_E("add MCF request pool task failed.");
            return -MCF_FAILD;
        }
    }
#else
    req_data_task((void *)new_pkt);
#endif /* PKG_USING_THREAD_POOL */

    return MCF_OK;
}
#endif /* MCF_USING_REQ_EXEC */

/* receive packet type is response, execute data storage and sending event notice */
static mcf_err_t mcf_rsp_exec(struct mcf_d2d_pkt *pkt)
{
    struct mcf_req_result *result = &g_req_result;
    struct mcf_d2d_pkt *d2d_pkt = RT_NULL;
    struct mcf_pbuf *pbuf = RT_NULL;
    rt_base_t level = 0;
    uint8_t pid_index = 0;

    pid_index = PKT_INDEX(pkt->pkt_id);
    
    if((result->event->set & (1 << pid_index)))
    {
        /* The response packet that is not waiting is not processed */
        return MCF_OK;
    }

    //level = rt_hw_interrupt_disable();
    if (result->pid_arry[pid_index] != 0xFF && result->pkt[pid_index])
    {
        /* free and empty exist request results object */
        mcf_req_result_free(pid_index);
    }
    //rt_hw_interrupt_enable(level);

    /* allocate packet buffer for data storage */
    pbuf = mcf_pbuf_alloc(pkt->pbuf->len);
    if (pbuf == RT_NULL)
    {
        LOG_W("NO Memory");
        return -MCF_MEM_FULL;
    }
    rt_memcpy(pbuf->buf, pkt->pbuf->buf, pkt->pbuf->len);
    pbuf->len = pkt->pbuf->len;

    d2d_pkt = mcf_d2d_pkt_alloc(pkt->src_id, pkt->dst_id, pkt->pkt_id,
                                pkt->pkt_type, pkt->need_ack << 1, pbuf);
    if (d2d_pkt == RT_NULL)
    {
        LOG_W("NO Memory");
        mcf_pbuf_free(pbuf);
        return -MCF_MEM_FULL;
    }

    /* set packet information to request result object */
    level = rt_hw_interrupt_disable();
    result->pid_arry[pid_index] = pkt->pkt_id;
    result->did_arry[pid_index] = pkt->src_id;
    result->pkt[pid_index] = d2d_pkt;
    rt_hw_interrupt_enable(level);

    /* send response packet event notice */
    if (rt_event_send(result->event, 1 << pid_index) != RT_EOK)
    {
        LOG_E("Response event send failed");
        return -MCF_FAILD;
    }

    return MCF_OK;
}

/* receive packet type is ACK, execute data storage and sending event notification */
static mcf_err_t mcf_ack_exec(struct mcf_d2d_pkt *pkt)
{
    int pid_index = 0;
    rt_base_t level = 0;
    struct mcf_ack_result *result = &g_ack_result;

    pid_index = PKT_INDEX(pkt->pkt_id);

    /* set packet information to result object */
    level = rt_hw_interrupt_disable();
    result->pid_arry[pid_index] = pkt->pkt_id;
    result->did_arry[pid_index] = pkt->src_id;
    rt_hw_interrupt_enable(level);

    /* send ACK packet event notice */
    if (rt_event_send(result->event, 1 << pid_index) != RT_EOK)
    {
        return -MCF_FAILD;
    }

    return MCF_OK;
}

/* receive broadcast data and execute function */
static mcf_err_t mcf_broadcast_exec(struct mcf_d2d_pkt *pkt)
{
    struct mcf_arp *arp = RT_NULL;
    struct mcf_link *link = RT_NULL;
    rt_slist_t *node = RT_NULL;
    rt_slist_t arp_list = mcf_link_arp_list_get();
    uint8_t port_list[MCF_LINK_PORT_MAX] = {0}, port_idx = 0, idx = 0;

    RT_ASSERT(pkt != RT_NULL);

    link = mcf_link_get_by_did(pkt->src_id);
    if (link == RT_NULL)
    {
        LOG_E("broadcast packet get link object by source ID(%d) failed.", pkt->src_id);
        return -MCF_FAILD;
    }

    /* find all broadcast device */
    rt_slist_for_each(node, &arp_list)
    {
        arp = rt_slist_entry(node, struct mcf_arp, list);

        /* not send broadcast data to source port device */
        if (arp->port_id != link->id)
        {
            for (idx = 0; idx < sizeof(port_list) && port_list[idx] != arp->port_id; idx++);

            if (idx == sizeof(port_list))
            {
                port_list[port_idx++] = arp->port_id;

                /* send broadcast packet buffer */
                if (mcf_d2d_pkt_send(arp->dev_id, pkt, MCF_PKT_SEND_TIMEOUT) != MCF_OK)
                {
                    LOG_E("broadcast packet send to device(%d) failed.", pkt->dst_id);
                    continue;
                }
            }
        }
    }

    return MCF_OK;
}

/* receive and process link data */
static void parser_entry(void *parameter)
{
    struct mcf_d2d_pkt d2d_pkt = {0};
    struct mcf_link *link = RT_NULL;

    while (g_init_ok)
    {
        mcf_link_notice_recv(&link, RT_WAITING_FOREVER);
        if (link == RT_NULL)
        {
            LOG_E("get data receive link object failed.");
            return;
        }
        
        struct mcf_pbuf *pbuf = RT_NULL;
        mcf_err_t result = 0;
        int type = 0;
        
        /* allocate packet buffer for link data receive */
        pbuf = mcf_pbuf_alloc(link->mtu);
        if (pbuf == RT_NULL)
        {
            LOG_E("no memory for MCF packet buffer allocate.");
            return;
        }

        /* receive packet buffer data by MCF link object */
        if (mcf_link_recv(link, pbuf) != MCF_OK)
        {
            LOG_E("receive packet buffer data by MCF link object failed.");
            goto _continue;
        }

        /* allocate D2D packet buffer */
        d2d_pkt.pbuf = mcf_pbuf_alloc(pbuf->len);
        if (d2d_pkt.pbuf == RT_NULL)
        {
            LOG_E("no memory for MCF D2D packet buffer allocate.");
            mcf_pbuf_free(pbuf);
            return;
        }

        /* get D2D packet data by packet buffer */
        result = mcf_pbuf_to_d2d_pkt(pbuf, &d2d_pkt);
        if (result != MCF_OK)
        {
            LOG_E("MCF receive packet buffer to d2d packet failed(%d).", result);
            goto _continue;
        }

        LOG_D("trans layer recv a packet (len: %d, id: %d) from %d", d2d_pkt.len, d2d_pkt.pkt_id, d2d_pkt.src_id);

        /* check the receive D2D packet information */
        if (d2d_pkt.src_id == g_mcf_obj->id)
        {
            LOG_E("MCF receive packet source ID(%d) error", d2d_pkt.src_id);
            goto _continue;
        }

        /* get the current D2D packet type */
        type = mcf_d2d_pkt_type_get(&d2d_pkt);
        switch (type)
        {
        case MCF_D2D_PKT_TYPE_PROXY:
            mcf_d2d_pkt_send(d2d_pkt.dst_id, &d2d_pkt, MCF_PKT_SEND_TIMEOUT);
            break;

        case MCF_D2D_PKT_TYPE_BROAD:
            mcf_broadcast_exec(&d2d_pkt);
            break;

#ifdef MCF_USING_REQ_EXEC
        case MCF_D2D_PKT_TYPE_REQ:
        {
            mcf_req_exec(&d2d_pkt);
            break;
        }
#endif /* MCF_USING_REQ_EXEC */
        
        case MCF_D2D_PKT_TYPE_RSP:
            mcf_rsp_exec(&d2d_pkt);
            break;
        
        case MCF_D2D_PKT_TYPE_ACK:
            mcf_ack_exec(&d2d_pkt);
            break;

        default:
            LOG_E("d2d packet type(%d) error.", type);
            break;
        }

_continue:
        /* free packet buffer */
        mcf_pbuf_free(pbuf);
        mcf_pbuf_free(d2d_pkt.pbuf);
    }
}

/* get MCF object */
struct mcf_object *mcf_object_get(void)
{
    return g_mcf_obj;
}

/* MCF request result object initialize */
static mcf_err_t mcf_req_result_init(void)
{
    struct mcf_req_result *result = &g_req_result;

    rt_memset(result->pid_arry, 0xFF, sizeof(result->pid_arry));
    rt_memset(result->did_arry, 0xFF, sizeof(result->did_arry));

    result->event = rt_event_create("req_event", RT_IPC_FLAG_FIFO);
    if (result->event == RT_NULL)
    {
        return -MCF_MEM_FULL;
    }
    return MCF_OK;
}

/* MCF ack result object initialize */
static mcf_err_t mcf_ack_result_init(void)
{
    struct mcf_ack_result *result = &g_ack_result;

    rt_memset(result->pid_arry, 0xFF, sizeof(result->pid_arry));
    rt_memset(result->did_arry, 0xFF, sizeof(result->did_arry));

    result->event = rt_event_create("ack_event", RT_IPC_FLAG_FIFO);
    if (result->event == RT_NULL)
    {
        return -MCF_MEM_FULL;
    }
    return MCF_OK;
}

/**
 * MCF object initialize.
 *
 * @param id device ID
 * @param recv_bu
 *
 * @return  0: MCF object initialize success
 *         -1: MCF object initialize failed
 *         -2: no memory for MCF object allocate
 */
int mcf_init(uint8_t id)
{
    if (g_init_ok == RT_TRUE)
    {
        LOG_E("MCF is already initialized.");
        return -MCF_FAILD;
    }

    /* create MCF object*/
    g_mcf_obj = rt_calloc(1, sizeof(struct mcf_object));
    if (g_mcf_obj == RT_NULL)
    {
        LOG_E("no memory for MCF object allocate.");
        return -MCF_MEM_FULL;
    }

    // TODO initialize recv_buf recv_lock send_lock

    g_mcf_obj->id = id;
    g_mcf_obj->parser = rt_thread_create("mcf_psr", parser_entry, RT_NULL,
                                       MCF_PARSER_STACK_SIZE, MCF_PARSER_PRIORITY, 10);
    if (g_mcf_obj->parser == RT_NULL)
    {
        LOG_E("MCF object parser thread create failed.");
        rt_free(g_mcf_obj);
        return -MCF_FAILD;
    }

#if defined(MCF_USING_REQ_EXEC) && defined(PKG_USING_THREAD_POOL)
    init_thread_pool(&g_mcf_obj->req_pool, "mcf_req",
            MCF_REQ_THREAD_POOL_SIZE, MCF_REQ_THREAD_POOL_STACK_SIZE, MCF_REQ_THREAD_POOL_PRIORITY, MCF_REQ_THREAD_POOL_TASK_MAX);
#endif

    /* initialize the global request result object */
    if (mcf_req_result_init() != MCF_OK)
    {
        LOG_E("MCF request result initialized failed.");
        rt_free(g_mcf_obj);
        return -MCF_FAILD;
    }

    /* initialize the global ack result object */
    if (mcf_ack_result_init() != MCF_OK)
    {
        LOG_E("MCF ack result initialized failed.");
        rt_event_delete(g_req_result.event);
        rt_free(g_mcf_obj);
        return -MCF_FAILD;
    }

    g_init_ok = RT_TRUE;
    LOG_I("MCF(V%s) initialized successfully.", MCF_SW_VER);

    return MCF_OK;
}

/**
 * MCF object start.
 */
rt_err_t mcf_start(void)
{
    RT_ASSERT(g_init_ok == RT_TRUE);

    if (g_start_ok == RT_TRUE)
    {
        LOG_W("MCF is already started.");
        return -RT_ERROR;
    }

    /* start MCF object parser */
    if (g_mcf_obj->parser)
    {
        if (RT_EOK != rt_thread_startup(g_mcf_obj->parser))
        {
            LOG_E("MCF protocol started failed.");
            return -RT_ERROR;
        }
    }

    g_start_ok = RT_TRUE;
    LOG_I("MCF protocol started successfully.");
    
    return RT_EOK;
}

/* get MCF session object by session ID */
static struct mcf_session *mcf_session_get(int sid)
{
    rt_base_t level;
    rt_slist_t *node = RT_NULL;
    struct mcf_session *session = RT_NULL;

    level = rt_hw_interrupt_disable();
    /* get current session by ID in session list */
    rt_slist_for_each(node, &g_mcf_session_list)
    {
        struct mcf_session *s = rt_slist_entry(node, struct mcf_session, list);
        if (s->sid == sid)
        {
            session = s;
            break;
        }
    }
    rt_hw_interrupt_enable(level);
    return session;
}

/* check this MCF session ID is available */
static rt_bool_t mcf_session_id_is_available(int sid)
{
    rt_base_t level;
    rt_slist_t *node = RT_NULL;

    level = rt_hw_interrupt_disable();
    /* get current session by ID in session list */
    rt_slist_for_each(node, &g_mcf_session_list)
    {
        struct mcf_session *s = rt_slist_entry(node, struct mcf_session, list);
        /* packet index MUST NOT equal */
        if (PKT_INDEX(s->sid) == PKT_INDEX(sid))
        {
            rt_hw_interrupt_enable(level);
            return RT_FALSE;
        }
    }
    rt_hw_interrupt_enable(level);
    return RT_TRUE;
}

/**
 * open and create MCF session for data communicate
 *
 * @param dst_id destination device ID
 *
 * @return >0: MCF session ID
 *         <0: open MCF session failed
 */
int mcf_d2d_open(uint8_t dst_id)
{
    rt_base_t level;
    static uint8_t mcf_sid = 0;
    struct mcf_session *session = RT_NULL;
    int retry;

    RT_ASSERT(g_start_ok == RT_TRUE);

    session = rt_calloc(1, sizeof(struct mcf_session));
    if (session == RT_NULL)
    {
        LOG_E("no memory for MCF session create.");
        return -MCF_FAILD;
    }

    level = rt_hw_interrupt_disable();
    retry = rt_slist_len(&g_mcf_session_list) + 1;
    RT_ASSERT(retry > 0 && retry < 256);
    while (retry--)
    {
        /* MCF session information fill */
        session->sid = (mcf_sid++) % 0xFF;
        if (mcf_session_id_is_available(session->sid))
        {
            break;
        }
    }
    session->dst_id = dst_id;
    rt_slist_init(&session->list);
    /* MCF session list add */
    rt_slist_append(&g_mcf_session_list, &(session->list));
    rt_hw_interrupt_enable(level);
    return session->sid;
}

/* check input MCF packet length */
static void mcf_pkt_len_check(struct mcf_pkt *pkt)
{
    struct mcf_pbuf *pbuf = &(pkt->pbuf);
    RT_ASSERT(pbuf->size <= MCF_PKT_MAX_SIZE);
}

/* send MCF D2D packet within timeout */
static mcf_err_t mcf_d2d_pkt_send(uint8_t dst_id, struct mcf_d2d_pkt *d2d_pkt, int32_t timeout)
{
    struct mcf_pbuf *pbuf = RT_NULL;
    struct mcf_link *link = RT_NULL;
    mcf_err_t result;
    
    size_t pbuf_len = d2d_pkt->len;
    
#if defined(MCF_SECURITY_FASTLZ) && defined(MCF_SECURITY_AES256)
    pbuf_len = MCF_AES_ALIGN(pbuf_len) + MCF_AES_DATA_HEAD +
            FASTLZ_BUFFER_PADDING(MCF_AES_ALIGN(pbuf_len) + MCF_AES_DATA_HEAD);
#elif defined(MCF_SECURITY_FASTLZ)
    pbuf_len = pbuf_len + FASTLZ_BUFFER_PADDING(pbuf_len);
#elif defined(MCF_SECURITY_AES256)
    pbuf_len = MCF_AES_ALIGN(pbuf_len) + MCF_AES_DATA_HEAD;
#endif

    /* allocate packet buffer for data send */
    pbuf = mcf_pbuf_alloc(pbuf_len);
    if (pbuf == RT_NULL)
    {
        LOG_E("no memory for MCF packet buffer allocate.");
        return -MCF_MEM_FULL;
    }

    /* MCF D2D packet to packet buffer */
    mcf_d2d_pkt_to_pbuf(d2d_pkt, pbuf);
    
   /* get MCF link object by destination device ID */
    link = mcf_link_get_by_did(dst_id);
    if (link == RT_NULL)
    {
        LOG_E("get MCF link by device ID(%d) failed.", d2d_pkt->dst_id);
        result = -MCF_FAILD;
        goto _exit;
    }

    LOG_D("trans layer send a packet (len: %d, id: %d) to %d", d2d_pkt->len, d2d_pkt->pkt_id, dst_id);
    /* MCF link send packet buffer data within timeout */
    result = mcf_link_send(link, pbuf, timeout);
    if (result != MCF_OK)
    {
        LOG_E("MCF link send packet failed.");
        goto _exit;
    }

#ifdef MCF_USING_ACK
    /* check session flag, wait packet ACK */
    if (d2d_pkt->need_ack &&
            (d2d_pkt->pkt_type == MCF_D2D_PKT_TYPE_REQ || d2d_pkt->pkt_type == MCF_D2D_PKT_TYPE_RSP))
    {
        result = mcf_rsp_event_wait(d2d_pkt->dst_id,
            d2d_pkt->pkt_id, MCF_D2D_PKT_TYPE_ACK, RT_NULL, MCF_ACK_TIMOUT);
        if (result != MCF_OK)
        {
            goto _exit;
        }
    }
#endif /* MCF_USING_ACK */

_exit:
    mcf_pbuf_free(pbuf);

    return result;
}

/* receive packet is broadcast type, send the packet to all connected devices */
static mcf_err_t mcf_d2d_broadcast_send(struct mcf_d2d_pkt *pkt, int32_t timeout)
{
    struct mcf_arp *arp = RT_NULL;
    rt_slist_t *node = RT_NULL;
    rt_slist_t arp_list = mcf_link_arp_list_get();
    uint8_t port_list[MCF_LINK_PORT_MAX] = {0}, port_idx = 0, idx = 0;

    RT_ASSERT(pkt != RT_NULL);

    /* check MCF D2D packet information, broadcast packet not support response and ACK */
    if (pkt->need_ack || pkt->need_rsp)
    {
        LOG_E("send broadcast packet not support response or ACK.");
        return -MCF_FAILD;
    }

    /* find all broadcast device */
    rt_slist_for_each(node, &arp_list)
    {
        arp = rt_slist_entry(node, struct mcf_arp, list);

        for (idx = 0; idx < sizeof(port_list) && port_list[idx] != arp->port_id; idx++);

        if (idx == sizeof(port_list))
        {
            port_list[port_idx++] = arp->port_id;

            /* send broadcast packet buffer */
            if (mcf_d2d_pkt_send(arp->dev_id, pkt, timeout) != MCF_OK)
            {
                LOG_E("broadcast packet send to device(%d) failed.", pkt->dst_id);
                continue;
            }
        }
    }

    return MCF_OK;
}

/**
 * receive MCF packet object within timeout.
 *
 * @param sid MCF session ID
 * @param pkt send MCF packet object
 * @param timout receive timeout
 *
 * @return 0: close session success
 *        -1: get session by session ID error
 *        -2: no memory for packet allocate
 *        -3: send packet timeout
 *        -4: retry to send packet error
 */
int mcf_d2d_send(int sid, struct mcf_pkt *pkt, int32_t timeout)
{
    struct mcf_session *session = RT_NULL;
    struct mcf_d2d_pkt *d2d_pkt = RT_NULL;
    mcf_err_t result = 0;
    uint8_t retry = MCF_REQ_RERTY_TIMES;

    RT_ASSERT(sid >= 0);
    RT_ASSERT(pkt != RT_NULL);
    RT_ASSERT(g_start_ok == RT_TRUE);

    /* check MCF packet buffer length */
    mcf_pkt_len_check(pkt);

    /* get MCF session by session ID */
    session = mcf_session_get(sid);
    if (session == RT_NULL)
    {
        LOG_E("get MCF session by session ID(%d) failed.", sid);
        return -MCF_FAILD;
    }

    /* filling input packet information */
    pkt->info.src_id = g_mcf_obj->id;
    pkt->info.dst_id = session->dst_id;
    pkt->info.pkt_id = session->sid;

    /* allocate MCF D2D packet by input packet data and information */
    d2d_pkt = mcf_d2d_pkt_alloc(g_mcf_obj->id, session->dst_id, session->sid,
                                pkt->info.pkt_type, (pkt->info.need_ack << 1) | pkt->info.need_rsp, &pkt->pbuf);
    if (d2d_pkt == RT_NULL)
    {
        LOG_E("no memory for MCF D2D packet allocate.");
        return -MCF_MEM_FULL;
    }

    if (d2d_pkt->dst_id == MCF_DEV_ID_BROADCAST)
    {
        /* send broadcast packet */
        if (mcf_d2d_broadcast_send(d2d_pkt, MCF_PKT_SEND_TIMEOUT) != MCF_OK)
        {
            mcf_d2d_pkt_free(d2d_pkt);
            return -MCF_FAILD;
        }
    }
    else
    {
        while (retry--)
        {
            /* send MCF D2D packet data */
            result = mcf_d2d_pkt_send(session->dst_id, d2d_pkt, timeout);
            if (result == MCF_OK)
            {
                break;
            }

            LOG_W("MCF send D2D packet failed(%d), try to retry(%d)...", result, retry);
            if (retry == 0)
            {
                /* retry times exceeded */
                LOG_E("MCF send D2D packet failed(%d).", result);
                mcf_d2d_pkt_free(d2d_pkt);
                return -MCF_SEND_RETRY_ERR;
            }
        }
    }

    if (d2d_pkt)
    {
        mcf_d2d_pkt_free(d2d_pkt);
    }

    return MCF_OK;
}

#ifdef MCF_USING_ACK
/* send ACK packet by receive MCF D2D packet */
static mcf_err_t mcf_d2d_ack_pkt_send(struct mcf_d2d_pkt *d2d_pkt)
{
    RT_ASSERT(d2d_pkt);

    /* check receive packet flag, wait response ACK packet */
    if (d2d_pkt->pkt_type == MCF_D2D_PKT_TYPE_REQ || d2d_pkt->pkt_type == MCF_D2D_PKT_TYPE_RSP)
    {
        struct mcf_d2d_pkt *ack_pkt = RT_NULL;
        mcf_err_t result = MCF_OK;

        ack_pkt = mcf_d2d_pkt_alloc(d2d_pkt->dst_id, d2d_pkt->src_id, d2d_pkt->pkt_id,
                                    MCF_D2D_PKT_TYPE_ACK, 0, RT_NULL);
        if (ack_pkt == RT_NULL)
        {
            LOG_E("no memory for MCF D2D ACK packet allocate.");
            return -MCF_MEM_FULL;
        }

        result = mcf_d2d_pkt_send(ack_pkt->dst_id, ack_pkt, MCF_ACK_TIMOUT);
        if (result != MCF_OK)
        {
            LOG_E("MCF send D2D ACK packet failed(%d).", result);
            mcf_d2d_pkt_free(ack_pkt);
            return -MCF_FAILD;
        }

        mcf_d2d_pkt_free(ack_pkt);
    }

    return MCF_OK;
}
#endif /* MCF_USING_ACK */

/**
 * receive MCF packet object within timeout.
 *
 * @param sid MCF session ID
 * @param pkt receive MCF packet object
 * @param timout receive timeout
 *
 * @return 0: close session success
 *        -1: get session by session ID error
 *        -2: no memory for packet allocate
 *        -3: send ACK packet timeout
 *        -5: receive packet timeout
 */
int mcf_d2d_recv(int sid, struct mcf_pkt *pkt, int32_t timeout)
{
    struct mcf_session *session = RT_NULL;
    struct mcf_d2d_pkt *d2d_pkt = RT_NULL;
    rt_base_t level = 0;
    mcf_err_t result = 0;
    uint8_t pid_idx = 0;

    RT_ASSERT(pkt != RT_NULL);
    RT_ASSERT(g_start_ok == RT_TRUE);

    /* get MCF session by session ID */
    session = mcf_session_get(sid);
    if (session == RT_NULL)
    {
        LOG_E("get MCF session by session ID(%d) failed.", sid);
        return -MCF_FAILD;
    }

    /* wait for MCF D2D packet response */
    result = mcf_rsp_event_wait(session->dst_id,
                session->sid, MCF_D2D_PKT_TYPE_RSP, &d2d_pkt, timeout);
    if (result != MCF_OK)
    {
        LOG_E("MCF get D2D packet (id: %d) response failed.", session->sid);
        return result;
    }
    pid_idx = d2d_pkt->pkt_id % MCF_RESULT_INDEX_LEN;

    /* fill user packet */
    pkt->info.dst_id = d2d_pkt->dst_id;
    pkt->info.src_id = d2d_pkt->src_id;
    pkt->info.pkt_id = d2d_pkt->pkt_id;
    pkt->info.pkt_type = d2d_pkt->pkt_type;
    pkt->info.need_ack = d2d_pkt->need_ack;

    /* fill packet buffer */
    pkt->pbuf.len = d2d_pkt->pbuf->len;
    rt_memcpy(pkt->pbuf.buf, d2d_pkt->pbuf->buf, d2d_pkt->pbuf->len);

#ifdef MCF_USING_ACK
    if (d2d_pkt->need_ack)
    {
        if (mcf_d2d_ack_pkt_send(d2d_pkt) != MCF_OK)
        {
            LOG_E("MCF D2D receive data send ACK packet failed.");
            result = -MCF_FAILD;
        }
    }
#endif /* MCF_USING_ACK */

    if (d2d_pkt)
    {
        struct mcf_pbuf *pbuf = d2d_pkt->pbuf;
        level = rt_hw_interrupt_disable();
        /* empty request result index */
        mcf_req_result_empty(pid_idx);
        rt_hw_interrupt_enable(level);
        mcf_pbuf_free(pbuf);
        mcf_d2d_pkt_free(d2d_pkt);
    }

    return result;
}

/**
 * close and free MCF D2D session.
 *
 * @param sid MCF session ID
 *
 * @return 0: close session success
 *        -1: close session failed, get session by session ID error
 */
int mcf_d2d_close(int sid)
{
    rt_base_t level;
    struct mcf_session *session = RT_NULL;

    RT_ASSERT(g_start_ok == RT_TRUE);

    /* get MCF session object by session ID */
    session = mcf_session_get(sid);
    if (session == RT_NULL)
    {
        LOG_E("get MCF session by session ID(%d) failed.", sid);
        return -MCF_FAILD;
    }

    /* delete session in session list */
    level = rt_hw_interrupt_disable();
    rt_slist_remove(&g_mcf_session_list, &session->list);
    rt_hw_interrupt_enable(level);

    rt_free(session);

    return MCF_OK;
}

/**
 * register request automatic execution of callback function.
 *
 * @param mcf_req_exec_fn request execution of callback function
 *
 * @return 0: register success
 *        -1: not enable request execute feature
 */
int mcf_req_exec_fn_register(mcf_req_exec_fn req_fn)
{
    RT_ASSERT(req_fn != RT_NULL);

#ifdef MCF_USING_REQ_EXEC
    if (g_mcf_obj)
    {
        g_mcf_obj->req_fn = req_fn;
    }
    return MCF_OK;
#else
    LOG_E("not enable MCF request execute feature.");
    return -MCF_FAILD;
#endif /* MCF_USING_REQ_EXEC */
}

/**
 * get current mcf transfer session info.
 *
 * @note This function only get mcf session info in the current thread.
 *       
 * @return struct mcf_pkt_info*
 */
struct mcf_pkt_info* _mcf_trans_get_cur_session_info(void)
{ 
    return (struct mcf_pkt_info *)(rt_thread_self()->user_data);
}
