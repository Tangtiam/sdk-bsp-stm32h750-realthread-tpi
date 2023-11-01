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
#include <urpc.h>

#define DBG_TAG               "urpc"
#ifdef URPC_USING_DEBUG
#define DBG_LVL               DBG_LOG
#else
#define DBG_LVL               DBG_INFO
#endif /* URPC_DEBUG */
#include <rtdbg.h>

#ifndef URPC_REQ_SVC_THREAD_POOL_SIZE
#define URPC_REQ_SVC_THREAD_POOL_SIZE            3
#endif
#ifndef URPC_REQ_SVC_THREAD_POOL_STACK_SIZE
#define URPC_REQ_SVC_THREAD_POOL_STACK_SIZE      1024
#endif

#define URPC_PARSER_THREAD_STACK_SIZE            2048
#define URPC_PARSER_THREAD_PRIORITY              10

#define URPC_SVC_NAME_END_SIGN                   '\0'
#define URPC_SVC_HEADER_LEN                      2

#define URPC_RESULT_BLOB_HEAD                    0xEC
#define URPC_RESULT_BLOB_LEN                     2

static rt_bool_t g_init_ok = RT_FALSE, g_start_ok = RT_FALSE;
/* the global uRPC service table begin and end address */
static struct urpc_svc *g_svc_table_begin = RT_NULL;
static struct urpc_svc *g_svc_table_end = RT_NULL;
/* the global uRPC dynamic service list */
static rt_slist_t g_svc_list = RT_SLIST_OBJECT_INIT(&g_svc_list);
static ve_module_t module_svc;
static ve_module_t module_ffi;
static struct rt_mutex urpc_svc_mutex;

static rt_base_t urpc_svc_lock(void)
{
    rt_mutex_take(&urpc_svc_mutex, RT_WAITING_FOREVER);
    return (rt_base_t)&urpc_svc_mutex;
}

static void urpc_svc_unlock(rt_base_t ctx)
{
    rt_mutex_release((struct rt_mutex *)ctx);
}

/* find uRPC service object by service name */
struct urpc_svc *_urpc_svc_find(const char *name)
{
    struct urpc_svc *service = RT_NULL;
    rt_slist_t *node = RT_NULL;
    rt_base_t level, address = 0x00;
    struct urpc_svc *svc = RT_NULL;

    level = urpc_svc_lock();
    rt_slist_for_each(node, &g_svc_list)
    {
        service = rt_slist_entry(node, struct urpc_svc, list);
        if (rt_strcmp(service->name, name) == 0)
        {
            urpc_svc_unlock(level);
            return service;
        }
    }
    urpc_svc_unlock(level);

    for (service = g_svc_table_begin; service < g_svc_table_end; service++)
    {
        if (rt_strcmp(service->name, name) == 0)
        {
            return service;
        }
    }

    if (rt_strncmp(name, "ffi." , sizeof("ffi.") - 1) == 0)
    {
        address = ve_value_get(&module_ffi, &name[sizeof("ffi.") - 1]);
        if (address != VE_NOT_FOUND)
        {
            svc = (struct urpc_svc *)address;
            return svc;
        } 
        else
        {
            return RT_NULL;
        }
    }
    else
    {
        address = ve_value_get(&module_svc, name);
        if (address != VE_NOT_FOUND)
        {
            svc = (struct urpc_svc *)address;
            return svc;
        }
        else
        {
            return RT_NULL;
        }
    }
}


/* service request execute callback function */
static int req_exec_task(struct mcf_pkt *in_pkt, struct mcf_pkt *out_pkt)
{
    int result = RT_EOK;
    char *svc_name = RT_NULL;
    struct urpc_svc *service = RT_NULL;
    struct urpc_blob in_blob = {0};
    struct urpc_blob *out_blob = RT_NULL;
    rt_size_t blob_head_size = 0, blob_buff_size = 0;

    /* get service   name by input packet buffer */
    svc_name = rt_strdup(in_pkt->pbuf.buf);
    if (svc_name == RT_NULL)
    {
        LOG_E("no memory for uRPC service name allocate.");
        return -RT_ENOMEM;
    }

    /* find service object by service name */
    service = _urpc_svc_find(svc_name);
    if (service == RT_NULL)
    {
        LOG_E("not find uRPC service name(%s).", svc_name);
        result = -RT_ERROR;
        goto _exit;
    }

    blob_head_size = rt_strlen(svc_name) +  URPC_SVC_HEADER_LEN;
    blob_buff_size = in_pkt->pbuf.len - blob_head_size;
    LOG_D("exec service %s", svc_name);
    /* execute register request function callback, get response packet data */
    out_blob = service->fn(urpc_blob_make(&in_blob, (char *)in_pkt->pbuf.buf + blob_head_size, blob_buff_size, blob_buff_size));

    /* filling out packet data and uRPC packet header */
    if (out_blob)
    {
        rt_memcpy((char *)out_pkt->pbuf.buf, out_blob->buf, out_blob->len);
        out_pkt->pbuf.len += out_blob->len;
    }

    if (out_blob)
    {
        urpc_blob_free(out_blob);
    }

_exit:
    rt_free(svc_name);

    return result;
}

static struct urpc_blob *devinfo_svc(struct urpc_blob *input)
{
    //TODO 增加更多设备信息
    return urpc_create_result_blob(RT_EOK);
}

/**
 * uRPC object initialize.
 *
 * @param id current device ID
 *
 * @return   0: uRPC object initialize success
 *         !=0: uRPC object initialize failed
 */
int urpc_init(rt_uint8_t id)
{
    if (g_init_ok == RT_TRUE)
    {
        LOG_W("uRPC object is already initialized.");
        return -RT_ERROR;
    }
    
    /* init urpc svc locker */
    rt_mutex_init(&urpc_svc_mutex, "urpc_svc_lock", RT_IPC_FLAG_PRIO);

    /* initialize uRPC static service table address */
    ve_module_init(&module_svc, "urpc_svc");
    ve_module_init(&module_ffi, "urpc_ffi");

    /* initialize MCF object by input device ID */
    if (mcf_init(id) != MCF_OK)
    {
        return -RT_ERROR;
    }

    /* register MCF request execute callback function */
    mcf_req_exec_fn_register((mcf_req_exec_fn) req_exec_task);

    g_init_ok = RT_TRUE;

    extern int _urpc_ffi_init(void);
    _urpc_ffi_init();

    urpc_svc_register("devinfo", devinfo_svc);

    LOG_I("uRPC(V%s) initialize successfully.", URPC_SW_VER);

    return RT_EOK;
}

/**
 * uRPC object start.
 */
void urpc_start(void)
{
    RT_ASSERT(g_init_ok);

    if (mcf_start() == RT_EOK)
    {
        g_start_ok = RT_TRUE;
    }
}

/**
 * uRPC service dynamic register.
 *
 * @param name service name
 * @param svc_fn service execute function
 *
 * @return  0: uRPC service dynamic register success
 *         <0: uRPC service dynamic register failed
 */
int urpc_svc_register(const char *name, urpc_svc_fn svc_fn)
{
    struct urpc_svc *service = RT_NULL;
    rt_base_t level;

    RT_ASSERT(name);
    RT_ASSERT(svc_fn);

    service = (struct urpc_svc *)rt_calloc(1, sizeof(struct urpc_svc));
    if (service == RT_NULL)
    {
        LOG_E("no memory for uRPC service object allocate.");
        return -RT_ERROR;
    }

    /* set uRPC service object information */
    service->ver  = URPC_SVC_VER_NUM;
    service->name = rt_strdup(name);
    service->fn   = svc_fn;

    /* add current service list global service list */
    rt_slist_init(&service->list);

    level = rt_hw_interrupt_disable();
    rt_slist_append(&g_svc_list, &service->list);
    rt_hw_interrupt_enable(level);

    return RT_EOK;
}

/**
 * execute uRPC service and get service response within timeout.
 *
 * @param dst_id destination device ID
 * @param name execute service name
 * @param flag execute service flag
 * @param input input blob data address
 * @param output output blob data address
 * @param timeout wait response timeout
 *
 * @return   0: uRPC service execute success
 *         !=0: uRPC service execute failed
 */
int urpc_svc_exec(rt_uint8_t dst_id, const char *name, rt_uint16_t flag,
        struct urpc_blob *input, struct urpc_blob *output, rt_int32_t timeout)
{
    int sid = 0, result = 0;
    rt_tick_t start_tick = 0, wait_tick = 0;
    mcf_err_t mcf_err = MCF_OK;
    struct mcf_pkt *pkt = RT_NULL;
    rt_uint8_t end_sign = URPC_SVC_NAME_END_SIGN, version = URPC_SVC_VER_NUM;
    rt_int32_t diff_tick = 0;

    RT_ASSERT(name);
    RT_ASSERT(g_start_ok);
    RT_ASSERT(flag <= (URPC_SVC_FLAG_NEED_ACK | URPC_SVC_FLAG_NEED_RSP));

    sid = mcf_d2d_open(dst_id);
    if (sid < 0)
    {
        return -RT_ERROR;
    }

    /* allocate packet for data send and receive */
    pkt = mcf_pkt_alloc(sid, MCF_PKT_MAX_SIZE);
    if (pkt == RT_NULL)
    {
        result = -RT_ENOMEM;
        goto _exit;
    }

    /*  server name length + service name end sign(1 byte) + service version(1 byte) + input blob buffer size */
    pkt->pbuf.len = rt_strlen(name) + sizeof(end_sign) + sizeof(version);
    rt_snprintf(pkt->pbuf.buf, pkt->pbuf.size, "%s%c%c", name, end_sign, version);
    if (input)
    {
        RT_ASSERT(input->len + pkt->pbuf.len <= MCF_PKT_MAX_SIZE);
        rt_memcpy((char *)pkt->pbuf.buf + pkt->pbuf.len, input->buf, input->len);
        pkt->pbuf.len += input->len;
    }

    start_tick = rt_tick_get();
    /* set packet information and send service request */
    mcf_err = mcf_d2d_send(sid, mcf_pkt_info_set(pkt, MCF_PKT_TYPE_REQ, flag), timeout);
    if(mcf_err != MCF_OK)
    {
        LOG_E("uRPC service send data failed(%d).", mcf_err);
        result = -RT_ERROR;
        goto _exit;
    }

    /* service request need response */
    if (flag & URPC_SVC_FLAG_NEED_RSP)
    {
        /* calculate the remaining waiting time  */
        if(timeout < 0)
        {
            diff_tick = timeout;
        }
        else
        {
            diff_tick = timeout - (rt_tick_get() - start_tick);
            if(diff_tick < 0)
            {
                diff_tick = 0;
            }
        }
        wait_tick = diff_tick;

        /* clean packet */
        rt_memset(pkt->pbuf.buf, 0, pkt->pbuf.len);
        pkt->pbuf.len = 0;

        mcf_err = mcf_d2d_recv(sid, pkt, wait_tick);
        if (mcf_err != MCF_OK)
        {
            LOG_E("uRPC service receive response failed(%d).", mcf_err);
            result = -RT_ERROR;
            goto _exit;
        }

        /* create and get output uRPC blob */
        {
            rt_size_t recv_data_len, out_data_len;

            /* calculate out blob object buffer length */
            recv_data_len = pkt->pbuf.len;
            out_data_len = (recv_data_len > output->size) ? output->size : recv_data_len;
            if (out_data_len)
            {
                /* set output blob object data */
                rt_memcpy(output->buf, (char *)pkt->pbuf.buf, out_data_len);
                output->len = out_data_len;
            }
        }
    }

_exit:
    if (sid >= 0)
    {
        mcf_d2d_close(sid);
    }

    mcf_pkt_free(pkt);

    return result;
}

/**
 * static create blob data object.
 *
 * @param blob blob object address
 * @param buf input blob buffer address
 * @param len input blob buffer length
 * @param size input blob buffer maximum size
 *
 * @return blob data address
 */
struct urpc_blob *urpc_blob_make(struct urpc_blob *blob, void *buf, rt_size_t len, rt_size_t size)
{
    RT_ASSERT(blob);

    blob->buf = buf;
    blob->len = len;
    blob->size = size;

    return blob;
}

/**
 * allocate blob object.
 *
 * @param size blob buffer size
 *
 * @return blob object address
 */
struct urpc_blob *urpc_blob_alloc(rt_size_t size)
{
    struct urpc_blob *blob = RT_NULL;

    blob = rt_calloc(1, sizeof(struct urpc_blob));
    if (blob == RT_NULL)
    {
        LOG_E("no memory for blob object allocate.");
        return RT_NULL;
    }

    blob->buf = rt_calloc(1, size);
    if (blob->buf == RT_NULL)
    {
        rt_free(blob);
        LOG_E("no memory for blob buffer allocate.");
        return RT_NULL;
    }

    /* initialize blob object information */
    blob->size = size;
    blob->len = 0;

    return blob;
}

/**
 * free blob object.
 *
 * @param blob blob object address
 */
void urpc_blob_free(struct urpc_blob *blob)
{
    if (blob && blob->buf)
    {
        rt_free(blob->buf);
    }

    if (blob)
    {
        rt_free(blob);
    }
}

/**
 * create result blob object by input result code
 *
 * @param result_code result code
 *
 * @return result code blob object
 */
struct urpc_blob *urpc_create_result_blob(rt_uint8_t result_code)
{
    struct urpc_blob *blob = RT_NULL;

    blob = urpc_blob_alloc(URPC_RESULT_BLOB_LEN);
    if (blob == RT_NULL)
    {
        LOG_E("no memory for uRPC blob object allocate.");
        return RT_NULL;
    }

    /* set uRPC error code blob */
    *((rt_uint8_t *)blob->buf) = URPC_RESULT_BLOB_HEAD;
    *((rt_uint8_t *)blob->buf + 1) = result_code;
    blob->len = URPC_RESULT_BLOB_LEN;

    return blob;
}

/**
 * get result blob object result code
 *
 * @param blob result blob object
 *
 * @return result code
 */
rt_uint8_t urpc_result_code_get(struct urpc_blob* blob)
{
    RT_ASSERT(blob != RT_NULL);

    if ((blob->len != URPC_RESULT_BLOB_LEN) || (*((rt_uint8_t *)blob->buf) != URPC_RESULT_BLOB_HEAD))
    {
        LOG_E("input blob object is not result blob object.");
        return RT_ERROR;
    }

    return *((rt_uint8_t *)blob->buf + 1);
}

#ifdef URPC_USING_CJSON
#include <cJSON.h>

/**
 * convert the cJSON object to blob object.
 *
 * @param json cJSON object address
 * @param blob blob object address
 *
 * @return blob object address
 */
struct urpc_blob *urpc_json_to_blob(cJSON *json, struct urpc_blob *blob)
{
    char *json_str = RT_NULL;
    rt_size_t json_str_len = 0;

    RT_ASSERT(json);
    RT_ASSERT(blob && blob->buf);

    /* render a JSON entity to string data */
    json_str = cJSON_PrintUnformatted(json);
    json_str_len = rt_strlen(json_str);
    /* check JSON string data size */
    if (json_str_len > blob->size)
    {
        LOG_W("input JSON data length exceeds blob data maximum length(%d).", blob->size);
    }

    blob->len = json_str_len > blob->size ? blob->size : json_str_len;
    rt_memcpy(blob->buf, json_str, blob->len);

    rt_free(json_str);

    return blob;
}

/**
 * convert the blob object to cJSON object.
 *
 * @param blob blob object address
 * @param json cJSON object address
 *
 * @return cJSON object address
 */
cJSON *urpc_blob_to_json(struct urpc_blob *blob, cJSON *json)
{
    RT_ASSERT(blob && blob->buf);

   return cJSON_Parse((const char *)blob->buf);
}
#endif /* URPC_USING_CJSON */

