/*
 * Copyright (c) 2006-2020, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2020-12-08     tyustli      first version
 */

#include <rtdevice.h>
#include <urpc.h>
#include <mcf.h>
#include <mcf_link.h>
#include <udbd.h>

#define DBG_TAG                       "udbd"
#define DBG_LVL                        DBG_INFO//DBG_LOG
#include <rtdbg.h>

#define UDBD_VER                       "2.4.0"
#define UDBD_SERVER_VER_NUM_DEFAULT    0x00000

static uint8_t urpc_dev_id = 1;
static int udbd_module_init(void);
static int udbd_server_ver_num = UDBD_SERVER_VER_NUM_DEFAULT;

/**
 * UDB daemon initialization
 *
 * @param link_type link type for MCF @see udbd_link_device_type
 * @param device_name link device name, using NULL when using socket type
 *
 * @return <0: failed, >=0: success
 */
int udbd_init(int link_type, char *device_name)
{
    int res = -1;
    static uint8_t port_id = 1;

    switch (link_type)
    {
#ifdef MCF_USING_LINK_UART
        case UDBD_LINK_UART:
            res = mcf_link_uart_register(port_id, device_name, RT_NULL);
        break;
#endif

#ifdef MCF_USING_LINK_USB
        case UDBD_LINK_USB:
            res = mcf_link_usb_register(port_id, device_name, RT_NULL);
        break;
#endif

#ifdef MCF_USING_LINK_SOCKET
        case UDBD_LINK_SOCKET:
            res = mcf_link_socket_register(port_id, device_name, RT_NULL);
        break;
#endif

        default:
            LOG_E("not support udbd link type: %d", link_type);
        break; 

    }

    if (res != 0)
    {
        LOG_E("mcf link register failed. device_name:%s res:%d", device_name, res);
        return res;
    }

    /* generate new port id */
    port_id ++;

    { /* urpc and urpc's service initialization */
        static rt_bool_t is_init = RT_FALSE;

        if (is_init)
        {
            /* initialize only once */
            return 0;
        }
        is_init = RT_TRUE;
        /* TODO: the device ID is ONLY support 1 */
        urpc_dev_id = 1;
        urpc_init(urpc_dev_id);
        urpc_start();
    }

    udbd_module_init();
    return 0;
}

static int udbd_module_init(void)
{
    ve_module_t module;
    ve_iterator_t iter;
    const ve_exporter_t *exporter;
    int result = RT_EOK;

    result = ve_module_init(&module, "udbd");
    if(result != RT_EOK)
    {
        LOG_E("udbd module init failed(%d).", result);
        return RT_ERROR;
    }
    ve_iter_init(&module, &iter); 
    while (RT_TRUE)
    {
        exporter = ve_iter_next(&iter);
        if (exporter == RT_NULL)
        {
            break;
        }
        else
        {
            struct udbd_module *module = (struct udbd_module *)exporter->value;
            if (module != RT_NULL)
            {
                result = module->init();
                if(result != RT_EOK)
                {
                    LOG_E("udbd module:%s init failed(%d).", exporter->identifier, result);
                    break;
                }
            }
        }
    }
    return result;
}

static rt_list_t status_cb_list_head = RT_LIST_OBJECT_INIT(status_cb_list_head);
static udbd_status_t udbd_status = UDBD_STATUS_SERVER_LINK_DOWN;

udbd_status_t udbd_get_status(void)
{
    return udbd_status;
}

/**
 * set status changed callback
 * @param status_callback  callback
 */
void udbd_set_status_callback(udbd_status_cb *cb, udbd_status_callback_fn status_callback)
{
    rt_list_init(&cb->node);
    cb->type.status_cb = status_callback;
    rt_list_insert_before(&status_cb_list_head, &cb->node);
}

static void status_callback_work(struct rt_work *work, void *work_data)
{
    rt_list_t *n;
    udbd_status_cb *cb;

    rt_list_for_each(n, &status_cb_list_head)
    {
        cb = rt_container_of(n, udbd_status_cb, node);
        cb->type.status_cb(cb, (enum udbd_status_cb_type)((rt_uint32_t)work_data));
    }
    rt_free(work);
}

void udbd_low_level_post_status_changed_event(enum udbd_status_cb_type type)
{
    struct rt_work *work = RT_NULL;

    LOG_I("post an UDB server LINK %s%s event",
        type == UDBD_STATUS_CB_TYPE_SERVER_LINK_UP ? "UP" : "",
        type == UDBD_STATUS_CB_TYPE_SERVER_LINK_DOWN ? "DOWN" : "");

    if (type == UDBD_STATUS_CB_TYPE_SERVER_LINK_UP)
    {
        udbd_status = UDBD_STATUS_SERVER_LINK_UP;
    }
    else
    {
        udbd_status = UDBD_STATUS_SERVER_LINK_DOWN;
        udbd_server_ver_num = UDBD_SERVER_VER_NUM_DEFAULT;
    }

    if (!rt_list_isempty(&status_cb_list_head))
    {
        work = rt_malloc(sizeof(struct rt_work));
        if (work)
        {
            rt_work_init(work, status_callback_work, (void *)type);
            rt_work_submit(work, 0);
        }
        else
        {
            LOG_E("udbd_low_level_post_status_changed_event malloc failed.");
        }
    }
}

/* call from UDB Server, will be deprecated */
static int server_link_up(void)
{
    udbd_low_level_post_status_changed_event(UDBD_STATUS_CB_TYPE_SERVER_LINK_UP);
    return 0;
}
URPC_FFI_FUNC_STATIC_REGISTER_ALIAS(server_link_up, _link_up)
MSH_CMD_EXPORT_ALIAS(server_link_up, link_up, "udb netdev linkup");

/* call from UDB Server */
static struct urpc_blob *server_link_up2(struct urpc_blob *input)
{
    cJSON *json_root = cJSON_ParseWithLength(input->buf, input->len);
    cJSON *json_ver = RT_NULL;
    struct urpc_blob* output_blob = NULL;

    if (json_root)
    {
        json_ver = cJSON_GetObjectItem(json_root, "version");
        if (json_ver)
        {
            udbd_server_ver_num = json_ver->valueint;
        }
        cJSON_Delete(json_root);

        {
            cJSON *output_json = cJSON_CreateObject();
            RT_ASSERT(output_json)
            char *output_str = cJSON_PrintUnformatted(output_json);
            RT_ASSERT(output_str)
            cJSON_Delete(output_json);

            output_blob = urpc_blob_alloc(rt_strlen(output_str));
            RT_ASSERT(output_blob)
            output_blob->len = rt_strlen(output_str);
            rt_memcpy(output_blob->buf, output_str, output_blob->len);
            cJSON_free(output_str);
        }
    }

    LOG_I("UDB server version number: %x", udbd_server_ver_num);

    return output_blob;
}
URPC_SVC_STATIC_REGISTER_ALIAS(server_link_up2, _link_up2)

int udbd_get_server_ver_num(void)
{
    return udbd_server_ver_num;
}

/**
 * get device info.
 *
 * @param key   (32 Byte)
 * @param value (32 Byte)
 *
 * @return 0: get info fail, other: success.
 */
static int get_device_info(uint8_t *key, uint8_t *value)
{
    /* KEY = udbd.mtu, get device mtu */
    if (rt_strcmp((const char *)key, "udbd.mtu") == 0)
    {
        return rt_sprintf((char *)value, "%d", MCF_PKT_MAX_SIZE);
    }
    /* KEY = udbd.ver, get udbd version */
    if (rt_strcmp((const char *)key, "udbd.ver") == 0)
    {
        return rt_sprintf((char *)value, "%s", UDBD_VER);
    }
    if (rt_strcmp((const char *)key, "udbd.zlib.decompress") == 0)
    {
#if defined UDBD_USING_FILE_COMPRESS
        return rt_sprintf((char *)value, "%s", "True");
#else
        return rt_sprintf((char *)value, "%s", "False");
#endif
    }

    /* not support key*/
    LOG_W("not support key: %s.", key);
    return 0;
}
URPC_FFI_FUNC_STATIC_REGISTER_ALIAS(get_device_info, _dev_info)

/**
 * udbd heartbeat response. 
 * When udb device freetime over 5 second, udb server will send heartbeat request.
 *
 * @return 0: success.
 */
static uint32_t ping_resp(uint8_t req)
{
    return 0;
}
URPC_FFI_FUNC_STATIC_REGISTER_ALIAS(ping_resp, _ping)

/**
 * udbd data black hole.
 * You can use this function to test the reading and writing of data.
 *
 * @return Index of received data.
 */
static uint32_t _null(uint32_t index, const char *data, size_t length)
{
    return index;
}
URPC_FFI_FUNC_STATIC_REGISTER_ALIAS(_null, _null)

static struct urpc_blob *_echo(struct urpc_blob *input)
{
    struct urpc_blob *blob = urpc_blob_alloc(input->len);

    if (blob)
    {
        rt_memcpy(blob->buf, input->buf, input->len);
        blob->len = input->len;
    }
    else
    {
        LOG_W("NO memory for echo");
    }

    return blob;
}
URPC_SVC_STATIC_REGISTER(_echo);

static uint32_t _lost(uint32_t index, uint32_t reset, const char *data, size_t length)
{
    static uint32_t lost_count = 0;
    static uint32_t last_index = 0;

    if(index == 0)
    {
        last_index = 0;
        LOG_I("packet index reset to 0.");
    }
    else
    {
        if(index > (last_index + 1))
        {
            lost_count = lost_count + (index - (last_index + 1));
        }
        if(index < (last_index + 1))
        {
            LOG_E("packet index error, index:%u < last + 1:%u.", index, (last_index + 1));
        }
    }
    if(reset > 0)
    {
        lost_count = 0;
        LOG_I("packet lost count reset to 0.");
    }
    //last = current
    last_index = index;
    LOG_I("packet index is %u, lost count is %u.", index, lost_count);
    return lost_count;
}
URPC_FFI_FUNC_STATIC_REGISTER_ALIAS(_lost, _lost)

/************************ end of file *******************/
