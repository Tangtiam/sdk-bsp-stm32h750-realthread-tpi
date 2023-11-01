/*
 * Copyright (c) 2006-2020, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2020-11-26     BalanceTWK   first version
 */

#include <udbd.h>
#include <vconsole.h>
#ifdef PKG_USING_CJSON
    #include <cJSON.h>
#endif
#include <mcf.h>

#define DBG_TAG               "udb.shell"
#define DBG_LVL                DBG_INFO//DBG_LOG

#include <rtdbg.h>

static uint8_t urpc_dst_id = 0;

/* TODO MCU->PC*/
rt_size_t urpc_shell_output_t(rt_device_t device, rt_uint8_t *buff, rt_size_t size)
{
    int result = 0;
    struct urpc_blob in_blob = {0};
    char *send_data = (char *)buff;
    
    result = urpc_svc_exec(urpc_dst_id,
                           "cout",
                           RT_NULL,
                           urpc_blob_make(&in_blob, send_data, size, size),
                           RT_NULL,
                           rt_tick_from_millisecond(1000));
    if (result != RT_EOK)
    {
        //TODO Cannot print log
//        LOG_E("uRPC service execute failed(%d).", result);
//        return size;
    }

    return size;
}

static rt_device_t urpc_shell_device = NULL;
static rt_device_t old_dev = NULL;
/*
 *
 * */
struct urpc_blob *urpc_shell_start(struct urpc_blob *input)
{
    struct mcf_pkt_info *info = RT_NULL;
    
    if (urpc_shell_device == NULL)
    {
        extern struct mcf_pkt_info* _mcf_trans_get_cur_session_info(void);
        
        info = _mcf_trans_get_cur_session_info();
        if (info == NULL)
        {
            LOG_E("can't get pkt info.");
            return RT_NULL;
        }
        
        urpc_dst_id = info->dst_id;
        
        urpc_shell_device = vconsole_create("urpc_shell", urpc_shell_output_t);
        if (urpc_shell_device)
        {
            //TODO Disable logs of all MCF/urpc modules
            old_dev = vconsole_switch(urpc_shell_device);
            return RT_NULL;
        }
        LOG_E("urpc shell start failed");
    }

    return RT_NULL;
}
URPC_SVC_STATIC_REGISTER_ALIAS(urpc_shell_start, urpc_shell_start)

/*
 *
 * */
struct urpc_blob *urpc_shell_end(struct urpc_blob *input)
{
    if (urpc_shell_device)
    {
        vconsole_switch(old_dev);
        if (vconsole_delete(urpc_shell_device) == RT_EOK)
        {
            //TODO enable logs of all MCF/urpc modules
            urpc_shell_device = NULL;
            return RT_NULL;
        }

        LOG_E("urpc shell end failed");
    }
    return RT_NULL;
}
URPC_SVC_STATIC_REGISTER_ALIAS(urpc_shell_end, urpc_shell_end)

/*
 * PC->MCU
 * */
static struct urpc_blob *cin(struct urpc_blob *input)
{
    rt_uint8_t *buf = input->buf;

    if (urpc_shell_device)
    {
        if (vconsole_input(urpc_shell_device, buf, input->len) == input->len)
        {
            return RT_NULL;
        }
    }

//    LOG_E("urpc_shell_input failed");
    return RT_NULL;
}
URPC_SVC_STATIC_REGISTER_ALIAS(cin, cin)

extern int msh_exec(char *cmd, rt_size_t length);
URPC_FFI_FUNC_STATIC_REGISTER_ALIAS(msh_exec, msh_exec)

/********************** end of file ********************/

