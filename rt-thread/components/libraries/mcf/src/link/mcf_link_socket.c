/*
 * Copyright (c) 2006-2020, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2020-05-25     chenyong     first version
 * 2021-11-26     KevinXu      modify to implement tcp
 */

#include <mcf.h>
#include <mcf_link.h>
#include <mcf_trans.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>
#ifdef PKG_USING_UDBD
#include <udbd.h>
#endif

#define DBG_TAG               "mcf.link.tcp"
#ifdef MCF_USING_DEBUG
#define DBG_LVL               DBG_LOG
#else
#define DBG_LVL               DBG_LOG
#endif /* MCF_DEBUG */
#include <rtdbg.h>

#define MCF_TCP_FRAME_LEN_LEN          3
/* 6 bytes frame header, frame header(1byte) + frame len(2bytes) + frame ID (1byte)  + reserve(1byte) + frame type(1byte) */
#define MCF_TCP_FRAME_HEAD_LEN         6
/* end sign(1byte) */
#define MCF_TCP_FRAME_TAIL_LEN         1
/* D2D package header length */
#define MCF_D2D_PKT_HEAD_LEN           4
 /* frame and packet total header */
#define MCF_TCP_PKT_HEAD_LEN          (MCF_TCP_FRAME_HEAD_LEN + MCF_TCP_FRAME_TAIL_LEN + MCF_D2D_PKT_HEAD_LEN)

#define MCF_TCP_FRAME_HEAD_SIGN        (0xFC)
#define MCF_TCP_FRAME_HEAD_SIGN_LEN    (1)
#define MCF_TCP_FRAME_END_SIGN         (0xCF)
#define MCF_TCP_FRAME_END_SIGN_LEN     1
#ifdef MCF_PKT_MAX_SIZE
#define MCF_LINK_SOCKET_MTU              (MCF_PKT_MAX_SIZE + MCF_TCP_PKT_HEAD_LEN)
#else
#define MCF_LINK_SOCKET_MTU              (1024 + MCF_TCP_PKT_HEAD_LEN)
#endif

extern uint8_t g_frame_id;

/* thread to receive data */
static struct rt_thread tcp_thread;
static char tcp_thread_stack[4096];
static uint8_t tcp_tmp_rx_buf[MCF_LINK_SOCKET_MTU];

#define TCP_THREAD_PRIORITY     12
#define MCF_TCP_LISTEN_PORT     7758

enum mcf_tcp_frame_index
{
    MCF_TCP_FRAME_INDEX_HEAD = 0,
    MCF_TCP_FRAME_INDEX_LEN1,
    MCF_TCP_FRAME_INDEX_LEN2,
    MCF_TCP_FRAME_INDEX_ID,
    MCF_TCP_FRAME_INDEX_RESERVE,
    MCF_TCP_FRAME_INDEX_TYPE,
};

/* MCF link tcp frame */
struct mcf_tcp_frame
{
    uint8_t head;                      /* frame header */
    uint16_t len;                      /* frame length */
    uint8_t id;                        /* frame ID */
    uint8_t reserve;                   /* reserve bytes */
    struct mcf_frame frame;            /* MCF common frame */
    uint8_t end_sign;                  /* end sign */
};

struct mcf_link_tcp
{
    struct mcf_link link;              /* link object */

    int sock, clnt;
    struct sockaddr_in sock_addr, clnt_addr;
    socklen_t sock_len, clnt_len;
    int pipe_fb[2];
    void *user_data;                   /* user-specific data */
};

/* send *len* bytes of data from *buf* to tcp. returns length of ACTUAL send data. BLOCKED! */
static ssize_t _mcf_link_tcp_send(struct mcf_link_tcp *link, void *buf, size_t len)
{
    if (link == RT_NULL || link->clnt < 0)
    {
        return -1;
    }
    return write(link->pipe_fb[1], buf, len);
}

/* receive *len* bytes of data from tcp to *buf*. returns length of ACTUAL received data. NON-BLOCKED! */
static ssize_t _mcf_link_tcp_recv(struct mcf_link_tcp *link, void *buf, size_t len)
{
    if (link == RT_NULL || link->clnt < 0)
    {
        return -1;
    }

    return recv(link->clnt, buf, len, 0);
}

/* launch tcp thread and initialize *mcf_link_tcp* structure */
static mcf_err_t _mcf_link_tcp_init(struct mcf_link_tcp *link)
{
    link->clnt = -1;

    if ((link->sock = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        LOG_E("Socket error");
        return -MCF_FAILD;
    }

    link->sock_addr.sin_family = AF_INET;
    link->sock_addr.sin_port = htons(MCF_TCP_LISTEN_PORT);
    link->sock_addr.sin_addr.s_addr = INADDR_ANY;
    link->sock_len = sizeof(link->sock_addr);
    rt_memset(&(link->sock_addr.sin_zero), 0, sizeof(link->sock_addr.sin_zero));

    if (bind(link->sock, (struct sockaddr *)&link->sock_addr, link->sock_len) == -1)
    {
        LOG_E("Unable to bind");
        closesocket(link->sock);
        link->sock = -1;
        return -MCF_FAILD;
    }

    if (listen(link->sock, 5) == -1)
    {
        LOG_E("Listen error");
        closesocket(link->sock);
        link->sock = -1;
        return -MCF_FAILD;
    }

    LOG_I("TCPServer Waiting for client on port %d...", MCF_TCP_LISTEN_PORT);

    return MCF_OK;
}

/* allocate link tcp frame object */
static struct mcf_tcp_frame *link_tcp_frame_alloc(struct mcf_pbuf *pbuf)
{
    struct mcf_tcp_frame *tcp_frame = RT_NULL;

    tcp_frame = rt_calloc(1, sizeof(struct mcf_link_tcp));
    if (tcp_frame == RT_NULL)
    {
        return RT_NULL;
    }

    tcp_frame->head = MCF_TCP_FRAME_HEAD_SIGN;
    tcp_frame->len = MCF_TCP_FRAME_HEAD_LEN + MCF_TCP_FRAME_TAIL_LEN + pbuf->len;
    tcp_frame->id = mcf_link_frame_id_add();
    tcp_frame->end_sign = MCF_TCP_FRAME_END_SIGN;
    tcp_frame->frame.type = 1;
    tcp_frame->frame.payload = pbuf->buf;

    return tcp_frame;
}

/* free link tcp frame object */
static void mcf_tcp_frame_free(struct mcf_tcp_frame *link_tcp)
{
    if (link_tcp)
    {
        rt_free(link_tcp);
    }
}

/* get raw packet buffer by link tcp frame object */
static mcf_err_t link_tcp_frame_to_pbuf(struct mcf_link_tcp *link_tcp, struct mcf_tcp_frame *frame, struct mcf_pbuf *pbuf)
{
    uint8_t *buffer = RT_NULL;
    size_t payload_len;

    /* fill buffer data by frame information */
    buffer = pbuf->buf;
    buffer[MCF_TCP_FRAME_INDEX_HEAD]    = frame->head;
    buffer[MCF_TCP_FRAME_INDEX_LEN1]    = frame->len >> 8;
    buffer[MCF_TCP_FRAME_INDEX_LEN2]    = frame->len & 0xFF;
    buffer[MCF_TCP_FRAME_INDEX_ID]      = frame->id;
    buffer[MCF_TCP_FRAME_INDEX_RESERVE] = frame->reserve;
    buffer[MCF_TCP_FRAME_INDEX_TYPE]    = frame->frame.type;

    /* get frame buffer length and update packet buffer length */
    payload_len = frame->len - MCF_TCP_FRAME_HEAD_LEN - MCF_TCP_FRAME_TAIL_LEN;
    rt_memcpy(buffer + MCF_TCP_FRAME_HEAD_LEN, frame->frame.payload, payload_len);

    /* splice link tcp frame end sign */
    buffer[frame->len - MCF_TCP_FRAME_END_SIGN_LEN] = MCF_TCP_FRAME_END_SIGN;
    /* calculate packet buffer length */
    pbuf->len = MCF_TCP_FRAME_HEAD_LEN + payload_len + MCF_TCP_FRAME_TAIL_LEN;

    return MCF_OK;
}

/* check link tcp frame data header */
static mcf_err_t frame_data_header_check(struct mcf_link_tcp *link_tcp)
{
    size_t recv_bufsz;
    uint8_t *recv_buffer = RT_NULL;

    recv_buffer = link_tcp->link.recv_buf + link_tcp->link.frames_len;
    recv_bufsz = link_tcp->link.recv_bufsz - link_tcp->link.frames_len;

    /* check frame header is normal */
    if (recv_buffer[0] != (uint8_t) MCF_TCP_FRAME_HEAD_SIGN)
    {
        size_t index;
        size_t recv_buf_len = link_tcp->link.cur_recv_bufsz;

        /* find link tcp frame header in remaining receive buffer */
        for (index = 1; index < recv_buf_len; index++)
        {
            if (recv_buffer[index] == (uint8_t) MCF_TCP_FRAME_HEAD_SIGN)
            {
                /* clean dirty tcp frame data */
                rt_memmove(recv_buffer, recv_buffer + index, recv_bufsz - index);
                link_tcp->link.cur_recv_bufsz -= index;
                break;
            }
        }
        /* not find MCF tcp frame header sign */
        if (index == recv_buf_len)
        {
           /*clean dirty tcp frame data */
            rt_memset(recv_buffer, 0x0, recv_bufsz);
            link_tcp->link.cur_recv_bufsz = 0;
            return -MCF_FAILD;
        }
    }

    return MCF_OK;
}

/* check link tcp frame data, return frame length */
static size_t frame_data_check(struct mcf_link_tcp *link_tcp)
{
    size_t recv_bufsz;
    uint8_t *recv_buffer = RT_NULL;
    size_t frame_len = 0;

    recv_buffer = link_tcp->link.recv_buf + link_tcp->link.frames_len;
    recv_bufsz = link_tcp->link.recv_bufsz - link_tcp->link.frames_len;

    /* check receive buffer can get the received frame length */
    if (link_tcp->link.cur_recv_bufsz < MCF_TCP_FRAME_LEN_LEN)
    {
        goto _exit;
    }

    if (frame_data_header_check(link_tcp) != MCF_OK)
    {
        LOG_E("link tcp frame data header check failed.");
        goto _exit;
    }

    frame_len = (recv_buffer[1] << 8) + recv_buffer[2];

    /* check frame length */
    if (frame_len > MCF_LINK_SOCKET_MTU || 0 == frame_len || recv_buffer[5] >= MCF_D2D_PKT_TYPE_MAX)
    {
        /* clean abnormal packet */
        rt_memset(recv_buffer, 0x0, recv_bufsz);
        link_tcp->link.cur_recv_bufsz = 0;

        LOG_E("link tcp frame data frame length check failed.");
        return 0;
    }

    /* if the length is not enough, continue to receive data */
    if (frame_len > link_tcp->link.cur_recv_bufsz)
    {
        return 0;
    }

    /* already get the full length link tcp frame, check end sign */
    if (recv_buffer[frame_len - MCF_TCP_FRAME_END_SIGN_LEN] != (uint8_t) MCF_TCP_FRAME_END_SIGN)
    {
        // TODO find frame header, change frame header address pointer
        /* clean error tcp frame data */
        rt_memmove(recv_buffer, recv_buffer + frame_len, recv_bufsz - frame_len);
        link_tcp->link.cur_recv_bufsz -= frame_len;

        LOG_E("link tcp frame data end sign check failed.");
        return 0;
    }

_exit:
    return frame_len;
}

static int service_wait(struct mcf_link_tcp *link_tcp, fd_set *r, fd_set *e)
{
    int max_fd = link_tcp->clnt > link_tcp->pipe_fb[0] ? link_tcp->clnt : link_tcp->pipe_fb[0];
    struct timeval timeout = {5, 0};
    int result = 0;

    /* wait data */
    while (result == 0)
    {
        /* init select fds */
        FD_ZERO(r);
        FD_SET(link_tcp->clnt, r);
        FD_SET(link_tcp->pipe_fb[0], r);
        FD_ZERO(e);
        FD_SET(link_tcp->clnt, e);
        FD_SET(link_tcp->pipe_fb[0], e);
        result = select(max_fd + 1, r, RT_NULL, e, &timeout);
    }
    return result;
}

static int send_to_client(struct mcf_link_tcp *link_tcp, fd_set *r, fd_set *e)
{
    int result = 0;
    rt_uint8_t buff[512];

    if (FD_ISSET(link_tcp->pipe_fb[0], e))
    {
        /* pipe close */
        RT_ASSERT(0);
    }
    if (FD_ISSET(link_tcp->pipe_fb[0], r))
    {
        result = read(link_tcp->pipe_fb[0], buff, sizeof(buff));
        if (result > 0)
        {
            if (send(link_tcp->clnt, buff, result, 0) == result)
            {
                return 0;
            }
            else
            {
                /* send error */
                return -1;
            }
        }
        else if (result < 0)
        {
            /* unknown error */
            RT_ASSERT(0);
        }
    }
    return 0;
}

static int recv_from_client(struct mcf_link_tcp *link_tcp, fd_set *r, fd_set *e)
{
    int sem_cnt = 0;

    /* check error */
    if (FD_ISSET(link_tcp->clnt, e))
    {
        return -1;
    }
    /* recv data */
    if (FD_ISSET(link_tcp->clnt, r))
    {
        int recv_len = 0;
        /* get frame_head first in order to get the length of this frame */
        recv_len = _mcf_link_tcp_recv(link_tcp, tcp_tmp_rx_buf, MCF_LINK_SOCKET_MTU);
        if (recv_len > 0)
        {
            size_t frame_len;
            rt_base_t level;
            level = rt_hw_interrupt_disable();
            uint8_t *recv_buffer = link_tcp->link.recv_buf + link_tcp->link.frames_len;
            size_t recv_bufsz  = link_tcp->link.recv_bufsz - link_tcp->link.frames_len;
            /* check link tcp receive buffer is full */
            if ((link_tcp->link.cur_recv_bufsz + recv_len) > recv_bufsz)
            {
                //LOG_E("link tcp receive buffer size(%d) is full.", link_tcp->link.recv_bufsz);
            }
            else
            {
                /* receive data of specified length and update receive buffer length */
                memcpy(recv_buffer + link_tcp->link.cur_recv_bufsz, tcp_tmp_rx_buf, recv_len);
                link_tcp->link.cur_recv_bufsz += recv_len;
                /* there may be more than one frame in rbuf */
                while ((frame_len = frame_data_check(link_tcp)) != 0)
                {
                    /* link tcp frame check success, send receive event notice */
                    link_tcp->link.frames_len += frame_len;
                    link_tcp->link.cur_recv_bufsz -= frame_len;
                    /* send frame data received notice */
                    sem_cnt += 1;
                }
            }
            rt_hw_interrupt_enable(level);
            while (sem_cnt > 0)
            {
                mcf_link_notice_send(&(link_tcp->link));
                sem_cnt -= 1;
            }
        }
        else
        {
            /* recv error */
            return -1;
        }
    }
    return 0;
}

static void client_disconnect(struct mcf_link_tcp *link_tcp)
{
    /* close client socket */
    closesocket(link_tcp->clnt);
    link_tcp->clnt = -1;

    while (1)
    {
        rt_uint8_t buff[512];
        fd_set read_fd;
        struct timeval timeout = {0, 10 * 1000};
        FD_ZERO(&read_fd);
        FD_SET(link_tcp->pipe_fb[0], &read_fd);
        /* wait fifo */
        int result = select(link_tcp->pipe_fb[0] + 1, &read_fd, RT_NULL, RT_NULL, &timeout);
        if (result == 0)
        {
            /* fifo empty */
            break;
        }
        else if (result < 0)
        {
            /* pipe error */
        }
        else
        {
            read(link_tcp->pipe_fb[0], buff, sizeof(buff));
        }
    }
}

static void service_client(struct mcf_link_tcp *link_tcp)
{
    fd_set read_fd, err_fd;

    while (RT_TRUE)
    {
        /* wait */
        if (service_wait(link_tcp, &read_fd, &err_fd) < 0)
            break;
        /* send */
        if (send_to_client(link_tcp, &read_fd, &err_fd) < 0)
            break;
        /* recv */
        if (recv_from_client(link_tcp, &read_fd, &err_fd) < 0)
            break;
    }
    client_disconnect(link_tcp);
}

static void _sock_init(struct mcf_link_tcp *link_tcp)
{
    link_tcp->sock = -1;
    while (link_tcp->sock < 0)
    {
        if (pipe(link_tcp->pipe_fb) == 0)
        {
            if (_mcf_link_tcp_init(link_tcp) != MCF_OK)
            {
                LOG_D("mcf tcp link init failed. retry");
                close(link_tcp->pipe_fb[0]);
                link_tcp->pipe_fb[0] = -1;
                close(link_tcp->pipe_fb[1]);
                link_tcp->pipe_fb[1] = -1;
            }
        }
        else
        {
            LOG_E("MCF create pipe error.");
        }
        //socket create failed.
        if (link_tcp->sock < 0)
        {
            rt_thread_mdelay(5000);
        }
    }
}

/* thread to receive data from tcp stream */
static void tcp_thread_entry(void *param)
{
    struct mcf_link_tcp *link_tcp = param;

    _sock_init(link_tcp);

    while (RT_TRUE)
    {
        link_tcp->clnt_len = sizeof(link_tcp->clnt_addr);
        link_tcp->clnt = accept(link_tcp->sock, (struct sockaddr *)&link_tcp->clnt_addr, &link_tcp->clnt_len);
        if (link_tcp->clnt < 0)
        {
            LOG_E("accept connection failed!");
            continue;
        }
        LOG_I("tcp client is connected");
        service_client(link_tcp);
        LOG_I("tcp client disconnected");
        mcf_low_level_set_link_status(&(link_tcp->link), RT_FALSE);
#ifdef PKG_USING_UDBD
        udbd_low_level_post_status_changed_event(UDBD_STATUS_CB_TYPE_SERVER_LINK_DOWN);
#endif
    }
}

/* link tcp object initialize */
static mcf_err_t mcf_link_tcp_init(struct mcf_link *link)
{

    return MCF_OK;
}

/* send data by link tcp object */
static mcf_err_t mcf_link_tcp_send(struct mcf_link *link, struct mcf_pbuf *pbuf, int32_t timeout)
{
    struct mcf_tcp_frame *tcp_frame = RT_NULL;
    struct mcf_link_tcp *link_tcp = (struct mcf_link_tcp *)link;
    struct mcf_pbuf *out_pbuf = RT_NULL;
    mcf_err_t result;
    size_t write_len;

    RT_ASSERT(link);
    RT_ASSERT(pbuf && pbuf->buf && (pbuf->size > 0));

    /* allocate link tcp frame */
    tcp_frame = link_tcp_frame_alloc(pbuf);
    if (tcp_frame == RT_NULL)
    {
        result = -MCF_MEM_FULL;
        LOG_E("tcp alloc frame memory failed.");
        goto __exit;
    }

    /* allocate packet buffer for  */
    out_pbuf = mcf_pbuf_alloc(MCF_LINK_SOCKET_MTU);
    if (out_pbuf == RT_NULL)
    {
        result = -MCF_MEM_FULL;
        LOG_E("tcp alloc packet memory failed.");
        goto __exit;
    }

    /* MCF link tcp frame to packet buffer */
    result = link_tcp_frame_to_pbuf(link_tcp, tcp_frame, out_pbuf);
    if (result != MCF_OK)
    {
        LOG_E("tcp pack frame failed.");
        goto __exit;
    }
    /* send packet buffer data by tcp device */
    write_len = _mcf_link_tcp_send(link_tcp, out_pbuf->buf, out_pbuf->len);

    if (write_len != out_pbuf->len)
    {
        result = -MCF_FAILD;
        LOG_E("link tcp send frame failed.");
        goto __exit;
    }

    mcf_print_raw_data((uint8_t *)(out_pbuf->buf) + MCF_TCP_FRAME_HEAD_LEN,
            out_pbuf->len - MCF_TCP_FRAME_HEAD_LEN - MCF_TCP_FRAME_TAIL_LEN);

__exit:

    /* free link tcp frame and packet buffer */
    mcf_tcp_frame_free(tcp_frame);
    mcf_pbuf_free(out_pbuf);

    return result;
}

/* receive data by link tcp object */
static mcf_err_t mcf_link_tcp_recv(struct mcf_link *link, struct mcf_pbuf *pbuf, int32_t timeout)
{
    size_t frame_len;
    rt_base_t level;

    RT_ASSERT(link && link->recv_buf);
    RT_ASSERT(pbuf && pbuf->buf);

    if (link->frames_len == 0)
    {
        return -MCF_FAILD;
    }

    level = rt_hw_interrupt_disable();

    /* get receive frame length */
    frame_len = (link->recv_buf[1] << 8) | link->recv_buf[2];
    RT_ASSERT(pbuf->size >= frame_len);

    /* fill receive data to packet buffer */
    pbuf->len = frame_len - MCF_TCP_FRAME_HEAD_LEN - MCF_TCP_FRAME_TAIL_LEN;
    rt_memcpy(pbuf->buf, link->recv_buf + MCF_TCP_FRAME_HEAD_LEN, pbuf->len);

    /* update link receive buffer */
    rt_memmove(link->recv_buf, link->recv_buf + frame_len, link->recv_bufsz - frame_len);
    RT_ASSERT(link->frames_len >= frame_len);
    link->frames_len -= frame_len;

    rt_hw_interrupt_enable(level);

    mcf_print_raw_data(pbuf->buf, pbuf->len);

    return MCF_OK;
}

/* link tcp object control feature */
static mcf_err_t mcf_link_tcp_control(struct mcf_link *link, int cmd, void *arg)
{
    return MCF_OK;
}

/* link tcp object operation */
static const struct mcf_link_ops tcp_link_ops =
{
    .init    = mcf_link_tcp_init,
    .send    = mcf_link_tcp_send,
    .recv    = mcf_link_tcp_recv,
    .control = mcf_link_tcp_control,
};

/**
 * MCF link tcp object register.
 *
 * @param port_id link port ID
 * @param sock_port tcp port number as string
 * @param uaer_data reserved user data
 *
 * @return <0: MCF link tcp object register failed
 *         =0: MCF link tcp object register success
 */
int mcf_link_socket_register(uint8_t port_id, const char *dev_name,void *user_data)
{
    struct mcf_link_tcp *link_tcp = RT_NULL;
    rt_err_t ret = RT_EOK;

    link_tcp = rt_calloc(1, sizeof(struct mcf_link_tcp));
    if (link_tcp == RT_NULL)
    {
        LOG_E("no memory for MCF link tcp object create.");
        return -MCF_MEM_FULL;
    }

    if (dev_name == RT_NULL)
    {
       LOG_E("MCF link tcp dev name is NULL.");
       return -MCF_FAILD;
    }

    /* set link tcp object information */
    link_tcp->user_data = user_data;
    link_tcp->link.ops = &tcp_link_ops;

    /* init tcp device thread */
    ret = rt_thread_init(&tcp_thread,
                        "udb_tcp",
                        tcp_thread_entry, link_tcp,
                        tcp_thread_stack, sizeof(tcp_thread_stack),
                        TCP_THREAD_PRIORITY, 20);
    ret = rt_thread_startup(&tcp_thread);
    RT_ASSERT(ret == RT_EOK);
    return mcf_link_register(&(link_tcp->link), port_id, MCF_LINK_TYPE_SOCKET, MCF_LINK_SOCKET_MTU, RT_NULL, RT_NULL);
}
