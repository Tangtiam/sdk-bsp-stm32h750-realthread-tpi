/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2023-03-06     armink       the first version
 */

#include <rtthread.h>
#include <dfs.h>
#include <dfs_file.h>
#include <poll.h>
#include <udbd.h>
#include <mcf_link.h>
#include <httpclient.h>
#include <ubj.h>
#include <sys/errno.h>

#define DBG_TAG    "udbd.httpclient"
#define DBG_LVL    DBG_INFO//DBG_LOG
#include <rtdbg.h>

/* default receive or send timeout */
#define HTTPCLIENT_DEFAULT_TIMEO        6

enum {
  HTTP_EVT_RCVPLUS,
  HTTP_EVT_RCVMINUS,
  HTTP_EVT_SENDPLUS,
  HTTP_EVT_SENDMINUS,
  HTTP_EVT_ERROR
};

struct udbd_httpclient
{
    int timeout_ms;       /* timeout millisecond */
    char *req_url;
    int remote_socket;    /* remote rpc httpclient socket number */
    /** number of times data was received, set by event_callback(),
        tested by the receive and select functions */
    short rcvevent;
    /** number of times data was ACKed (free send buffer), set by event_callback(),
        tested by select */
    short sendevent;
    /** error happened for this socket, set by event_callback(), tested by select */
    short errevent;

    rt_wqueue_t wait_head;
};

static struct rt_mutex lock;
static time_t startup_time;

static int udbd_httpclient_socket(httpclient_session_t session);
static int udbd_httpclient_closesocket(httpclient_session_t session);

static httpclient_session_t udbd_httpclient_session_create(size_t header_sz)
{
    httpclient_session_t session = RT_NULL;
    struct udbd_httpclient *uhc = RT_NULL;

    if (udbd_get_status() != UDBD_STATUS_SERVER_LINK_UP || udbd_get_server_ver_num() < 0x10100)
        return RT_NULL;

    uhc = rt_calloc(1, sizeof(struct udbd_httpclient));
    if (uhc)
    {
        uhc->timeout_ms = HTTPCLIENT_DEFAULT_TIMEO * 1000;
        rt_wqueue_init(&uhc->wait_head);
        session = webclient_session_create(header_sz);
        session->user_data = uhc;
        session->socket = udbd_httpclient_socket(session);
    }
    return session;
}

static int udbd_httpclient_session_close(httpclient_session_t session)
{
    struct udbd_httpclient *uhc;
    int svc_result, result = -1;

    RT_ASSERT(session);
    RT_ASSERT(session->user_data);

    if (udbd_get_status() != UDBD_STATUS_SERVER_LINK_UP)
        return -1;

    uhc = (struct udbd_httpclient *) session->user_data;
    if (uhc)
    {
        struct urpc_blob in_blob = { 0 }, out_blob = { 0 };
        ubjsonw_t* ubj;
        ubj = ubj_write_memory();

        rt_mutex_take(&lock, RT_WAITING_FOREVER);
        ubj_begin_object(ubj);
        ubj_object_write_integer(ubj, "socket", uhc->remote_socket);
        ubj_end_object(ubj);
        rt_mutex_release(&lock);

        svc_result = urpc_svc_exec(UDBD_SERVER_DID, "udbd_http_close", URPC_SVC_FLAG_NEED_RSP,
                                   urpc_blob_make(&in_blob, ubj_get_memory(ubj), ubj->total, ubj->total),
                                   urpc_blob_make(&out_blob, &result, sizeof(result), sizeof(result)), rt_tick_from_millisecond(3000));

        ubj_write_end(ubj);

        if (svc_result == 0)
        {
            result = 0;
            LOG_D("udbd_http_close(%d) success, result: %d", session->socket, result);
        }
        else
        {
            result = -RT_ETIMEOUT;
            LOG_E("udbd_http_close(%d) failed: %d", session->socket, svc_result);
        }

        {
            rt_mutex_take(&lock, RT_WAITING_FOREVER);
            if (uhc->req_url)
            {
                rt_free(uhc->req_url);
            }
            rt_mutex_release(&lock);
        }
    }

    {
        udbd_httpclient_closesocket(session);
        /* webclient_close will exec closesocket when socket is >= 0, so set -1 now */
        session->socket = -1;
        rt_free(session->user_data);
        webclient_close(session);
    }

    return result;
}

static int udbd_httpclient_set_timeout(httpclient_session_t session, int millisecond)
{
    struct udbd_httpclient *uhc;

    RT_ASSERT(session);
    RT_ASSERT(session->user_data);

    if (udbd_get_status() != UDBD_STATUS_SERVER_LINK_UP)
        return -1;

    rt_mutex_take(&lock, RT_WAITING_FOREVER);

    uhc = (struct udbd_httpclient *) session->user_data;
    uhc->timeout_ms = millisecond;

    rt_mutex_release(&lock);
    return 0;
}

/* send or receive data from server */
static int udbd_httpclient_read(httpclient_session_t session, void *buffer, size_t size)
{
    rt_uint8_t *recv_buf = buffer;
    struct udbd_httpclient *uhc;
    int svc_result, result = -1;
    struct urpc_blob in_blob = { 0 }, out_blob = { 0 };

    RT_ASSERT(session);
    RT_ASSERT(session->user_data);

    if (udbd_get_status() != UDBD_STATUS_SERVER_LINK_UP)
        return -1;

    uhc = (struct udbd_httpclient *) session->user_data;
    if (uhc)
    {
        ubjsonw_t* ubj;
        ubj = ubj_write_memory();

        rt_mutex_take(&lock, RT_WAITING_FOREVER);
        ubj_begin_object(ubj);
        ubj_object_write_integer(ubj, "socket", uhc->remote_socket);
        ubj_object_write_integer(ubj, "size", size);
        ubj_end_object(ubj);
        rt_mutex_release(&lock);

        svc_result = urpc_svc_exec(UDBD_SERVER_DID, "udbd_http_read", URPC_SVC_FLAG_NEED_RSP,
                                   urpc_blob_make(&in_blob, ubj_get_memory(ubj), ubj->total, ubj->total),
                                   urpc_blob_make(&out_blob, recv_buf, 0, size),
                                   rt_tick_from_millisecond(HTTPCLIENT_DEFAULT_TIMEO * 1000));

        ubj_write_end(ubj);

        if (svc_result == 0)
        {
            result = out_blob.len;
            LOG_D("udbd_httpclient_read(%d) success, result: %d", session->socket, result);
        }
        else
        {
            result = -RT_ETIMEOUT;
            LOG_E("udbd_httpclient_read(%d) failed: %d", session->socket, svc_result);
        }
    }

    if (result >= 0)
    {
        rt_mutex_take(&lock, RT_WAITING_FOREVER);
        /* cab recv next data now */
        uhc->rcvevent = 1;
        rt_mutex_release(&lock);
    }


    return result;
}

static int udbd_httpclient_write(httpclient_session_t session, const void *buffer, size_t size)
{
    struct udbd_httpclient *uhc;
    int svc_result, result = -1;
    struct urpc_blob in_blob = { 0 }, out_blob = { 0 };

    RT_ASSERT(session);
    RT_ASSERT(session->user_data);

    if (udbd_get_status() != UDBD_STATUS_SERVER_LINK_UP)
        return -1;

    uhc = (struct udbd_httpclient *) session->user_data;
    if (uhc)
    {
        ubjsonw_t* ubj;
        ubj = ubj_write_memory();

        rt_mutex_take(&lock, RT_WAITING_FOREVER);
        ubj_begin_object(ubj);
        ubj_object_write_integer(ubj, "socket", uhc->remote_socket);
        ubj_object_write_buffer(ubj, "data", buffer, UBJ_TYPE_UINT8, size);
        ubj_object_write_integer(ubj, "size", size);
        ubj_end_object(ubj);
        rt_mutex_release(&lock);

        svc_result = urpc_svc_exec(UDBD_SERVER_DID, "udbd_http_send", URPC_SVC_FLAG_NEED_RSP,
                                   urpc_blob_make(&in_blob, ubj_get_memory(ubj), ubj->total, ubj->total),
                                   urpc_blob_make(&out_blob, &result, sizeof(result), sizeof(result)),
                                   rt_tick_from_millisecond(HTTPCLIENT_DEFAULT_TIMEO * 1000));

        ubj_write_end(ubj);

        if (svc_result == 0)
        {
            LOG_D("udbd_httpclient_write(%d) success, result: %d", session->socket, result);
        }
        else
        {
            result = -RT_ETIMEOUT;
            LOG_E("udbd_httpclient_write(%d) failed: %d", session->socket, svc_result);
        }
    }

    if (result >= 0)
    {
        rt_mutex_take(&lock, RT_WAITING_FOREVER);
        /* can send next data now */
        uhc->sendevent = 1;
        uhc->rcvevent = 1;
        rt_mutex_release(&lock);
    }

    return result;
}

static int udbd_httpclient_connect(httpclient_session_t session, const char *URI)
{
    struct udbd_httpclient *uhc;

    RT_ASSERT(session);
    RT_ASSERT(session->user_data);

    if (udbd_get_status() != UDBD_STATUS_SERVER_LINK_UP)
        return -1;

    /* WearService was NOT support HTTPS when version less V1.2.0 */
    if (rt_strncmp(URI, "https://", 8) == 0 && udbd_get_server_ver_num() < 0x10200)
    {
        /* clean udbd_httpclient_session info */
        rt_free(session->user_data);
        session->user_data = RT_NULL;
        udbd_httpclient_closesocket(session);
        session->socket = -1;

        return webclient_connect(session, URI);
    }

    rt_mutex_take(&lock, RT_WAITING_FOREVER);
    uhc = (struct udbd_httpclient *) session->user_data;
    uhc->req_url = rt_strdup(URI);
    rt_mutex_release(&lock);

    if (uhc->req_url)
    {
        return 0;
    }
    else
    {
        return -RT_ENOMEM;
    }
}

static int udbd_httpclient_send_header(httpclient_session_t session, int method)
{
    struct udbd_httpclient *uhc;
    int svc_result, result = -1;
    struct urpc_blob in_blob = { 0 }, out_blob = { 0 };
    rt_uint8_t recv[64];

    RT_ASSERT(session);
    RT_ASSERT(session->user_data);

    if (udbd_get_status() != UDBD_STATUS_SERVER_LINK_UP)
        return -1;

    uhc = (struct udbd_httpclient *)session->user_data;
    if (uhc)
    {
        ubjsonw_t* ubj;
        ubj = ubj_write_memory();

        rt_mutex_take(&lock, RT_WAITING_FOREVER);
        ubj_begin_object(ubj);
        ubj_object_write_string(ubj, "url", uhc->req_url);
        ubj_object_write_string(ubj, "header", session->header->buffer);
        ubj_object_write_integer(ubj, "timeout", uhc->timeout_ms);
        ubj_object_write_integer(ubj, "startup_time", startup_time);
        ubj_end_object(ubj);
        rt_mutex_release(&lock);

        svc_result = urpc_svc_exec(UDBD_SERVER_DID, "udbd_http_request", URPC_SVC_FLAG_NEED_RSP,
                                   urpc_blob_make(&in_blob, ubj_get_memory(ubj), ubj->total, ubj->total),
                                   urpc_blob_make(&out_blob, recv, 0, sizeof(recv)),
                                   rt_tick_from_millisecond(HTTPCLIENT_DEFAULT_TIMEO * 1000));

        ubj_write_end(ubj);

        if (svc_result == 0)
        {
            ubjh_t *ubjh = ubj_parse(recv, out_blob.len);
            if (ubjh != RT_NULL)
            {
                ubjh_t *u_remote_socket = ubj_object_item(ubjh, "remote_socket");
                ubjh_t *u_result = ubj_object_item(ubjh, "result");

                if (ubj_is_integer(u_remote_socket) && ubj_is_integer(u_result))
                {
                    rt_mutex_take(&lock, RT_WAITING_FOREVER);
                    uhc->remote_socket = ubj_get_integer(u_remote_socket);
                    rt_mutex_release(&lock);
                    result = ubj_get_integer(u_result);
                }
                ubj_parse_free(ubjh);
            }
            LOG_D("udbd_httpclient_send_header(%d) success, result: %d", session->socket, result);
        }
        else
        {
            result = -RT_ETIMEOUT;
            LOG_E("udbd_httpclient_send_header(%d) failed: %d", session->socket, svc_result);
        }
    }

    if (result >= 0)
    {
        rt_mutex_take(&lock, RT_WAITING_FOREVER);
        /* can send next data now */
        uhc->sendevent = 1;
        uhc->rcvevent = 1;
        rt_mutex_release(&lock);
    }

    return result;
}

static int udbd_httpclient_handle_response(httpclient_session_t session)
{
    struct udbd_httpclient *uhc;
    int svc_result, result = -1;
    struct urpc_blob in_blob = { 0 }, out_blob = { 0 };
    rt_uint8_t *recv = rt_calloc(1, session->header->size);

    RT_ASSERT(session);
    RT_ASSERT(session->user_data);

    if (udbd_get_status() != UDBD_STATUS_SERVER_LINK_UP)
        return -1;

    //TODO 返回 /chunk_sz&offset/content_remainder
    uhc = (struct udbd_httpclient *) session->user_data;
    if (uhc && recv)
    {
        ubjsonw_t* ubj;
        ubj = ubj_write_memory();

        rt_mutex_take(&lock, RT_WAITING_FOREVER);
        ubj_begin_object(ubj);
        ubj_object_write_integer(ubj, "socket", uhc->remote_socket);
        ubj_end_object(ubj);
        rt_mutex_release(&lock);

        svc_result = urpc_svc_exec(UDBD_SERVER_DID, "udbd_http_getresponse", URPC_SVC_FLAG_NEED_RSP,
                                   urpc_blob_make(&in_blob, ubj_get_memory(ubj), ubj->total, ubj->total),
                                   urpc_blob_make(&out_blob, recv, 0, session->header->size),
                                   rt_tick_from_millisecond(HTTPCLIENT_DEFAULT_TIMEO * 1000));

        ubj_write_end(ubj);

        if (svc_result == 0)
        {
            ubjh_t *ubjh = ubj_parse(recv, out_blob.len);
            if (ubjh != RT_NULL)
            {
                ubjh_t *u_status = ubj_object_item(ubjh, "status");
                ubjh_t *u_header_len = ubj_object_item(ubjh, "header_len");
                ubjh_t *u_header = ubj_object_item(ubjh, "header");
                ubjh_t *u_content_length = ubj_object_item(ubjh, "content_length");

                if (ubj_is_integer(u_status) && ubj_is_integer(u_header_len) && ubj_is_string(u_header)
                        && ubj_is_integer(u_content_length))
                {
                    session->resp_status = ubj_get_integer(u_status);
                    session->header->length = ubj_get_integer(u_header_len);
                    rt_strncpy(session->header->buffer, ubj_get_string(u_header).ptr, session->header->length);
                    for (int i = 0; i < session->header->length; i ++)
                    {
                        /* set terminal charter */
                        if (session->header->buffer[i] == '\r' || session->header->buffer[i] == '\n')
                            session->header->buffer[i] = '\0';
                    }
                    session->content_length = ubj_get_integer(u_content_length);
                    session->content_remainder = session->content_length ? (size_t) session->content_length : 0xFFFFFFFF;
                    result = session->resp_status;
                }
                ubj_parse_free(ubjh);
            }
            LOG_D("udbd_httpclient_handle_response(%d) success, result: %d", session->socket, result);
        }
        else
        {
            result = -RT_ETIMEOUT;
            LOG_E("udbd_httpclient_handle_response(%d) failed: %d", session->socket, svc_result);
        }
    }

    rt_free(recv);

    return result;
}

static int udbd_httpclient_get_bytes_avail(httpclient_session_t session)
{
    return 0;
}

static const struct httpclient_ops udbd_httpclient_ops =
{
    udbd_httpclient_session_create,
    udbd_httpclient_session_close,
    udbd_httpclient_set_timeout,
    udbd_httpclient_read,
    udbd_httpclient_write,
    udbd_httpclient_connect,
    udbd_httpclient_send_header,
    udbd_httpclient_handle_response,
    udbd_httpclient_get_bytes_avail,
};

static void udbd_httpclient_event_callback(struct urpc_blob* input)
{
    // TODO 多线程并发，加锁后，会影响性能
    ubjh_t *ubjh = ubj_parse(input->buf, input->len);
    uint32_t event = 0;
    struct udbd_httpclient *uhc = RT_NULL;

    rt_mutex_take(&lock, RT_WAITING_FOREVER);

    if (ubjh != RT_NULL)
    {
        ubjh_t *u_fd = ubj_object_item(ubjh, "fd");
        ubjh_t *u_evt = ubj_object_item(ubjh, "evt");

        if (ubj_is_integer(u_fd) && ubj_is_integer(u_evt))
        {
            int fd = (int) ubj_get_integer(u_fd);
            int evt = (int) ubj_get_integer(u_evt);
            struct dfs_fd *_dfs_fd;

            _dfs_fd = fd_get(fd);
            if (_dfs_fd != NULL && _dfs_fd->type == FT_USER)
            {
                httpclient_session_t session = (httpclient_session_t) _dfs_fd->data;
                uhc = (struct udbd_httpclient *) session->user_data;
                /* Set event as required */
                switch (evt)
                {
                case HTTP_EVT_RCVPLUS:
                    uhc->rcvevent++;
                    break;
                case HTTP_EVT_RCVMINUS:
                    uhc->rcvevent--;
                    break;
                case HTTP_EVT_SENDPLUS:
                    uhc->sendevent = 1;
                    break;
                case HTTP_EVT_SENDMINUS:
                    uhc->sendevent = 0;
                    break;
                case HTTP_EVT_ERROR:
                    uhc->errevent = 1;
                    break;
                default:
                    break;
                }
                if (uhc->rcvevent > 0)
                    event |= POLLIN;
                if (uhc->sendevent)
                    event |= POLLOUT;
                if (uhc->errevent)
                    event |= POLLERR;

            }
            fd_put(_dfs_fd); /* put this dfs fd */
        }
        ubj_parse_free(ubjh);
    }

    if (event && uhc)
    {
        rt_wqueue_wakeup(&uhc->wait_head, (void*) event);
    }
    rt_mutex_release(&lock);
}
URPC_SVC_STATIC_REGISTER_ALIAS(udbd_httpclient_event_callback, httpclient_evt_cb);

//TODO 待测试 poll 性能
static int udbd_httpclient_poll(struct dfs_fd *file, struct rt_pollreq *req)
{
    int mask = 0;
    httpclient_session_t session;
    struct udbd_httpclient *uhc;

    session = (httpclient_session_t) file->data;
    if (!session)
    {
        return -1;
    }
    uhc = (struct udbd_httpclient *) session->user_data;

    rt_mutex_take(&lock, RT_WAITING_FOREVER);

    rt_poll_add(&uhc->wait_head, req);

    if (uhc->rcvevent)
    {
        mask |= POLLIN;
    }
    if (uhc->sendevent)
    {
        mask |= POLLOUT;
    }
    if (uhc->errevent)
    {
        mask |= POLLERR;
        /* clean error event */
        uhc->errevent = 0;
    }

    rt_mutex_release(&lock);

    return mask;
}

const struct dfs_file_ops _udbd_httpclient_fops =
{
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    udbd_httpclient_poll,
};

static int udbd_httpclient_socket(httpclient_session_t session)
{
    int fd;
    struct dfs_fd *d;

    /* allocate a fd */
    fd = fd_new();
    if (fd < 0)
    {
        rt_set_errno(-ENOMEM);
        return -1;
    }
    d = fd_get(fd);

    /* this is a user fd */
    d->type = FT_USER;
    d->path = NULL;
    d->fops = &_udbd_httpclient_fops;
    d->flags = O_RDWR; /* set flags as read and write */
    d->size = 0;
    d->pos = 0;
    d->data = (void *) session;

    /* release the ref-count of fd */
    fd_put(d);

    return fd;
}

static int udbd_httpclient_closesocket(httpclient_session_t session)
{
    int error = 0;
    struct dfs_fd *d;

    d = fd_get(session->socket);
    if (d == RT_NULL)
    {
        rt_set_errno(-EBADF);
        return -1;
    }

    /* socket has been closed, delete it from file system fd */
    fd_put(d);
    fd_put(d);

    return error;
}

static void udb_status_callback(udbd_status_cb *cb, enum udbd_status_cb_type type)
{
    if (type == UDBD_STATUS_CB_TYPE_SERVER_LINK_UP)
    {
        startup_time = time(RT_NULL);
    }
    else if (type == UDBD_STATUS_CB_TYPE_SERVER_LINK_DOWN)
    {
        startup_time = 0;
    }
}

static int _udbd_httpclient_init(void)
{
    rt_mutex_init(&lock, "udbd_http", RT_IPC_FLAG_PRIO);
    httpclient_set_ops(&udbd_httpclient_ops);

    {
        static udbd_status_cb udbd_sal_cb;
        udbd_set_status_callback(&udbd_sal_cb, udb_status_callback);
    }

    return 0;
}
UDBD_MODULE_EXPORT(udbd_httpclient_module, _udbd_httpclient_init, RT_NULL, 5);
