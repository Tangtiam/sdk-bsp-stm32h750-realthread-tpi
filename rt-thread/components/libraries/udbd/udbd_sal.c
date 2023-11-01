/*
 * Copyright (c) 2006-2018, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-02-07     armink     First version
 */

#include <rtthread.h>
#include <udbd.h>
#include <mcf_link.h>
#include <sys/time.h>
#include <sys/ioctl.h>

#ifdef RT_USING_SAL
#include <netdb.h>
#if RT_VER_NUM <= 0x40100
#include <sal.h>
#else
#include <sal_low_lvl.h>
#endif
#include <arpa/inet.h>
#include <netdev.h>

#ifndef PKG_USING_UBJSON
#error "Please enable UBJSON package"
#endif

#include <ubj.h>

#ifdef SAL_USING_POSIX
#if RT_VER_NUM <= 0x40003
#include <dfs_poll.h>
#else
#include <poll.h>
#endif
#endif

#define DBG_TAG    "udbd.sal"
#define DBG_LVL    DBG_INFO//DBG_LOG
#include <rtdbg.h>

#define AF_UDBD         47  /* UDBD socket */

static struct netdev *netdev = RT_NULL;
static uint8_t urpc_dst_id = 0;

struct udbd_sock
{
    int socket;
    int ref;
    rt_list_t node;
    int req_event;
    rt_wqueue_t wait;
    rt_bool_t is_nonblocking;
    rt_uint8_t cache_buf[960]; // 960=2*512-64, 512: BLE MTU, 64: MCF header
    rt_uint16_t cache_buf_len;
    rt_uint16_t cache_pos;
    rt_uint16_t cache_len;
    int rcvtimeo; // unit ms
    int sndtimeo; // unit ms
};

struct udbd_set
{
    int count;
    int array[16];   // 
};

static rt_list_t socket_list = RT_LIST_OBJECT_INIT(socket_list);
static struct rt_mutex lock;
static time_t startup_time;

#ifdef SAL_USING_POSIX
static int udbd_wakeup_pipe[2] = {-1, -1};
#endif
#define UDBD_LINK_UP_EVENT_FLAG     (0x1 << 0)
#define UDBD_SELECT_EVENT_FLAG      (0x1 << 1)
static struct rt_event link_up_flag;
static struct rt_thread thread;
static rt_uint8_t thread_stack[4096];

static int udbd_socket(int domain, int type, int protocol);
static int udbd_closesocket(int socket);
static int udbd_bind(int socket, const struct sockaddr *name, socklen_t namelen);
static int udbd_listen(int socket, int backlog);
static int udbd_recvfrom(int socket, void *data, size_t len, int flags, struct sockaddr *from, socklen_t *fromlen);
static int udbd_connect(int socket, const struct sockaddr *name, socklen_t namelen);
static int udbd_accept(int socket, struct sockaddr *addr, socklen_t *addrlen);
static int udbd_sendto(int socket, const void *data, size_t size, int flags, const struct sockaddr *to, socklen_t tolen);
static int udbd_recvfrom(int socket, void *data, size_t len, int flags, struct sockaddr *from, socklen_t *fromlen);
static int udbd_shutdown(int socket, int how);
static int udbd_getpeername(int socket, struct sockaddr *name, socklen_t *namelen);
static int udbd_getsockname(int socket, struct sockaddr *name, socklen_t *namelen);

void list_udbd_sock(int argc, char *argv[])
{
    rt_list_t *n;
    struct udbd_sock *sock = RT_NULL;

    rt_mutex_take(&lock, RT_WAITING_FOREVER);
    rt_list_for_each(n, &socket_list)
    {
        sock = rt_container_of(n, struct udbd_sock, node);
        rt_kprintf("sock:%d ref:%d\n", sock->socket, sock->ref);
    }
    rt_mutex_release(&lock);
}
MSH_CMD_EXPORT(list_udbd_sock, list_udbd_sock);

static struct udbd_sock *udbd_sock_find(int s)
{
    rt_list_t *n;
    struct udbd_sock *sock = RT_NULL;

    rt_mutex_take(&lock, RT_WAITING_FOREVER);
    rt_list_for_each(n, &socket_list)
    {
        sock = rt_container_of(n, struct udbd_sock, node);
        if (sock->socket == s)
        {
            rt_mutex_release(&lock);
            return sock;
        }
    }
    rt_mutex_release(&lock);
    return RT_NULL;
}

static struct udbd_sock *udbd_sock_get(int s)
{
    struct udbd_sock *sock = RT_NULL;

    rt_mutex_take(&lock, RT_WAITING_FOREVER);
    sock = udbd_sock_find(s);
    if (sock == RT_NULL)
    {
        sock = rt_malloc(sizeof(struct udbd_sock));
        if (sock != RT_NULL)
        {
            sock->ref = 1;
            sock->req_event = 0;
            sock->socket = s;
            sock->is_nonblocking = RT_FALSE;
            sock->cache_pos = 0;
            sock->cache_len = 0;
            sock->cache_buf_len = sizeof(sock->cache_buf);
            sock->rcvtimeo = 0;
            sock->sndtimeo = 0;
            rt_wqueue_init(&sock->wait);
            rt_list_init(&sock->node);
            rt_list_insert_after(&socket_list, &sock->node);
        }
    }
    else
    {
        sock->ref += 1;
    }
    rt_mutex_release(&lock);
    return sock;
}

static void udbd_sock_put(int s)
{
    struct udbd_sock *sock = RT_NULL;

    rt_mutex_take(&lock, RT_WAITING_FOREVER);
    sock = udbd_sock_find(s);
    if (sock != RT_NULL)
    {
        sock->ref --;
        RT_ASSERT(sock->ref >= 0);
        if (sock->ref <= 0)
        {
            rt_list_remove(&sock->node);
            rt_free(sock);
        }
    }
    rt_mutex_release(&lock);
}

#ifdef SAL_USING_POSIX
rt_inline void udbd_sock_set(int s, struct udbd_set *set)
{
    int i;

    for (i = 0; i < set->count; i++)
    {
        if (set->array[i] == s)
        {
            break;
        }
    }
    if (i == set->count)
    {
        if (set->count < sizeof(set->array) / sizeof(set->array[0]))
        {
            set->array[i] = s;
            set->count++;
        }
    }
}

rt_inline int udbd_sock_isset(int s, struct udbd_set *set)
{
    int i;

    for (i = 0; i < set->count; i++)
    {
        if (set->array[i] == s)
        {
            return 1;
        }
    }
    return 0;
}

rt_inline void __set(ubjsonw_t *ubj, struct udbd_set *set, const char *tag)
{
    ubj_object_write_array(ubj, tag);
    if (set && set->count > 0)
    {
        for (int i = 0; i < set->count; i++)
        {
            ubj_write_int32(ubj, set->array[i]);
        }
    }
    ubj_end_array(ubj);
}

rt_inline void __get(ubjh_t *ubjh, struct udbd_set *set, const char *tag)
{
    set->count = 0;
    if (ubjh && ubj_is_object(ubjh))
    {
        ubjh = ubj_object_item(ubjh, tag);
        if (ubjh && ubj_is_array(ubjh))
        {
            ubjh_t *child = ubj_child_item(ubjh);
            while (child)
            {
                if (ubj_is_integer(child))
                {
                    set->array[set->count] = (int)ubj_get_integer(child);
                    set->count ++;
                }
                child = ubj_next_item(child);
            }
        }
    }
}

static int __udbd_select(struct udbd_set *reads, struct udbd_set *writes, struct udbd_set *errors, int timeout)
{
    struct urpc_blob in_blob = {0}, out_blob = {0};
    int svc_result, result = -1;
    ubjsonw_t *ubj;
    rt_uint8_t recv[256];

    if (netdev_is_link_up(netdev) == RT_FALSE)
    {
        // link disconnect, keep error flag
        if (reads)
            reads->count = 0;
        if (writes)
            writes->count = 0;
        if (errors && errors->count > 0)
            return 1;
        else
            return -1;
    }

    if (reads && reads->count)
    {
        struct udbd_set set = { 0 };
        // If there is data in the read buffer, set it
        for (int i = 0; i < reads->count; i ++)
        {
            struct udbd_sock *sock = udbd_sock_get(reads->array[i]);
            if (sock && (sock->cache_len - sock->cache_pos) > 0)
            {
                set.array[set.count] = reads->array[i];
                set.count += 1;
            }
            udbd_sock_put(reads->array[i]);
        }
        if (set.count > 0)
        {
            *reads = set;
            if (writes)
                writes->count = 0;
            if (errors)
                errors->count = 0;
            return 1;
        }
    }

    ubj = ubj_write_memory();
    ubj_begin_object(ubj);
    // timeout
    ubj_object_write_integer(ubj, "timeout", timeout);
    __set(ubj, reads, "reads");
    __set(ubj, writes, "writes");
    __set(ubj, errors, "errors");
    ubj_end_object(ubj);

    // select
    svc_result = urpc_svc_exec(urpc_dst_id, "udbd_select", URPC_SVC_FLAG_NEED_RSP,
                urpc_blob_make(&in_blob, ubj_get_memory(ubj), ubj->total, ubj->total),
                urpc_blob_make(&out_blob, &recv, 0, sizeof(recv)),
                rt_tick_from_millisecond((timeout + 3) * 1000));

    ubj_write_end(ubj);

    if (svc_result == 0)
    {
        ubjh_t *ubjh = ubj_parse(recv, out_blob.len);
        __get(ubjh, reads, "reads");
        __get(ubjh, writes, "writes");
        __get(ubjh, errors, "errors");
        ubj_parse_free(ubjh);
        if ((reads && reads->count) || (writes && writes->count)|| (errors && errors->count))
        {
            result = 1;
        }
        else
        {
            result = 0;
        }
        LOG_D("udbd_select success, result: %d", result);
    }
    else
    {
        LOG_E("udbd_select failed: %d", svc_result);
    }
    return result;
}

static void udbd_do_poll(void)
{
    rt_list_t *n;
    struct udbd_sock *sock;
    struct udbd_set read_set, write_set, error_set;

    while (1)
    {
        // init
        rt_memset(&read_set, 0, sizeof(read_set));
        rt_memset(&write_set, 0, sizeof(write_set));
        rt_memset(&error_set, 0, sizeof(error_set));
        rt_mutex_take(&lock, RT_WAITING_FOREVER);
        rt_list_for_each(n, &socket_list)
        {
            sock = rt_container_of(n, struct udbd_sock, node);
            if (sock->req_event & POLLIN)
            {
                udbd_sock_set(sock->socket, &read_set);
            }
            if (sock->req_event & POLLOUT)
            {
                udbd_sock_set(sock->socket, &write_set);
            }
            if (sock->req_event & POLLERR)
            {
                udbd_sock_set(sock->socket, &error_set);
            }
        }
        rt_mutex_release(&lock);
        if (read_set.count == 0 && write_set.count == 0 && error_set.count == 0)
        {
            rt_event_recv(&link_up_flag, UDBD_SELECT_EVENT_FLAG, RT_EVENT_FLAG_AND | RT_EVENT_FLAG_CLEAR, RT_WAITING_FOREVER, RT_NULL);
            continue;
        }
        udbd_sock_set(udbd_wakeup_pipe[0], &read_set);
        udbd_sock_set(udbd_wakeup_pipe[0], &error_set);
        // wait
        if (__udbd_select(&read_set, &write_set, &error_set, 5) <= 0)
        {
            if (udbd_sock_isset(udbd_wakeup_pipe[0], &error_set))
            {
                break;
            }
            else
            {
                continue;
            }
        }
        if (udbd_sock_isset(udbd_wakeup_pipe[0], &read_set))
        {
            char buf[64];
            udbd_recvfrom(udbd_wakeup_pipe[0], buf, sizeof(buf), 0, RT_NULL, RT_NULL);
        }
        // check
        rt_mutex_take(&lock, RT_WAITING_FOREVER);
        rt_list_for_each(n, &socket_list)
        {
            int e = 0;
            sock = rt_container_of(n, struct udbd_sock, node);
            if (sock->req_event & POLLIN && udbd_sock_isset(sock->socket, &read_set))
            {
                e |= POLLIN;
            }
            if (sock->req_event & POLLOUT && udbd_sock_isset(sock->socket, &write_set))
            {
                e |= POLLOUT;
            }
            if (sock->req_event & POLLERR && udbd_sock_isset(sock->socket, &error_set))
            {
                e |= POLLERR;
            }
            if (e != 0)
            {
                rt_wqueue_wakeup(&sock->wait, (void*)e);
                sock->req_event = 0;
            }
        }
        rt_mutex_release(&lock);
    }
}

#define __ERROR_CHECK(__ret) \
    do \
    {   \
        if (!(__ret))   \
        {   \
            if (wakeup_pipe[0] >= 0)   \
            {   \
                udbd_closesocket(wakeup_pipe[0]);  \
                wakeup_pipe[0] = -1;   \
            }   \
            if (wakeup_pipe[1] >= 0)   \
            {   \
                udbd_closesocket(wakeup_pipe[1]);  \
                wakeup_pipe[0] = -1;   \
            }   \
            res = -1; \
            goto end;   \
        }   \
    } while (0)

static int udbd_wakeup_pipe_init(void)
{
    struct sockaddr_in name = { 0 };
    int namelen = sizeof(name);
    int res, s = -1;
    int wakeup_pipe[2] = {-1, -1};

    name.sin_family = AF_INET;
    name.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    // init tmp socket
    __ERROR_CHECK((s = udbd_socket(AF_INET, SOCK_STREAM, 0)) >= 0);
    __ERROR_CHECK(res = udbd_bind(s, (struct sockaddr*)&name, sizeof(name)) == 0);
    __ERROR_CHECK((res = udbd_listen(s, 2)) == 0);
    __ERROR_CHECK((res = udbd_getsockname(s, (struct sockaddr*)&name, (socklen_t *)&namelen)) == 0);
    // init read sock
    __ERROR_CHECK((wakeup_pipe[0] = udbd_socket(AF_INET, SOCK_STREAM, 0)) >= 0);
    __ERROR_CHECK((res = udbd_connect(wakeup_pipe[0], (struct sockaddr*)&name, namelen)) == 0);
    // init wirte sock
    __ERROR_CHECK((wakeup_pipe[1] = udbd_accept(s, (struct sockaddr*)&name, (socklen_t *)&namelen)) >= 0);
    udbd_wakeup_pipe[0] = wakeup_pipe[0];
    udbd_wakeup_pipe[1] = wakeup_pipe[1];
    res = 0;
end:
    if (s)
        udbd_closesocket(s);
    return res;
}

static void udbd_wakeup_pipe_deinit(void)
{
    if (udbd_wakeup_pipe[0] >= 0)
    {
        udbd_closesocket(udbd_wakeup_pipe[0]);
        udbd_wakeup_pipe[0] = -1;
    }
    if (udbd_wakeup_pipe[1] >= 0)
    {
        udbd_closesocket(udbd_wakeup_pipe[1]);
        udbd_wakeup_pipe[1] = -1;
    }
}

static void udbd_poll_check_thread(void *param)
{
    rt_tick_t tick;

    while (1)
    {
        tick = rt_tick_get();
        if (rt_event_recv(&link_up_flag, UDBD_LINK_UP_EVENT_FLAG, RT_EVENT_FLAG_AND, RT_WAITING_FOREVER, RT_NULL) == RT_EOK)
        {
            if (udbd_wakeup_pipe_init() == 0)
            {
                udbd_do_poll();
                udbd_wakeup_pipe_deinit();
            }
        }
        tick = rt_tick_get() - tick;
        // Minimum retry cycle 3 seconds
        if (tick < 3 * RT_TICK_PER_SECOND)
        {
            if (tick < RT_TICK_PER_SECOND)
                tick = RT_TICK_PER_SECOND;
            rt_thread_mdelay(tick);
        }
    }
}
#endif

/**
 * TODO Watch restart test
 * TODO Mobile phone restart test
 * TODO Disconnect test
 *   - [ ] Connection successfully
 *     - [ ] Watch and mobile phones are executed separately sal_init
 *   - [ ] Disconnect callback
 *     - [ ] Watch and mobile phones are executed separately sal_deinit
 *   - [X] MCF Link layer device disconnection callback
 *      - [X] When disconnected, the transport layer automatically wakes up the blocking receiving waiting thread and returns an error
 *   - [X] UDB server disconnect callback
 *   - [X] Watch UDB server disconnect callback
 *   - [ ] Pthon UDB daemon disconnection callback
 *      - [ ] When disconnected, the transport layer automatically wakes up the blocking receiving waiting thread and returns an error
 */


static int udbd_socket(int domain, int type, int protocol)
{
    struct urpc_blob in_blob = { 0 }, out_blob = { 0 };
    int sock = -1, svc_result;
    ubjsonw_t* ubj;

    if (netdev_is_link_up(netdev) == RT_FALSE)
        return -1;

    ubj = ubj_write_memory();
    ubj_begin_object(ubj);
    ubj_object_write_integer(ubj, "domain", domain);
    ubj_object_write_integer(ubj, "type", type);
    ubj_object_write_integer(ubj, "protocol", protocol);
    ubj_object_write_integer(ubj, "startup_time", startup_time);
    ubj_end_object(ubj);

    svc_result = urpc_svc_exec(urpc_dst_id, "udbd_socket", URPC_SVC_FLAG_NEED_RSP,
                               urpc_blob_make(&in_blob, ubj_get_memory(ubj), ubj->total, ubj->total),
                               urpc_blob_make(&out_blob, &sock, sizeof(sock), sizeof(sock)), rt_tick_from_millisecond(3000));

    ubj_write_end(ubj);

    if (svc_result == 0)
    {
        LOG_D("udbd_socket success, fd: %d", sock);
        if (udbd_sock_find(sock) != RT_NULL)
        {
            RT_ASSERT(0)
        }
        // new socket
        if (udbd_sock_get(sock) == RT_NULL)
        {
            LOG_E("no mem. udbd scok get failed");
            // close sock
        }
    }
    else
    {
        LOG_E("udbd_socket failed: %d", svc_result);
    }

    return sock;
}

static int udbd_closesocket(int socket)
{
    struct urpc_blob in_blob = { 0 }, out_blob = { 0 };
    int result = -1, svc_result = 0;
    ubjsonw_t* ubj;

    if (netdev_is_link_up(netdev))
    {
        ubj = ubj_write_memory();
        ubj_begin_object(ubj);
        ubj_object_write_integer(ubj, "socket", socket);
        ubj_end_object(ubj);

        svc_result = urpc_svc_exec(urpc_dst_id, "udbd_closesocket", URPC_SVC_FLAG_NEED_RSP,
                                   urpc_blob_make(&in_blob, ubj_get_memory(ubj), ubj->total, ubj->total),
                                   urpc_blob_make(&out_blob, &result, sizeof(result), sizeof(result)), rt_tick_from_millisecond(3000));
        ubj_write_end(ubj);
    }
    udbd_sock_put(socket);
    if (svc_result == 0)
    {
        LOG_D("udbd_closesocket(%d) success, result: %d", socket, result);
    }
    else
    {
        LOG_E("udbd_closesocket(%d) failed: %d", socket, svc_result);
    }

    return result;
}

static int udbd_bind(int socket, const struct sockaddr *name, socklen_t namelen)
{
    struct urpc_blob in_blob = {0}, out_blob = {0};
    int result = -1, svc_result;
    ubjsonw_t *ubj;
    struct sockaddr_in *sin = (struct sockaddr_in *) name;

    if (netdev_is_link_up(netdev) == RT_FALSE)
        return -1;

    ubj = ubj_write_memory();
    ubj_begin_object(ubj);
    ubj_object_write_integer(ubj, "socket", socket);
    ubj_object_write_string(ubj, "host", inet_ntoa(sin->sin_addr));
    ubj_object_write_integer(ubj, "port", ntohs(sin->sin_port));
    ubj_end_object(ubj);

    svc_result = urpc_svc_exec(urpc_dst_id, "udbd_bind", URPC_SVC_FLAG_NEED_RSP,
                               urpc_blob_make(&in_blob, ubj_get_memory(ubj), ubj->total, ubj->total),
                               urpc_blob_make(&out_blob, &result, sizeof(result), sizeof(result)), rt_tick_from_millisecond(3000));

    ubj_write_end(ubj);

    if (svc_result == 0)
    {
        LOG_D("udbd_bind(%d) success, result: %d", socket, result);
    }
    else
    {
        LOG_E("udbd_bind(%d) failed: %d", socket, svc_result);
    }
    return result;
}

static int udbd_listen(int socket, int backlog)
{
    struct urpc_blob in_blob = {0}, out_blob = {0};
    int result = -1, svc_result;
    ubjsonw_t *ubj;

    if (netdev_is_link_up(netdev) == RT_FALSE)
        return -1;

    ubj = ubj_write_memory();
    ubj_begin_object(ubj);
    ubj_object_write_integer(ubj, "socket", socket);
    ubj_object_write_integer(ubj, "backlog", backlog);
    ubj_end_object(ubj);

    svc_result = urpc_svc_exec(urpc_dst_id, "udbd_listen", URPC_SVC_FLAG_NEED_RSP,
                               urpc_blob_make(&in_blob, ubj_get_memory(ubj), ubj->total, ubj->total),
                               urpc_blob_make(&out_blob, &result, sizeof(result), sizeof(result)), rt_tick_from_millisecond(3000));

    ubj_write_end(ubj);

    if (svc_result == 0)
    {
        LOG_D("udbd_listen(%d) success, result: %d", socket, result);
    }
    else
    {
        LOG_E("udbd_listen(%d) failed: %d", socket, svc_result);
    }
    return result;
}

static int udbd_connect(int socket, const struct sockaddr *name, socklen_t namelen)
{
    struct urpc_blob in_blob = {0}, out_blob = {0};
    int result = -1, svc_result;
    ubjsonw_t *ubj;
    struct sockaddr_in *sin = (struct sockaddr_in *) name;

    if (netdev_is_link_up(netdev) == RT_FALSE)
        return -1;

    ubj = ubj_write_memory();
    ubj_begin_object(ubj);
    ubj_object_write_integer(ubj, "socket", socket);
    ubj_object_write_string(ubj, "host", inet_ntoa(sin->sin_addr));
    ubj_object_write_integer(ubj, "port", ntohs(sin->sin_port));
    ubj_end_object(ubj);

    svc_result = urpc_svc_exec(urpc_dst_id, "udbd_connect", URPC_SVC_FLAG_NEED_RSP,
                               urpc_blob_make(&in_blob, ubj_get_memory(ubj), ubj->total, ubj->total),
                               urpc_blob_make(&out_blob, &result, sizeof(result), sizeof(result)), rt_tick_from_millisecond(5000));

    ubj_write_end(ubj);

    if (svc_result == 0)
    {
        LOG_D("udbd_connect(%d) success, result: %d", socket, result);
    }
    else
    {
        LOG_E("udbd_connect(%d) failed: %d", socket, svc_result);
    }
    return result;
}

static int udbd_accept(int socket, struct sockaddr *addr, socklen_t *addrlen)
{
    struct urpc_blob in_blob = {0}, out_blob = {0};
    int svc_result, result = -1;
    ubjsonw_t *ubj;
    rt_uint8_t recv[64];
    /* default timeout is 24hour for blocking socket */
    rt_int32_t timeout = 24*60*60*1000;

    if (netdev_is_link_up(netdev) == RT_FALSE)
        return -1;

    struct udbd_sock *udbd_s = udbd_sock_get(socket);
    if (udbd_s)
    {
        if (udbd_s->is_nonblocking)
        {
            timeout = 1000;
        }
        else if (udbd_s->rcvtimeo > 0)
        {
            timeout = udbd_s->rcvtimeo + 1000;
        }
    }
    udbd_sock_put(socket);

    ubj = ubj_write_memory();
    ubj_begin_object(ubj);
    ubj_object_write_integer(ubj, "socket", socket);
    ubj_end_object(ubj);

    svc_result = urpc_svc_exec(urpc_dst_id, "udbd_accept", URPC_SVC_FLAG_NEED_RSP,
                               urpc_blob_make(&in_blob, ubj_get_memory(ubj), ubj->total, ubj->total),
                               urpc_blob_make(&out_blob, recv, 0, sizeof(recv)), timeout);

    ubj_write_end(ubj);

    if (svc_result == 0)
    {
        ubjh_t *ubjh = ubj_parse(recv, out_blob.len);
        if(ubjh != RT_NULL)
        {
            ubjh_t *u_sock = ubj_object_item(ubjh, "sock");
            ubjh_t *u_host  = ubj_object_item(ubjh, "host");
            ubjh_t *u_port  = ubj_object_item(ubjh, "port");

            if (ubj_is_string(u_host) && ubj_is_integer(u_sock) && ubj_is_integer(u_port))
            {
                int new_sock = (int)ubj_get_integer(u_sock);
                struct sockaddr_in *sin = (struct sockaddr_in *) addr;
                inet_aton(ubj_get_string(u_host).ptr, &sin->sin_addr);
                sin->sin_port = htons((int)ubj_get_integer(u_port));
                sin->sin_family = AF_INET;
                sin->sin_len = sizeof(struct sockaddr_in);
                rt_memset(sin->sin_zero, 0, sizeof(sin->sin_zero));
                if (udbd_sock_find(new_sock) != RT_NULL)
                {
                    RT_ASSERT(0)
                }
                // new socket
                if (udbd_sock_get(new_sock) == RT_NULL)
                {
                    LOG_E("no mem. udbd scok get failed");
                    // close sock
                }
                result = new_sock;
            }
            ubj_parse_free(ubjh);
        }
        LOG_D("udbd_accept(%d) success, result: %d", socket, result);
    }
    else
    {
        LOG_E("udbd_accept(%d) failed: %d", socket, svc_result);
    }
    return result;
}

static int udbd_sendto(int socket, const void *data, size_t size, int flags, const struct sockaddr *to, socklen_t tolen)
{
    struct urpc_blob in_blob = { 0 }, out_blob = { 0 };
    int result = -1, svc_result;
    ubjsonw_t* ubj;
    struct sockaddr_in *sin = (struct sockaddr_in *)to;

    if (netdev_is_link_up(netdev) == RT_FALSE)
        return -1;

    ubj = ubj_write_memory();
    ubj_begin_object(ubj);
    ubj_object_write_integer(ubj, "socket", socket);
    ubj_object_write_buffer(ubj, "data", data, UBJ_TYPE_UINT8, size);
    ubj_object_write_integer(ubj, "flags", flags);
    if (sin)
    {
        ubj_object_write_string(ubj, "host", inet_ntoa(sin->sin_addr));
        ubj_object_write_integer(ubj, "port", ntohs(sin->sin_port));
    }
    ubj_end_object(ubj);

    svc_result = urpc_svc_exec(urpc_dst_id, "udbd_sendto", URPC_SVC_FLAG_NEED_RSP,
                               urpc_blob_make(&in_blob, ubj_get_memory(ubj), ubj->total, ubj->total),
                               urpc_blob_make(&out_blob, &result, sizeof(result), sizeof(result)), rt_tick_from_millisecond(3000));

    ubj_write_end(ubj);

    if (svc_result == 0)
    {
        LOG_D("udbd_sendto(%d, %d) success, result: %d", socket, size, result);
    }
    else
    {
        LOG_E("udbd_sendto(%d, %d) failed: %d", socket, size, svc_result);
    }

    return result;
}

static int cache_recvfrom(struct udbd_sock *udbd_s, void *data, size_t len)
{
    int tmp_len = 0;
    int result = 0;

    rt_mutex_take(&lock, RT_WAITING_FOREVER);
    if (udbd_s != RT_NULL)
    {
        if (udbd_s->cache_buf_len < udbd_s->cache_pos || udbd_s->cache_len < udbd_s->cache_pos)
        {
            RT_ASSERT(0);
        }
        tmp_len = udbd_s->cache_len - udbd_s->cache_pos;
        if (tmp_len > 0)
        {
            int cp_len = tmp_len > (int)len ? (int)len : tmp_len;
            rt_memcpy(data, &udbd_s->cache_buf[udbd_s->cache_pos], cp_len);
            udbd_s->cache_pos += cp_len;
            result = cp_len;
        }
    }
    rt_mutex_release(&lock);
    return result;
}

static int udbd_recvfrom(int socket, void *data, size_t len, int flags, struct sockaddr *from, socklen_t *fromlen)
{
#define RECV_MIN_SIZE_PER_SECOND 2048

    struct urpc_blob in_blob = { 0 }, out_blob = { 0 };
    int result = 0, svc_result = 0, recv_len = 0;
    ubjsonw_t* ubj;
    struct sockaddr_in *sin = (struct sockaddr_in *)from;
    rt_uint8_t *recv_buf = data;
    struct udbd_sock *udbd_s;
    rt_int32_t timeout = RT_ALIGN(len, RECV_MIN_SIZE_PER_SECOND) / RECV_MIN_SIZE_PER_SECOND * 1000;

    if (netdev_is_link_up(netdev) == RT_FALSE)
        return -1;

    // recv from cache
    udbd_s = udbd_sock_get(socket);
    result = cache_recvfrom(udbd_s, data, len);
    len -= result;
    data = recv_buf + result;

    if (len == 0)
    {
        udbd_sock_put(socket);
        return result;
    }

    // Try to read more data into the buffer
    if (udbd_s && len < udbd_s->cache_buf_len)
    {
        recv_buf = udbd_s->cache_buf;
        recv_len = udbd_s->cache_buf_len;
    }
    else
    {
        // Read directly to user buffer
        recv_buf = data;
        recv_len = len;
    }

    if (result > 0)
    {
        flags |= MSG_DONTWAIT;
    }

    if (udbd_s)
    {
        if (udbd_s->is_nonblocking)
        {
            timeout = RT_ALIGN(len, RECV_MIN_SIZE_PER_SECOND) / RECV_MIN_SIZE_PER_SECOND * 1000;
        }
        else if (udbd_s->rcvtimeo > 0)
        {
            timeout = udbd_s->rcvtimeo;
        }
    }

    ubj = ubj_write_memory();
    ubj_begin_object(ubj);
    ubj_object_write_integer(ubj, "socket", socket);
    ubj_object_write_integer(ubj, "len", recv_len);
    ubj_object_write_integer(ubj, "flags", flags);
    if (sin)
    {
        ubj_object_write_string(ubj, "host", inet_ntoa(sin->sin_addr));
        ubj_object_write_integer(ubj, "port", ntohs(sin->sin_port));
    }
    ubj_object_write_integer(ubj, "timeout", timeout);
    ubj_end_object(ubj);

    svc_result = urpc_svc_exec(urpc_dst_id, "udbd_recvfrom", URPC_SVC_FLAG_NEED_RSP,
                               urpc_blob_make(&in_blob, ubj_get_memory(ubj), ubj->total, ubj->total),
                               urpc_blob_make(&out_blob, recv_buf, 0, recv_len), 1000 + timeout);

    ubj_write_end(ubj);


    if (svc_result == 0)
    {
        // recv from cache
        if (udbd_s && recv_buf == udbd_s->cache_buf)
        {
            rt_mutex_take(&lock, RT_WAITING_FOREVER);
            if (out_blob.len > udbd_s->cache_buf_len)
            {
                RT_ASSERT(0);
            }
            udbd_s->cache_len = out_blob.len;
            udbd_s->cache_pos = 0;
            result += cache_recvfrom(udbd_s, data, len);
            rt_mutex_release(&lock);

        }
        else
        {
            result += out_blob.len;
        }
        /* check recv timeout */
        if (len > 0 && result == 0)
        {
            /* recv timeout for blocking mode or no data for nonblocking mode */
            result = -1;
        }

        if (result >= 0)
            LOG_D("udbd_recvfrom(%d, %d:%d) = %d success, svc_result: %d", socket, len, recv_len, result, svc_result);
        else
            LOG_W("udbd_recvfrom(%d, %d:%d) from remote timeout(%d)", socket, len, recv_len, timeout);
    }
    else
    {
        result = -1;
        LOG_E("udbd_recvfrom(%d, %d:%d) failed: %d", socket, len, recv_len, svc_result);
    }

    udbd_sock_put(socket);

    return result;
}

static int __setsockopt(int socket, int level, int optname, const void *optval, socklen_t optlen)
{
    struct urpc_blob in_blob = { 0 }, out_blob = { 0 };
    int result = -1, svc_result;
    ubjsonw_t* ubj;

    if (netdev_is_link_up(netdev) == RT_FALSE)
        return -1;

    ubj = ubj_write_memory();
    ubj_begin_object(ubj);
    ubj_object_write_integer(ubj, "socket", socket);
    ubj_object_write_integer(ubj, "level", level);
    ubj_object_write_integer(ubj, "optname", optname);
    ubj_object_write_array(ubj, "optval");
    for (socklen_t i = 0; i < optlen; i++)
    {
        ubj_write_uint8(ubj, *(((uint8_t *) optval) + i));
    }
    ubj_end_array(ubj);
    ubj_end_object(ubj);

    svc_result = urpc_svc_exec(urpc_dst_id, "udbd_setsockopt", URPC_SVC_FLAG_NEED_RSP,
                               urpc_blob_make(&in_blob, ubj_get_memory(ubj), ubj->total, ubj->total),
                               urpc_blob_make(&out_blob, &result, sizeof(result), sizeof(result)), rt_tick_from_millisecond(3000));

    ubj_write_end(ubj);

    if (svc_result == 0)
    {
        LOG_D("udbd_setsockopt(%d) success, result: %d", socket, result);
    }
    else
    {
        LOG_E("udbd_setsockopt(%d) failed: %d", socket, svc_result);
    }

    return result;
}

static int udbd_setsockopt(int socket, int level, int optname, const void *optval, socklen_t optlen)
{
    int result = -1;
    struct udbd_sock *s;
    const struct timeval *timeout = optval;
    int new_timeout = 0, old_timeout = 0;

    if (netdev_is_link_up(netdev) == RT_FALSE)
        return -1;

    // get timeout
    s = udbd_sock_get(socket);
    if (s != RT_NULL && level == SOL_SOCKET)
    {
        if (optname == SO_RCVTIMEO)
        {
            old_timeout = s->rcvtimeo;
            if (timeout)
                new_timeout = timeout->tv_sec * 1000 + timeout->tv_usec / 1000;
            else
                new_timeout = 0;
            result = 0;
        }
        else if (optname == SO_SNDTIMEO)
        {
            old_timeout = s->sndtimeo;
            if (timeout)
                new_timeout = timeout->tv_sec * 1000 + timeout->tv_usec / 1000;
            else
                new_timeout = 0;
            result = 0;
        }
    }
    udbd_sock_put(socket);
    // set socket opt
    if (old_timeout != new_timeout)
    {
        result = __setsockopt(socket, level, optname, optval, optlen);
        if (result == 0)
        {
            if (optname == SO_RCVTIMEO)
                s->rcvtimeo = new_timeout;
            else if (optname == SO_SNDTIMEO)
                s->sndtimeo = new_timeout;
        }
    }
    return result;
}

static int udbd_shutdown(int socket, int how)
{
    struct urpc_blob in_blob = { 0 }, out_blob = { 0 };
    int result = -1, svc_result;
    ubjsonw_t* ubj;

    if (netdev_is_link_up(netdev) == RT_FALSE)
        return -1;

    ubj = ubj_write_memory();
    ubj_begin_object(ubj);
    ubj_object_write_integer(ubj, "socket", socket);
    ubj_object_write_integer(ubj, "how", how);
    ubj_end_object(ubj);

    svc_result = urpc_svc_exec(urpc_dst_id, "udbd_shutdown", URPC_SVC_FLAG_NEED_RSP,
                               urpc_blob_make(&in_blob, ubj_get_memory(ubj), ubj->total, ubj->total),
                               urpc_blob_make(&out_blob, &result, sizeof(result), sizeof(result)), rt_tick_from_millisecond(3000));

    ubj_write_end(ubj);

    if (svc_result == 0)
    {
        LOG_D("udbd_shutdown(%d) success, result: %d", socket, result);
    }
    else
    {
        LOG_E("udbd_shutdown(%d) failed: %d", socket, svc_result);
    }

    return result;
}

static int udbd_getpeername(int socket, struct sockaddr *name, socklen_t *namelen)
{
    struct urpc_blob in_blob = {0}, out_blob = {0};
    int svc_result, result = -1;
    ubjsonw_t *ubj;
    rt_uint8_t recv[64];

    if (netdev_is_link_up(netdev) == RT_FALSE)
        return -1;

    ubj = ubj_write_memory();
    ubj_begin_object(ubj);
    ubj_object_write_integer(ubj, "socket", socket);
    ubj_end_object(ubj);

    svc_result = urpc_svc_exec(urpc_dst_id, "udbd_getpeername", URPC_SVC_FLAG_NEED_RSP,
                               urpc_blob_make(&in_blob, ubj_get_memory(ubj), ubj->total, ubj->total),
                               urpc_blob_make(&out_blob, recv, 0, sizeof(recv)), 3000);

    ubj_write_end(ubj);

    if (svc_result == 0)
    {
        ubjh_t *ubjh = ubj_parse(recv, out_blob.len);
        if(ubjh != RT_NULL)
        {
            ubjh_t *u_host  = ubj_object_item(ubjh, "host");
            ubjh_t *u_port  = ubj_object_item(ubjh, "port");

            if (ubj_is_string(u_host) && ubj_is_integer(u_port))
            {
                struct sockaddr_in *sin = (struct sockaddr_in *) name;
                inet_aton(ubj_get_string(u_host).ptr, &sin->sin_addr);
                sin->sin_port = htons((int)ubj_get_integer(u_port));
                sin->sin_family = AF_INET;
                sin->sin_len = sizeof(struct sockaddr_in);
                rt_memset(sin->sin_zero, 0, sizeof(sin->sin_zero));
                result = 0;
            }
            ubj_parse_free(ubjh);
        }
        LOG_D("udbd_getpeername(%d) success, result: %d", socket, result);
    }
    else
    {
        LOG_E("udbd_getpeername(%d) failed: %d", socket, svc_result);
    }
    return result;
}

static int udbd_getsockname(int socket, struct sockaddr *name, socklen_t *namelen)
{
    struct urpc_blob in_blob = {0}, out_blob = {0};
    int svc_result, result = -1;
    ubjsonw_t *ubj;
    rt_uint8_t recv[64];

    if (netdev_is_link_up(netdev) == RT_FALSE)
        return -1;

    ubj = ubj_write_memory();
    ubj_begin_object(ubj);
    ubj_object_write_integer(ubj, "socket", socket);
    ubj_end_object(ubj);

    svc_result = urpc_svc_exec(urpc_dst_id, "udbd_getsockname", URPC_SVC_FLAG_NEED_RSP,
                               urpc_blob_make(&in_blob, ubj_get_memory(ubj), ubj->total, ubj->total),
                               urpc_blob_make(&out_blob, recv, 0, sizeof(recv)), 3000);

    ubj_write_end(ubj);

    if (svc_result == 0)
    {
        ubjh_t *ubjh = ubj_parse(recv, out_blob.len);
        if(ubjh != RT_NULL)
        {
            ubjh_t *u_host  = ubj_object_item(ubjh, "host");
            ubjh_t *u_port  = ubj_object_item(ubjh, "port");

            if (ubj_is_string(u_host) && ubj_is_integer(u_port))
            {
                struct sockaddr_in *sin = (struct sockaddr_in *) name;
                inet_aton(ubj_get_string(u_host).ptr, &sin->sin_addr);
                sin->sin_port = htons((int)ubj_get_integer(u_port));
                sin->sin_family = AF_INET;
                sin->sin_len = sizeof(struct sockaddr_in);
                rt_memset(sin->sin_zero, 0, sizeof(sin->sin_zero));
                result = 0;
            }
            ubj_parse_free(ubjh);
        }
        LOG_D("udbd_getsockname(%d) success, result: %d", socket, result);
    }
    else
    {
        LOG_E("udbd_getsockname(%d) failed: %d", socket, svc_result);
    }
    return result;
}

static void udbd_socket_set_nonblocking(struct udbd_sock *udbd_s, rt_bool_t val)
{
    if (!udbd_s)
        return;

    if (val)
    {
        struct timeval timeout = { 1, 1 };

        udbd_setsockopt(udbd_s->socket, SOL_SOCKET, SO_RCVTIMEO, (void*) &timeout, sizeof(timeout));
        /* using timeout '0' for nonblocking socket */
        timeout.tv_sec = 0;
        timeout.tv_usec = 0;
        udbd_setsockopt(udbd_s->socket, SOL_SOCKET, SO_RCVTIMEO, (void*) &timeout, sizeof(timeout));
        udbd_s->is_nonblocking = RT_TRUE;
    }
    else
    {
        udbd_s->is_nonblocking = RT_FALSE;
    }
}

static int udbd_fcntl(int socket, int cmd, int val)
{
    struct udbd_sock *udbd_s;
    int ret = -1;

    if (netdev_is_link_up(netdev) == RT_FALSE)
        return -1;

    udbd_s = udbd_sock_get(socket);

    switch (cmd)
    {
    case F_GETFL:
        ret = udbd_s->is_nonblocking ? O_NONBLOCK : 0;
        break;
    case F_SETFL:
        /* only support O_NONBLOCK */
        if ((val & ~O_NONBLOCK) == 0)
        {
            if (val & O_NONBLOCK)
            {
                udbd_socket_set_nonblocking(udbd_s, RT_TRUE);
            }
            else
            {
                udbd_socket_set_nonblocking(udbd_s, RT_FALSE);
            }
            ret = 0;
        }
        else
        {
            ret = -RT_ENOSYS; /* not yet implemented */
        }
        break;
    default:
        ret = -RT_ENOSYS; /* not yet implemented */
        break;
    }
    return ret;
}

static int udbd_ioctl(int socket, long cmd, void *argp)
{
    struct udbd_sock *udbd_s;
    rt_bool_t is_nonblocking;

    if (netdev_is_link_up(netdev) == RT_FALSE)
        return -1;

    udbd_s = udbd_sock_get(socket);

    switch (cmd)
    {
    case (long) FIONBIO:
        is_nonblocking = RT_FALSE;
        if (argp && *(uint32_t*) argp)
        {
            is_nonblocking = RT_TRUE;
        }
        udbd_socket_set_nonblocking(udbd_s, is_nonblocking);
        return 0;
    default:
        break;
    }
    /* not yet implemented */
    return -ENOSYS;
}

static int udbd_ioctlsocket(int socket, long cmd, void *arg)
{
    switch (cmd)
    {
    case F_GETFL:
    case F_SETFL:
        return udbd_fcntl(socket, cmd, (int) arg);

    default:
        return udbd_ioctl(socket, cmd, arg);
    }
}

#ifdef SAL_USING_POSIX
static int udbd_poll(struct dfs_file *file, struct rt_pollreq *req)
{
    struct sal_socket *sal_sock;
    struct udbd_sock *sock;
    int req_event = 0, recv_event = 0;
    struct udbd_set read_set = { 0 };
    struct udbd_set write_set = { 0 };
    struct udbd_set error_set = { 0 };

    if (netdev_is_link_up(netdev) == RT_FALSE)
    {
        return 0;
    }
    /* get sock */
    sal_sock = sal_get_socket((int)file->data);
    if(!sal_sock)
    {
        return -1;
    }
    sock = udbd_sock_get((int)sal_sock->user_data);
    if (sock == RT_NULL)
    {
        return -1;
    }
    // check select
    if ((int)req->_key & POLLIN)
    {
        udbd_sock_set(sock->socket, &read_set);
    }
    if ((int)req->_key & POLLOUT)
    {
        udbd_sock_set(sock->socket, &write_set);
    }
    if ((int)req->_key & POLLERR)
    {
        udbd_sock_set(sock->socket, &error_set);
    }
    if (__udbd_select(&read_set, &write_set, &error_set, 0) >= 0)
    {
        if ((int)req->_key & POLLIN)
        {
            req_event |= POLLIN;
            if (udbd_sock_isset(sock->socket, &read_set))
            {
                recv_event |= POLLIN;
            }
        }
        if ((int)req->_key & POLLOUT)
        {
            req_event |= POLLOUT;
            if (udbd_sock_isset(sock->socket, &write_set))
            {
                recv_event |= POLLOUT;
            }
        }
        if ((int)req->_key & POLLERR)
        {
            req_event |= POLLERR;
            if (udbd_sock_isset(sock->socket, &error_set))
            {
                recv_event |= POLLERR;
            }
        }
        if (recv_event == 0)
        {
            rt_poll_add(&sock->wait, req);
            sock->req_event = req_event;
            if (udbd_wakeup_pipe[1] >= 0)
            {
                udbd_sendto(udbd_wakeup_pipe[1], "", 1, 0, RT_NULL, 0);
            }
            rt_event_send(&link_up_flag, UDBD_SELECT_EVENT_FLAG);
        }
    }
    else
    {
        recv_event = -1;
    }
    udbd_sock_put((int)sal_sock->user_data);
    return recv_event;
}
#endif

static const struct sal_socket_ops udbd_socket_ops =
{
    udbd_socket,
    udbd_closesocket,
    udbd_bind,
    udbd_listen,
    udbd_connect,
    udbd_accept,
    udbd_sendto,
    udbd_recvfrom,
    RT_NULL,
    udbd_setsockopt,
    udbd_shutdown,
    udbd_getpeername,
    udbd_getsockname,
    udbd_ioctlsocket,
#ifdef SAL_USING_POSIX
    udbd_poll,
#endif /* SAL_USING_POSIX */
};

static int32_t ipstr_atol(const char *nptr)
{
    int32_t total = 0;
    char sign = '+';
    /* jump space */
    while ((unsigned int)((*nptr) - 9) < 5u || (*nptr) == ' ')
    {
        ++nptr;
    }

    if (*nptr == '-' || *nptr == '+')
    {
        sign = *nptr++;
    }

    while ((*nptr) >= '0' && (*nptr) <= '9')
    {
        total = 10 * total + ((*nptr++) - '0');
    }
    return (sign == '-') ? -total : total;
}

/* IP address to unsigned int type */
static uint32_t ipstr_to_u32(char *ipstr)
{
    char bytes[4] = { 0 }, *ipBytes = &bytes[0];
    uint32_t i;

    for (i = 0; i < 4; i++, ipstr++)
    {
        ipBytes[i] = (char) ipstr_atol(ipstr);
        if ((ipstr = strchr(ipstr, '.')) == RT_NULL)
        {
            break;
        }
    }
    return *(uint32_t *) ipBytes;
}

static struct hostent *udbd_gethostbyname(const char *name)
{
    ip_addr_t addr;
    char ipstr[16] = { 0 };
    /* buffer variables for at_gethostbyname() */
    static struct hostent s_hostent;
    static char *s_aliases;
    static ip_addr_t s_hostent_addr;
    static ip_addr_t *s_phostent_addr[2];
    static char s_hostname[DNS_MAX_NAME_LENGTH + 1];
    struct urpc_blob in_blob = { 0 }, out_blob = { 0 };
    int svc_result;
    ubjsonw_t* ubj;

    if (name == RT_NULL || (netdev_is_link_up(netdev) == RT_FALSE))
        return RT_NULL;

    ubj = ubj_write_memory();
    ubj_begin_object(ubj);
    ubj_object_write_string(ubj, "name", name);
    ubj_end_object(ubj);

    svc_result = urpc_svc_exec(urpc_dst_id, "udbd_gethostbyname", URPC_SVC_FLAG_NEED_RSP,
                               urpc_blob_make(&in_blob, ubj_get_memory(ubj), ubj->total, ubj->total),
                               urpc_blob_make(&out_blob, &ipstr, 0, sizeof(ipstr)), rt_tick_from_millisecond(3000));

    ubj_write_end(ubj);

    if (svc_result < 0 || rt_strlen(ipstr) == 0)
        return RT_NULL;

#if NETDEV_IPV4 && NETDEV_IPV6
    addr.u_addr.ip4.addr = ipstr_to_u32(ipstr);
    addr.type = IPADDR_TYPE_V4;
#elif NETDEV_IPV4
    addr.addr = ipstr_to_u32(ipstr);
#elif NETDEV_IPV6
#error "not support IPV6."
#endif /* NETDEV_IPV4 && NETDEV_IPV6 */

    /* fill hostent structure */
    s_hostent_addr = addr;
    s_phostent_addr[0] = &s_hostent_addr;
    s_phostent_addr[1] = RT_NULL;
    strncpy(s_hostname, name, DNS_MAX_NAME_LENGTH);
    s_hostname[DNS_MAX_NAME_LENGTH] = 0;
    s_hostent.h_name = s_hostname;
    s_aliases = RT_NULL;
    s_hostent.h_aliases = &s_aliases;
    s_hostent.h_addrtype = AF_UDBD;
    s_hostent.h_length = sizeof(ip_addr_t);
    s_hostent.h_addr_list = (char**) &s_phostent_addr;

    return &s_hostent;
}

static int udbd_getaddrinfo(const char *nodename, const char *servname, const struct addrinfo *hints,
        struct addrinfo **res)
{
    int port_nr = 0;
    ip_addr_t addr = { 0 };
    struct addrinfo *ai;
    struct sockaddr_storage *sa;
    size_t total_size = 0;
    size_t namelen = 0;

    if (res == RT_NULL)
        return EAI_FAIL;
    *res = RT_NULL;

    if ((nodename == RT_NULL) && (servname == RT_NULL))
        return EAI_NONAME;

    if ((hints != RT_NULL)
            && (hints->ai_family != AF_UDBD && hints->ai_family != AF_INET && hints->ai_family != AF_UNSPEC))
        return EAI_FAMILY;

    if (servname != RT_NULL)
    {
        /* service name specified: convert to port number */
        port_nr = atoi(servname);
        if ((port_nr <= 0) || (port_nr > 0xffff))
        {
            return EAI_SERVICE;
        }
    }

    if (nodename != RT_NULL)
    {
        struct hostent *hostent = udbd_gethostbyname(nodename);
        if (hostent == RT_NULL)
            return EAI_FAIL;

        #if NETDEV_IPV4 && NETDEV_IPV6
            addr.type = IPADDR_TYPE_V4;
            if ((addr.u_addr.ip4.addr = ((struct in_addr *) hostent->h_addr)->s_addr) == 0)
            {
                return EAI_FAIL;
            }
        #elif NETDEV_IPV4
            addr.addr = ((struct in_addr *) hostent->h_addr)->s_addr;
        #elif NETDEV_IPV6
        #error "not support IPV6."
        #endif /* NETDEV_IPV4 && NETDEV_IPV6 */
    }

    total_size = sizeof(struct addrinfo) + sizeof(struct sockaddr_storage);
    if (nodename != RT_NULL)
    {
        namelen = strlen(nodename);
        if (namelen > DNS_MAX_NAME_LENGTH)
        {
            /* invalid name length */
            return EAI_FAIL;
        }
        RT_ASSERT(total_size + namelen + 1 > total_size);
        total_size += namelen + 1;
    }
    /* If this fails, please report to lwip-devel! :-) */
    RT_ASSERT(total_size <= sizeof(struct addrinfo) + sizeof(struct sockaddr_storage) + DNS_MAX_NAME_LENGTH + 1);
    ai = (struct addrinfo *) rt_malloc(total_size);
    if (ai == RT_NULL)
    {
        return EAI_MEMORY;
    }
    memset(ai, 0, total_size);
    /* cast through void* to get rid of alignment warnings */
    sa = (struct sockaddr_storage *) (void *) ((uint8_t *) ai + sizeof(struct addrinfo));
    struct sockaddr_in *sa4 = (struct sockaddr_in *) sa;
    /* set up sockaddr */
#if NETDEV_IPV4 && NETDEV_IPV6
    sa4->sin_addr.s_addr = addr.u_addr.ip4.addr;
    sa4->type = IPADDR_TYPE_V4;
#elif NETDEV_IPV4
    sa4->sin_addr.s_addr = addr.addr;
#elif NETDEV_IPV6
#error "not support IPV6."
#endif /* NETDEV_IPV4 && NETDEV_IPV6 */
    sa4->sin_family = AF_INET;
    sa4->sin_len = sizeof(struct sockaddr_in);
    sa4->sin_port = htons((uint16_t)port_nr);
    ai->ai_family = AF_INET;

    /* set up addrinfo */
    if (hints != RT_NULL)
    {
        /* copy socktype & protocol from hints if specified */
        ai->ai_socktype = hints->ai_socktype;
        ai->ai_protocol = hints->ai_protocol;
    }
    if (nodename != RT_NULL)
    {
        /* copy nodename to canonname if specified */
        ai->ai_canonname = ((char *) ai + sizeof(struct addrinfo) + sizeof(struct sockaddr_storage));
        memcpy(ai->ai_canonname, nodename, namelen);
        ai->ai_canonname[namelen] = 0;
    }
    ai->ai_addrlen = sizeof(struct sockaddr_storage);
    ai->ai_addr = (struct sockaddr *) sa;

    *res = ai;

    return 0;
}

static void udbd_freeaddrinfo(struct addrinfo *ai)
{
    struct addrinfo *next;

    while (ai != NULL)
    {
        next = ai->ai_next;
        rt_free(ai);
        ai = next;
    }
}

static const struct sal_netdb_ops udbd_netdb_ops =
{
    udbd_gethostbyname,
    NULL,
    udbd_getaddrinfo,
    udbd_freeaddrinfo,
};

static const struct sal_proto_family udbd_inet_family =
{
    AF_UDBD,
    AF_INET,
    &udbd_socket_ops,
    &udbd_netdb_ops,
};

static int udbd_netdev_set_up(struct netdev *netdev)
{
    netdev_low_level_set_status(netdev, RT_TRUE);
    return RT_EOK;
}

static int udbd_netdev_set_down(struct netdev *netdev)
{
    netdev_low_level_set_status(netdev, RT_FALSE);
    return RT_EOK;
}

#ifdef RT_USING_FINSH
static int udbd_netdev_ping(struct netdev *netdev, const char *host, size_t data_len, uint32_t timeout, struct netdev_ping_resp *ping_resp)
{
    struct hostent *hostent = udbd_gethostbyname(host);
    struct urpc_blob in_blob = { 0 }, out_blob = { 0 };
    int result = -1, svc_result;
    ubjsonw_t* ubj = ubj_write_memory();

    if (hostent == RT_NULL || (netdev_is_link_up(netdev) == RT_FALSE))
        return -RT_ERROR;

    ubj_begin_object(ubj);
    ubj_object_write_integer(ubj, "data_len", data_len);
    ubj_object_write_string(ubj, "host", host);
    ubj_object_write_integer(ubj, "timeout", timeout / RT_TICK_PER_SECOND);
    ubj_end_object(ubj);

    svc_result = urpc_svc_exec(urpc_dst_id, "udbd_ping", URPC_SVC_FLAG_NEED_RSP,
                               urpc_blob_make(&in_blob, ubj_get_memory(ubj), ubj->total, ubj->total),
                               urpc_blob_make(&out_blob, &result, sizeof(result), sizeof(result)),
            rt_tick_from_millisecond(1000) + timeout);

    ubj_write_end(ubj);

    if (svc_result != RT_EOK)
    {
        return svc_result;
    }

    ping_resp->ip_addr.addr = ((struct in_addr *) hostent->h_addr)->s_addr;
    ping_resp->ticks = result;
    ping_resp->data_len = data_len;
    ping_resp->ttl = 0;

    return result;
}
#endif

static int udbd_netdev_set_dns_server(struct netdev *netif, uint8_t dns_num, ip_addr_t *dns_server)
{
    extern void netdev_low_level_set_dns_server(struct netdev *netdev, uint8_t dns_num, const ip_addr_t *dns_server);

    netdev_low_level_set_dns_server(netif, dns_num, dns_server);
    return RT_EOK;
}

static const struct netdev_ops udbd_netdev_ops =
{
    udbd_netdev_set_up,
    udbd_netdev_set_down,
    RT_NULL,
    udbd_netdev_set_dns_server,
    RT_NULL,
#ifdef RT_USING_FINSH
    udbd_netdev_ping,
    RT_NULL,
#endif
};

static void udb_status_callback(udbd_status_cb *cb, enum udbd_status_cb_type type)
{
    if (type == UDBD_STATUS_CB_TYPE_SERVER_LINK_UP)
    {
        startup_time = time(RT_NULL);
        if (netdev_is_link_up(netdev) == RT_FALSE)
        {
            netdev_low_level_set_link_status(netdev, RT_TRUE);
        }
#ifdef SAL_USING_POSIX
        rt_event_send(&link_up_flag, UDBD_LINK_UP_EVENT_FLAG);
#endif
        LOG_I("udbd netdev link up");
    }
    else if (type == UDBD_STATUS_CB_TYPE_SERVER_LINK_DOWN)
    {
        startup_time = 0;
        if (netdev_is_link_up(netdev) == RT_TRUE)
        {
            netdev_low_level_set_link_status(netdev, RT_FALSE);
        }
#ifdef SAL_USING_POSIX
        rt_event_control(&link_up_flag, RT_IPC_CMD_RESET, RT_NULL);
#endif
        LOG_I("udbd netdev link down");
    }
}

static int _udbd_sal_init(void)
{
    { /* netdev register */
#define HWADDR_LEN                     6

        struct mcf_pkt_info *info = RT_NULL;
    
        extern struct mcf_pkt_info* _mcf_trans_get_cur_session_info(void);
        
        info = _mcf_trans_get_cur_session_info();
        if (info == NULL)
        {
            LOG_E("can't get pkt info.");
            return -1;
        }
        
        urpc_dst_id = info->dst_id;

        netdev = (struct netdev *)rt_calloc(1, sizeof(struct netdev));
        if (netdev == RT_NULL)
            RT_ASSERT(0);

        rt_mutex_init(&lock, "udbd_sock", RT_IPC_FLAG_PRIO);
#ifdef SAL_USING_POSIX
        rt_event_init(&link_up_flag, "udbd_sock", RT_IPC_FLAG_PRIO);
        rt_thread_init(&thread, "udbd_sock", udbd_poll_check_thread, RT_NULL,
            thread_stack, sizeof(thread_stack), 15, 20);
        rt_thread_startup(&thread);
#endif
        netdev->flags = NETDEV_FLAG_UP;
        netdev->mtu = MCF_PKT_MAX_SIZE;
        netdev->ops = &udbd_netdev_ops;
        netdev->hwaddr_len = HWADDR_LEN;
        netdev->sal_user_data = (void *) &udbd_inet_family;
        netdev_register(netdev, "udbd", RT_NULL);
        ip_addr_t ip = {.addr = IPADDR_BROADCAST};
        /* set ip NOT 0.0.0.0, make sure the sal_check_netdev_internet_up can be worked */
        netdev_low_level_set_ipaddr(netdev, &ip);
        netdev_low_level_set_status(netdev, RT_TRUE);
    }

    {
        static udbd_status_cb udbd_sal_cb;
        udbd_set_status_callback(&udbd_sal_cb, udb_status_callback);
    }

    return 0;
}
URPC_SVC_STATIC_REGISTER_ALIAS(_udbd_sal_init, urpc_sal_init)

#if 0
void udb_net_test(void)
{
#define HOST "127.0.0.1"
#define PORT 8080
#define TEST_MSG "hello world\n"
#include <sys/socket.h>
    struct sockaddr_in addr;
    char send_msg[] = { TEST_MSG };

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    addr.sin_family = PF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    connect(sock, (const struct sockaddr *)&addr, sizeof(addr));
    send(sock, send_msg, sizeof(send_msg), 0);
    int flag = 1;
    setsockopt(sock,
               IPPROTO_TCP,     /* set option at TCP level */
               TCP_NODELAY,     /* name of option */
               (void *) &flag,  /* the cast is historical cruft */
               sizeof(int));    /* length of option value */
#include <sys/time.h>
    struct timeval timeout;
    timeout.tv_sec = 2;
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
//    recv(sock, recv_msg, sizeof(recv_msg), 0);
    shutdown(sock, 0);
    closesocket(sock);
    gethostbyname("www.rt-thread.org");
}
MSH_CMD_EXPORT(udb_net_test, udb_net_test)
#endif

#endif /* RT_USING_SAL */
