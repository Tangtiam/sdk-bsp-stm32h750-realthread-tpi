/*
 * Copyright (c) 2006-2022, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2021-01-18     Thread_Liu   first version
 */
 
#include <udbd.h>
#include <rthw.h>
#include <ulog.h>
#include <mcf.h>

#define DBG_TAG                       "udbd.log"
#define DBG_LVL                        DBG_INFO//DBG_LOG
#include <rtdbg.h>

static rt_uint8_t urpc_dst_id = 0;

static struct ulog_backend udbd_log = {0};
static rt_bool_t is_writing = RT_FALSE;

void udbd_log_console_backend_output(struct ulog_backend *backend, rt_uint32_t level, const char *tag, rt_bool_t is_raw,
        const char *log, size_t len)
{
    struct urpc_blob in_blob = {0};
    
    if (!is_writing)
    {
        is_writing = RT_TRUE;
        urpc_svc_exec(urpc_dst_id, "putlog", RT_NULL, urpc_blob_make(&in_blob, (char *) log, len, len), RT_NULL,
                rt_tick_from_millisecond(1000));
        is_writing = RT_FALSE;
    }
}

static int udbd_log_register(void)
{
    int result = 0;
    struct mcf_pkt_info *info = RT_NULL;
    
    extern struct mcf_pkt_info* _mcf_trans_get_cur_session_info(void);
    
    info = _mcf_trans_get_cur_session_info();
    if (info == NULL)
    {
        LOG_E("can't get pkt info.");
        return RT_NULL;
    }
    
    urpc_dst_id = info->dst_id;

    udbd_log.output = udbd_log_console_backend_output;
    result = ulog_backend_register(&udbd_log, "udbd_log", RT_TRUE);
    
    return result;
}
URPC_FFI_FUNC_STATIC_REGISTER_ALIAS(udbd_log_register, udbd_log_register)

static int udbd_log_unregister(void)
{
    int result = 0;

    result = ulog_backend_unregister(&udbd_log);

    return result;
}
URPC_FFI_FUNC_STATIC_REGISTER_ALIAS(udbd_log_unregister, udbd_log_unregister)

/************************ end of file *******************/
