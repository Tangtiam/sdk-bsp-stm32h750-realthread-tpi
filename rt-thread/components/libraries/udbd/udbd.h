/*
 * Copyright (c) 2006-2020, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2021-02-22     liukang      first version
 * 2021-09-08	  WJL		   second version
 */

#ifndef __UDBD_H
#define __UDBD_H

#include <stdint.h>
#include <urpc.h>
#include <var_export.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef UDBD_SERVER_DID
#define UDBD_SERVER_DID       0
#endif

typedef int (*udbd_module_fn)(void);

/* udbd module object */
typedef struct udbd_module
{
    udbd_module_fn init;
    udbd_module_fn deinit;
} udbd_module_t;

#define UDBD_MODULE_EXPORT(name, init, deinit, level)           \
    const udbd_module_t udbd_module_obj_##name =                \
    {                                                           \
        (udbd_module_fn)init,                                   \
        (udbd_module_fn)deinit,                                 \
    };                                                          \
    VAR_EXPORT(udbd, name, (rt_base_t)&udbd_module_obj_##name)  \

enum udbd_link_device_type
{
    UDBD_LINK_UART = 0,
    UDBD_LINK_CHAR = UDBD_LINK_UART,
    UDBD_LINK_USB,
    UDBD_LINK_SOCKET,
};

enum udbd_status_cb_type
{
    UDBD_STATUS_CB_TYPE_SERVER_LINK_UP,          /* changed to 'link up' */
    UDBD_STATUS_CB_TYPE_SERVER_LINK_DOWN,        /* changed to 'link down' */
};

typedef enum udbd_status_type
{
    UDBD_STATUS_SERVER_LINK_UP,          /* link up */
    UDBD_STATUS_SERVER_LINK_DOWN,        /* link down */
}udbd_status_t;

struct udbd_callback;
typedef struct udbd_callback udbd_status_cb;

typedef void (*udbd_status_callback_fn)(udbd_status_cb *cb, enum udbd_status_cb_type type);

struct udbd_callback
{
    rt_list_t node;
    union udbd_callback_function
    {
        udbd_status_callback_fn status_cb;
    }type;
};

int udbd_init(int type, char *device_name);
void udbd_set_status_callback(udbd_status_cb *cb, udbd_status_callback_fn status_callback);
udbd_status_t udbd_get_status(void);
int udbd_get_server_ver_num(void);

/* low level API */
void udbd_low_level_post_status_changed_event(enum udbd_status_cb_type type);

#ifdef  __cplusplus
}
#endif

#endif  /* __UDBD_H__ */
