
/*
 * Copyright (c) 2006-2020, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2020-11-26     BalanceTWK   first version
 */
#include <rtdevice.h>
#include <udbd.h>
#include <rtdbg.h>
#include <gdbserver.h>

#define DBG_TAG               "udb.gdb"
#define DBG_LVL                DBG_INFO//DBG_LOG

static void gdb_start(char* name, int port)
{
    int argc;
    char *argv[3];

    rt_memset(argv, 0x0, sizeof(argv));
    argc == 3;
    argv[0] = name;
    argv[1] = "--port";
    argv[2] = port;
    dbg(argc, argv);
}
URPC_SVC_STATIC_REGISTER_ALIAS(gdb_start, gdbserver)
