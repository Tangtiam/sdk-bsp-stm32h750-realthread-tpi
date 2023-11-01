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
#include <mcf_link.h>
#include <mcf_trans.h>

#define DBG_TAG               "mcf.link.uart"
#ifdef MCF_USING_DEBUG
#define DBG_LVL               DBG_LOG
#else
#define DBG_LVL               DBG_INFO
#endif /* MCF_DEBUG */
#include <rtdbg.h>

#define MCF_UART_FRAME_LEN_LEN         3
/* 6 bytes frame header, frame header(1byte) + frame len(2bytes) + frame ID (1byte)  + reserve(1byte) + frame type(1byte) */
#define MCF_UART_FRAME_HEAD_LEN        6
/* 2 bytes frame tail, CRC16(2bytes) + end sign(1byte) */
#define MCF_UART_FRAME_TAIL_LEN        3
/* D2D package header length */
#define MCF_D2D_PKT_HEAD_LEN           4
/* frame and packet total header */
#define MCF_UART_PKT_HEAD_LEN          (MCF_UART_FRAME_HEAD_LEN + MCF_UART_FRAME_TAIL_LEN + MCF_D2D_PKT_HEAD_LEN)

#define MCF_UART_FRAME_HEAD_SIGN       (0xFC)
#define MCF_UART_FRAME_HEAD_SIGN_LEN   (1)
#define MCF_UART_FRAME_END_SIGN        (0xCF)
#define MCF_UART_FRAME_END_SIGN_LEN    1
#ifdef MCF_PKT_MAX_SIZE
#define MCF_LINK_UART_MTU              (MCF_PKT_MAX_SIZE + MCF_UART_PKT_HEAD_LEN)
#else
#define MCF_LINK_UART_MTU              (1024 + MCF_UART_PKT_HEAD_LEN)
#endif
/* recv a frame timeout (unit: ms), default speed 1KB/S */
#define UART_RECV_FRAME_TIMEOUT        (rt_tick_from_millisecond(MCF_PKT_MAX_SIZE))
/* maximum length of a frame discard by the uart */
#define UART_DISCARD_MAX_SIZE          (64)

extern uint8_t g_frame_id;

static rt_mutex_t send_mutex = RT_NULL;
static rt_sem_t mcf_uart_dam_tx_sem = RT_NULL;
static rt_uint8_t mcf_uart_dma_tx_flag = 0;

enum mcf_uart_frame_index
{
    MCF_UART_FRAME_INDEX_HEAD = 0,
    MCF_UART_FRAME_INDEX_LEN1,
    MCF_UART_FRAME_INDEX_LEN2,
    MCF_UART_FRAME_INDEX_ID,
    MCF_UART_FRAME_INDEX_RESERVE,
    MCF_UART_FRAME_INDEX_TYPE,
    MCF_UART_FRAME_INDEX_CRC1,
    MCF_UART_FRAME_INDEX_CRC2,
};

/* MCF link uart frame */
struct mcf_uart_frame
{
    uint8_t head;                      /* frame header */
    uint16_t len;                      /* frame length */
    uint8_t id;                        /* frame ID */
    uint8_t reserve;                   /* reserve bytes */
    struct mcf_frame frame;            /* MCF common frame */
    uint16_t crc16;                    /* crc16 check value*/
    uint8_t end_sign;                  /* end sign */
};

struct mcf_link_uart
{
    struct mcf_link link;              /* link object */
    uint8_t need_crc16;                /* frame need crc16 check */
    struct rt_device *device;          /* device object index */
    struct rt_timer recv_timeout;      /* recv frame timeout timer */

    void *user_data;                   /* user-specific data */
};

/* allocate link uart frame object */
static struct mcf_uart_frame *link_uart_frame_alloc(struct mcf_pbuf *pbuf)
{
    struct mcf_uart_frame *uart_frame = RT_NULL;

    uart_frame = rt_calloc(1, sizeof(struct mcf_link_uart));
    if (uart_frame == RT_NULL)
    {
        return RT_NULL;
    }

    uart_frame->head = MCF_UART_FRAME_HEAD_SIGN;
    uart_frame->len = MCF_UART_FRAME_HEAD_LEN + MCF_UART_FRAME_TAIL_LEN + pbuf->len;
    uart_frame->id = mcf_link_frame_id_add();
    uart_frame->crc16 = 0;
    uart_frame->end_sign = MCF_UART_FRAME_END_SIGN;
    uart_frame->frame.type = 1;
    uart_frame->frame.payload = pbuf->buf;

    return uart_frame;
}

/* free link uart frame object */
static void mcf_uart_frame_free(struct mcf_uart_frame *link_uart)
{
    if (link_uart)
    {
        rt_free(link_uart);
    }
}

/* get raw packet buffer by link uart frame object */
static mcf_err_t link_uart_frame_to_pbuf(struct mcf_link_uart *link_uart, struct mcf_uart_frame *frame, struct mcf_pbuf *pbuf)
{
    uint8_t *buffer = RT_NULL;
    size_t payload_len;

    /* fill buffer data by frame information */
    buffer = pbuf->buf;
    buffer[MCF_UART_FRAME_INDEX_HEAD]    = frame->head;
    buffer[MCF_UART_FRAME_INDEX_LEN1]    = frame->len >> 8;
    buffer[MCF_UART_FRAME_INDEX_LEN2]    = frame->len & 0xFF;
    buffer[MCF_UART_FRAME_INDEX_ID]      = frame->id;
    buffer[MCF_UART_FRAME_INDEX_RESERVE] = frame->reserve;
    buffer[MCF_UART_FRAME_INDEX_TYPE]    = frame->frame.type;

    /* get frame buffer length and update packet buffer length */
    payload_len = frame->len - MCF_UART_FRAME_HEAD_LEN - MCF_UART_FRAME_TAIL_LEN;
    rt_memcpy(buffer + MCF_UART_FRAME_HEAD_LEN, frame->frame.payload, payload_len);

#ifdef MCF_LINK_UART_CRC
    if (link_uart->need_crc16)
    {
        uint16_t crc16;

        /* calculate frame buffer crc16 value */
        crc16 = mcf_crc16_calc(buffer + MCF_UART_FRAME_HEAD_SIGN_LEN,
                MCF_UART_FRAME_HEAD_LEN - MCF_UART_FRAME_HEAD_SIGN_LEN + payload_len);

        buffer[MCF_UART_FRAME_INDEX_CRC1 + payload_len] = crc16 >> 8;
        buffer[MCF_UART_FRAME_INDEX_CRC2 + payload_len] = crc16 & 0xFF;
    }
#endif /* MCF_LINK_UART_USING_CRC */

    /* splice link uart frame end sign */
    buffer[frame->len - MCF_UART_FRAME_END_SIGN_LEN] = MCF_UART_FRAME_END_SIGN;
    /* calculate packet buffer length */
    pbuf->len = MCF_UART_FRAME_HEAD_LEN + payload_len + MCF_UART_FRAME_TAIL_LEN;

    return MCF_OK;
}

/* get link uart object by device object */
static struct mcf_link_uart *link_uart_find(rt_device_t device)
{
    rt_slist_t *node = RT_NULL;
    struct mcf_link_uart *link_uart = RT_NULL;
    rt_slist_t link_list = mcf_link_list_get();

    rt_slist_for_each(node, &link_list)
    {
        link_uart = (struct mcf_link_uart *) rt_slist_entry(node, struct mcf_link, list);
        if (link_uart->device == device)
        {
            break;
        }
    }

    return link_uart;
}

/* check link uart frame data header */
static mcf_err_t frame_data_header_check(struct mcf_link_uart *link_uart)
{
    size_t recv_bufsz;
    uint8_t *recv_buffer = RT_NULL;

    recv_buffer = link_uart->link.recv_buf + link_uart->link.frames_len;
    recv_bufsz = link_uart->link.recv_bufsz - link_uart->link.frames_len;

    /* check frame header is normal */
    if (recv_buffer[0] != (uint8_t) MCF_UART_FRAME_HEAD_SIGN)
    {
        size_t index;
        size_t recv_buf_len = link_uart->link.cur_recv_bufsz;

        /* find link uart frame header in remaining receive buffer */
        for (index = 1; index < recv_buf_len; index++)
        {
            if (recv_buffer[index] == (uint8_t) MCF_UART_FRAME_HEAD_SIGN)
            {
                /* clean dirty uart frame data */
                rt_memmove(recv_buffer, recv_buffer + index, recv_bufsz - index);
                link_uart->link.cur_recv_bufsz -= index;
                break;
            }
        }
        /* not find MCF uart frame header sign */
        if (index == recv_buf_len)
        {
           /*clean dirty uart frame data */
            rt_memset(recv_buffer, 0x0, recv_bufsz);
            link_uart->link.cur_recv_bufsz = 0;
            return -MCF_FAILD;
        }
    }

    return MCF_OK;
}

/* check link uart frame data, return frame length */
static size_t frame_data_check(struct mcf_link_uart *link_uart)
{
    size_t recv_bufsz;
    uint8_t *recv_buffer = RT_NULL;
    size_t frame_len = 0;
    rt_uint32_t timer_state;

    recv_buffer = link_uart->link.recv_buf + link_uart->link.frames_len;
    recv_bufsz = link_uart->link.recv_bufsz - link_uart->link.frames_len;

    /* check receive buffer can get the received frame length */
    if (link_uart->link.cur_recv_bufsz < MCF_UART_FRAME_LEN_LEN)
    {
        goto _exit;
    }

    if (frame_data_header_check(link_uart) != MCF_OK)
    {
        LOG_E("link uart frame data header check failed.");
        goto _exit;
    }

    frame_len = (recv_buffer[1] << 8) + recv_buffer[2];

    /* check frame length */
    if (frame_len > MCF_LINK_UART_MTU || 0 == frame_len || recv_buffer[5] >= MCF_D2D_PKT_TYPE_MAX)
    {
        /* clean abnormal packet */
        rt_memset(recv_buffer, 0x0, recv_bufsz);
        link_uart->link.cur_recv_bufsz = 0;

        LOG_E("link uart frame data frame length check failed.");
        return 0;
    }

    /* recv frame start, start frame recv timeout timer */
    rt_timer_control(&link_uart->recv_timeout, RT_TIMER_CTRL_GET_STATE, &timer_state);
    if (timer_state == RT_TIMER_FLAG_DEACTIVATED)
    {
        rt_timer_start(&link_uart->recv_timeout);
    }

    /* if the length is not enough, continue to receive data */
    if (frame_len > link_uart->link.cur_recv_bufsz)
    {
        return 0;
    }

    /* recv finish, stop frame recv timeout timer */
    rt_timer_stop(&link_uart->recv_timeout);

    /* already get the full length link uart frame, check end sign */
    if (recv_buffer[frame_len - MCF_UART_FRAME_END_SIGN_LEN] != (uint8_t) MCF_UART_FRAME_END_SIGN)
    {
        // TODO find frame header, change frame header address pointer
        /* clean error uart frame data */
        rt_memmove(recv_buffer, recv_buffer + frame_len, recv_bufsz - frame_len);
        link_uart->link.cur_recv_bufsz -= frame_len;

        LOG_E("link uart frame data end sign check failed.");
        return 0;
    }

_exit:
    return frame_len;
}

#ifdef MCF_LINK_UART_CRC
/* check link uart frame crc16 value */
static mcf_err_t frame_crc16_check(struct mcf_link_uart *link_uart, size_t frame_len)
{
    uint16_t calc_crc16, frame_crc16;
    size_t recv_bufsz;
    uint8_t *recv_buffer = RT_NULL;

    recv_buffer = link_uart->link.recv_buf + link_uart->link.frames_len;
    recv_bufsz = link_uart->link.recv_bufsz - link_uart->link.frames_len;

    /* calculate link uart receive frame crc16 value  */
    calc_crc16 = mcf_crc16_calc(recv_buffer + MCF_UART_FRAME_HEAD_SIGN_LEN,
            frame_len - MCF_UART_FRAME_HEAD_SIGN_LEN - MCF_UART_FRAME_TAIL_LEN);

    /* get link uart frame crc16 value in receive buffer */
    frame_crc16 = (recv_buffer[frame_len - MCF_UART_FRAME_TAIL_LEN] << 8) | recv_buffer[frame_len - MCF_UART_FRAME_TAIL_LEN + 1];
    if (calc_crc16 != frame_crc16)
    {
        /* clean error uart frame data */
        rt_memmove(recv_buffer, recv_buffer + frame_len, recv_bufsz - frame_len);
        link_uart->link.cur_recv_bufsz -= frame_len;

        return -MCF_FAILD;
    }

    return MCF_OK;
}
#endif /* MCF_LINK_UART_USING_CRC */

/* link uart object receive data indicate */
static rt_err_t link_uart_rx_ind(rt_device_t device, rt_size_t size)
{
    rt_base_t level;
    size_t frame_len, recv_bufsz ;
    uint8_t *recv_buffer = RT_NULL;
    struct mcf_link_uart *link_uart = RT_NULL;
    uint8_t discard_frame[UART_DISCARD_MAX_SIZE] = { 0 };
    rt_size_t read_size = 0, left_size = 0, read_indx = 0;
    int read_real = 0, sem_cnt = 0;

    level = rt_hw_interrupt_disable();

    /* get link uart object by device */
    link_uart = link_uart_find(device);
    if (link_uart == RT_NULL)
    {
        rt_hw_interrupt_enable(level);
        LOG_E("find link uart object by device(%s) failed.", device->parent.name);
        return -RT_ERROR;
    }

    recv_buffer = link_uart->link.recv_buf + link_uart->link.frames_len;
    recv_bufsz  = link_uart->link.recv_bufsz - link_uart->link.frames_len;

    /* check link uart receive buffer is full */
    if ((link_uart->link.cur_recv_bufsz + size) > recv_bufsz)
    {
        read_indx = 0;
        left_size = size;
        while(left_size)
        {
            read_size = (left_size > UART_DISCARD_MAX_SIZE) ? UART_DISCARD_MAX_SIZE : left_size;
            read_real = rt_device_read(device, read_indx, discard_frame, read_size);
            if(read_real == read_size)
            {
                left_size = left_size - read_size;
                read_indx = read_indx + read_size;
            }
            else
            {
                break;
            }
        }
        rt_hw_interrupt_enable(level);
        LOG_E("link uart receive buffer size(%d) is full.", link_uart->link.recv_bufsz);
        return -RT_ERROR;
    }

    /* receive data of specified length and update receive buffer length */
    rt_device_read(device, 0, recv_buffer + link_uart->link.cur_recv_bufsz, size);
    link_uart->link.cur_recv_bufsz += size;

    while ((frame_len = frame_data_check(link_uart)) != 0)
    {
#ifdef MCF_LINK_UART_CRC
        /* check receive frame crc16 value */
        if (link_uart->need_crc16 && frame_crc16_check(link_uart, frame_len) != MCF_OK)
        {
            rt_hw_interrupt_enable(level);
            LOG_E("link uart frame check crc16 number error.");
            return -RT_ERROR;
        }
#endif /* MCF_LINK_UART_USING_CRC16 */

        /* link uart frame check success, send receive event notice */
        link_uart->link.frames_len += frame_len;
        link_uart->link.cur_recv_bufsz -= frame_len;

        sem_cnt += 1;

        rt_hw_interrupt_enable(level);
        LOG_D("recv a frame, len %d", frame_len);
        level = rt_hw_interrupt_disable();
    }

    rt_hw_interrupt_enable(level);

    while (sem_cnt > 0)
    {
        /* send frame data received notice */
        mcf_link_notice_send(&(link_uart->link));
        sem_cnt -= 1;
    }

    return RT_EOK;
}

/* link uart object initialize */
static mcf_err_t mcf_link_uart_init(struct mcf_link *link)
{
    struct mcf_link_uart *link_uart = (struct mcf_link_uart *)link;

    if (link_uart->device == RT_NULL)
    {
        return MCF_FAILD;
    }
    rt_device_set_rx_indicate(link_uart->device, link_uart_rx_ind);
    return MCF_OK;
}

/* uart send complete */
static rt_err_t mcf_link_uart_send_back(rt_device_t dev, void *buffer)
{

    rt_sem_release(mcf_uart_dam_tx_sem);

    return RT_EOK;
}

/* send data mode sync */
static rt_size_t uart_sync_write(rt_device_t dev, rt_off_t pos, const void *buffer, rt_size_t size, int32_t timeout)
{
    rt_size_t result;

    result = rt_device_write(dev, pos, buffer, size);
    if ((result != size) || (rt_sem_take(mcf_uart_dam_tx_sem, timeout) != RT_EOK))
    {
        return 0;
    }

    return result;
}

/* send data by link uart object */
static mcf_err_t mcf_link_uart_send(struct mcf_link *link, struct mcf_pbuf *pbuf, int32_t timeout)
{
    struct mcf_uart_frame *uart_frame = RT_NULL;
    struct mcf_link_uart *link_uart = (struct mcf_link_uart *)link;
    struct mcf_pbuf *out_pbuf = RT_NULL;
    mcf_err_t result;
    size_t write_len;

    RT_ASSERT(link);
    RT_ASSERT(pbuf && pbuf->buf && (pbuf->size > 0));

#if 0
    rt_mutex_take(send_mutex, RT_WAITING_FOREVER);
#else
    rt_enter_critical();
#endif
    /* allocate link uart frame */
    uart_frame = link_uart_frame_alloc(pbuf);
    if (uart_frame == RT_NULL)
    {
        result = -MCF_MEM_FULL;
        LOG_E("uart alloc frame memory failed.");
        goto __exit;
    }

    /* allocate packet buffer for  */
    out_pbuf = mcf_pbuf_alloc(MCF_LINK_UART_MTU);
    if (out_pbuf == RT_NULL)
    {
        result = -MCF_MEM_FULL;
        LOG_E("uart alloc packet memory failed.");
        goto __exit;
    }

    /* MCF link uart frame to packet buffer */
    result = link_uart_frame_to_pbuf(link_uart, uart_frame, out_pbuf);
    if (result != MCF_OK)
    {
        LOG_E("uart pack frame failed.");
        goto __exit;
    }
    /* send packet buffer data by uart device */
    if (mcf_uart_dma_tx_flag)
    {
        write_len = uart_sync_write(link_uart->device, 0, out_pbuf->buf, out_pbuf->len, timeout);
    }
    else
    {
        write_len = rt_device_write(link_uart->device, 0, out_pbuf->buf, out_pbuf->len);
    }

    if (write_len != out_pbuf->len)
    {
        result = -MCF_FAILD;
        LOG_E("link uart send frame (%d!=%d) failed.", write_len, out_pbuf->len);
        goto __exit;
    }

    LOG_D("send a frame, len %d", write_len);

    mcf_print_raw_data((uint8_t *)(out_pbuf->buf) + MCF_UART_FRAME_HEAD_LEN,
            out_pbuf->len - MCF_UART_FRAME_HEAD_LEN - MCF_UART_FRAME_TAIL_LEN);

__exit:

#if 0
    rt_mutex_release(send_mutex);
#else
    rt_exit_critical();
#endif

    /* free link uart frame and packet buffer */
    mcf_uart_frame_free(uart_frame);
    mcf_pbuf_free(out_pbuf);

    return result;
}

/* receive data by link uart object */
static mcf_err_t mcf_link_uart_recv(struct mcf_link *link, struct mcf_pbuf *pbuf, int32_t timeout)
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
    pbuf->len = frame_len - MCF_UART_FRAME_HEAD_LEN - MCF_UART_FRAME_TAIL_LEN;
    rt_memcpy(pbuf->buf, link->recv_buf + MCF_UART_FRAME_HEAD_LEN, pbuf->len);

    /* update link receive buffer */
    rt_memmove(link->recv_buf, link->recv_buf + frame_len, link->recv_bufsz - frame_len);
    RT_ASSERT(link->frames_len >= frame_len);
    link->frames_len -= frame_len;

    rt_hw_interrupt_enable(level);

    mcf_print_raw_data(pbuf->buf, pbuf->len);

    return MCF_OK;
}

/* link uart object control feature */
static mcf_err_t mcf_link_uart_control(struct mcf_link *link, int cmd, void *arg)
{
    struct mcf_link_uart *link_uart = (struct mcf_link_uart *)link;

    if (cmd == MCF_LINK_CTRL_NEED_CRC16)
    {
        link_uart->need_crc16 = *(int *)arg;
    }
    else
    {
        rt_device_control(link_uart->device, cmd, arg);
    }

    return MCF_OK;
}

static void recv_timeout_entry(void *para)
{
    struct mcf_link_uart *link_uart = para;
    size_t recv_bufsz;
    uint8_t *recv_buffer = RT_NULL;
    rt_base_t level;

    level = rt_hw_interrupt_disable();

    recv_buffer = link_uart->link.recv_buf + link_uart->link.frames_len;
    recv_bufsz = link_uart->link.recv_bufsz - link_uart->link.frames_len;
    /* clean abnormal packet */
    rt_memset(recv_buffer, 0x0, recv_bufsz);
    link_uart->link.cur_recv_bufsz = 0;

    rt_hw_interrupt_enable(level);

    LOG_W("link uart recv a frame data timeout(%dms)", UART_RECV_FRAME_TIMEOUT);
}

/* link uart object operation */
static const struct mcf_link_ops uart_link_ops =
{
    .init    = mcf_link_uart_init,
    .send    = mcf_link_uart_send,
    .recv    = mcf_link_uart_recv,
    .control = mcf_link_uart_control,
};

/**
 * MCF link uart object register.
 *
 * @param port_id link port ID
 * @param dev_name uart device name
 * @param uaer_data reserved user data
 *
 * @return <0: MCF link uart object register failed
 *         =0: MCF link uart object register success
 */
int mcf_link_uart_register(uint8_t port_id, const char *dev_name, void *user_data)
{
    struct mcf_link_uart *link_uart = RT_NULL;
    
    RT_ASSERT(dev_name);

    link_uart = rt_calloc(1, sizeof(struct mcf_link_uart));
    if (link_uart == RT_NULL)
    {
        LOG_E("no memory for MCF link uart object create.");
        return -MCF_MEM_FULL;
    }

    /* find and open uart device */
    {
        link_uart->device = rt_device_find(dev_name);

        if (link_uart->device)
        {
            rt_err_t open_result = RT_EOK;
            rt_uint16_t oflag;

            RT_ASSERT(link_uart->device->type == RT_Device_Class_Char);

            /* TX using DMA mode */
            open_result = rt_device_open(link_uart->device, RT_DEVICE_OFLAG_RDWR | RT_DEVICE_FLAG_DMA_TX);
            if (open_result == RT_EOK)
            {
                mcf_uart_dma_tx_flag = 1;
                mcf_uart_dam_tx_sem  = rt_sem_create("mcf_tx", 0, RT_IPC_FLAG_FIFO);
                rt_device_set_tx_complete(link_uart->device, mcf_link_uart_send_back);
            }

            oflag = link_uart->device->open_flag | RT_DEVICE_FLAG_DMA_RX;

            /* RX using DMA mode */
            open_result = rt_device_open(link_uart->device, RT_DEVICE_OFLAG_RDWR | oflag);

            /* RX using interrupt mode when DMA mode not supported */
            if (open_result == -RT_EIO)
            {
                if (mcf_uart_dma_tx_flag)
                {
                    open_result = rt_device_open(link_uart->device, RT_DEVICE_OFLAG_RDWR | RT_DEVICE_FLAG_INT_RX | RT_DEVICE_FLAG_DMA_TX);
                }
                else
                {
                    open_result = rt_device_open(link_uart->device, RT_DEVICE_OFLAG_RDWR | RT_DEVICE_FLAG_INT_RX);
                }
            }
            RT_ASSERT(open_result == RT_EOK);
        }
        else
        {
            rt_free(link_uart);
            return -MCF_FAILD;
        }
    }

    send_mutex = rt_mutex_create("smutex", RT_IPC_FLAG_FIFO);
    if (send_mutex == RT_NULL)
    {
        LOG_E("create dynamic mutex failed.");
    }

    rt_timer_init(&link_uart->recv_timeout, "mcf_recv", recv_timeout_entry, link_uart, UART_RECV_FRAME_TIMEOUT,
            RT_TIMER_FLAG_ONE_SHOT | RT_TIMER_FLAG_SOFT_TIMER);

    /* set link uart object information */
    link_uart->need_crc16 = 1;
    link_uart->link.ops = &uart_link_ops;
    link_uart->user_data = user_data;

    return mcf_link_register(&(link_uart->link), port_id, MCF_LINK_TYPE_UART, MCF_LINK_UART_MTU, link_uart->device, RT_NULL);
}
