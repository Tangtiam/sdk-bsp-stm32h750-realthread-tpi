/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2021-02-02     liukang     first version
 */

#include <mcf.h>
#include <mcf_link.h>
#include <mcf_trans.h>

#define DBG_TAG               "mcf.link.usb"
#ifdef MCF_USING_DEBUG
#define DBG_LVL               DBG_LOG
#else
#define DBG_LVL               DBG_INFO
#endif /* MCF_DEBUG */
#include <rtdbg.h>

#define MCF_USB_FRAME_LEN_LEN         3
/* 6 bytes frame header, frame header(1byte) + frame len(2bytes) + frame ID (1byte)  + reserve(1byte) + frame type(1byte) */
#define MCF_USB_FRAME_HEAD_LEN        6
/* end sign(1byte) */
#define MCF_USB_FRAME_TAIL_LEN        1
/* D2D package header length */
#define MCF_D2D_PKT_HEAD_LEN          4
 /* frame and packet total header */
#define MCF_USB_PKT_HEAD_LEN          (MCF_USB_FRAME_HEAD_LEN + MCF_USB_FRAME_TAIL_LEN + MCF_D2D_PKT_HEAD_LEN)

#define MCF_USB_FRAME_HEAD_SIGN       (0xFC)
#define MCF_USB_FRAME_END_SIGN        (0xCF)
#define MCF_USB_FRAME_END_SIGN_LEN    (1)
#ifdef MCF_PKT_MAX_SIZE
#define MCF_LINK_USB_MTU              (MCF_PKT_MAX_SIZE + MCF_USB_PKT_HEAD_LEN)
#else
#define MCF_LINK_USB_MTU              (1024 + MCF_USB_PKT_HEAD_LEN)
#endif

enum mcf_usb_frame_index
{
    MCF_USB_FRAME_INDEX_HEAD = 0,
    MCF_USB_FRAME_INDEX_LEN1,
    MCF_USB_FRAME_INDEX_LEN2,
    MCF_USB_FRAME_INDEX_ID,
    MCF_USB_FRAME_INDEX_RESERVE,
    MCF_USB_FRAME_INDEX_TYPE,
};

struct usb_cmd_message
{
   rt_uint32_t message_type;
   rt_uint32_t length;
};
typedef struct usb_cmd_message *usb_cmd_msg_t;

struct mcf_usb_rbuf
{
    rt_uint8_t recv_buf[MCF_PKT_MAX_SIZE];
    rt_uint32_t frames_len;
    rt_uint32_t recv_bufsz;
    rt_uint32_t cur_recv_bufsz;
    rt_uint8_t mark;
};
static rt_mutex_t send_mutex = RT_NULL;
static struct mcf_usb_rbuf usb_rbuf1 = {0};
static struct mcf_usb_rbuf usb_rbuf2 = {0};

/* MCF link usb frame */
struct mcf_usb_frame
{
    uint8_t head;                      /* frame header */
    uint16_t len;                      /* frame length */
    uint8_t id;                        /* frame ID */
    uint8_t reserve;                   /* reserve bytes */
    struct mcf_frame frame;            /* MCF common frame */
    uint8_t end_sign;                  /* end sign */
};

struct mcf_link_usb
{
   struct mcf_link link;              /* link object */
   struct rt_device *device;          /* device object index */

   void *user_data;                   /* user-specific data */
};

static struct rt_completion rx_completion, tx_completion;
static struct rt_messagequeue usb_mq;
static rt_uint8_t usb_mq_pool[(sizeof(struct usb_cmd_message) + sizeof(void *)) * 512];

static struct rt_thread usb_thread;
static char usb_thread_stack[4096];
#define usb_thread_priority     12

/* check link usb frame data header */
static mcf_err_t frame_data_header_check(struct mcf_usb_rbuf *usb_rbuf)
{
    size_t recv_buf_sz;
    uint8_t *recv_buffer = RT_NULL;

    recv_buffer = usb_rbuf->recv_buf + usb_rbuf->frames_len;
    recv_buf_sz = usb_rbuf->recv_bufsz - usb_rbuf->frames_len;

    /* check frame header is normal */
    if (recv_buffer[0] != MCF_USB_FRAME_HEAD_SIGN)
    {
        int index;
        size_t recv_buf_len = usb_rbuf->cur_recv_bufsz;

        /* find link usb frame header in remaining receive buffer */
        for (index = 1; index < recv_buf_len; index++)
        {
            if (recv_buffer[index] == MCF_USB_FRAME_HEAD_SIGN)
            {
                /* clean dirty usb frame data */
                rt_memmove(recv_buffer, recv_buffer + index, recv_buf_sz - index);
                usb_rbuf->cur_recv_bufsz -= index;
                break;
            }
        }
        /* not find MCF usb frame header sign */
        if (index == recv_buf_len)
        {
           /*clean dirty usb frame data */
            rt_memset(recv_buffer, 0x0, recv_buf_sz);
            usb_rbuf->cur_recv_bufsz = 0;
            return -MCF_FAILD;
        }
    }

    return MCF_OK;
}

/* check link usb frame data, return frame length */
static size_t frame_data_check(struct mcf_usb_rbuf *usb_rbuf)
{
    size_t recv_buf_sz;
    uint8_t *recv_buffer = RT_NULL;
    size_t frame_len = 0;
    
    recv_buffer = usb_rbuf->recv_buf + usb_rbuf->frames_len;
    recv_buf_sz = usb_rbuf->recv_bufsz - usb_rbuf->frames_len;

    /* check receive buffer can get the received frame length */
    if (usb_rbuf->cur_recv_bufsz < MCF_USB_FRAME_LEN_LEN)
    {
        goto _exit;
    }

    if (frame_data_header_check(usb_rbuf) != MCF_OK)
    {
        LOG_E("link usb frame data header check failed.");
        goto _exit;
    }

    frame_len = (recv_buffer[1] << 8) + recv_buffer[2];
    /* check frame length */
    if (frame_len > MCF_LINK_USB_MTU || 0 == frame_len || recv_buffer[5] >= MCF_D2D_PKT_TYPE_MAX)
    {
        /* clean abnormal packet */
        rt_memset(recv_buffer, 0x0, recv_buf_sz);
        usb_rbuf->cur_recv_bufsz = 0;

        LOG_E("link usb frame data frame length check failed.");
        return 0;
    }

    /* if the length is not enough, continue to receive data */
    if (frame_len > usb_rbuf->cur_recv_bufsz)
    {
        return 0;
    }

    /* already get the full length link usb frame, check end sign */
    if (recv_buffer[frame_len - MCF_USB_FRAME_END_SIGN_LEN] != MCF_USB_FRAME_END_SIGN)
    {
        // TODO find frame header, change frame header address pointer
        /* clean error usb frame data */
        rt_memmove(recv_buffer, recv_buffer + frame_len, recv_buf_sz - frame_len);
        usb_rbuf->cur_recv_bufsz -= frame_len;

        LOG_E("link usb frame data end sign check failed. frame_len: %d, end sign: 0x%x", frame_len, recv_buffer[frame_len - MCF_USB_FRAME_END_SIGN_LEN]);
        return 0;
    }

_exit:
    return frame_len;
}

/* allocate link usb frame object */
static struct mcf_usb_frame *link_usb_frame_alloc(struct mcf_pbuf *pbuf)
{
    struct mcf_usb_frame *usb_frame = RT_NULL;

    usb_frame = rt_calloc(1, sizeof(struct mcf_usb_frame));
    if (usb_frame == RT_NULL)
    {
        return RT_NULL;
    }

    usb_frame->head = MCF_USB_FRAME_HEAD_SIGN;
    usb_frame->len = MCF_USB_FRAME_HEAD_LEN + MCF_USB_FRAME_TAIL_LEN + pbuf->len;
    usb_frame->id = mcf_link_frame_id_add();
    usb_frame->end_sign = MCF_USB_FRAME_END_SIGN;
    usb_frame->frame.type = 1;
    usb_frame->frame.payload = pbuf->buf;

    return usb_frame;
}

/* free link usb frame object */
static void mcf_usb_frame_free(struct mcf_usb_frame *link_usb)
{
    if (link_usb)
    {
        rt_free(link_usb);
    }
}

/* get raw packet buffer by link usb frame object */
static mcf_err_t link_usb_frame_to_pbuf(struct mcf_link_usb *link_usb, struct mcf_usb_frame *frame, struct mcf_pbuf *pbuf)
{
    uint8_t *buffer = RT_NULL;
    size_t payload_len;

    /* fill buffer data by frame information */
    buffer = pbuf->buf;
    buffer[MCF_USB_FRAME_INDEX_HEAD]    = frame->head;
    buffer[MCF_USB_FRAME_INDEX_LEN1]    = frame->len >> 8;
    buffer[MCF_USB_FRAME_INDEX_LEN2]    = frame->len & 0xFF;
    buffer[MCF_USB_FRAME_INDEX_ID]      = frame->id;
    buffer[MCF_USB_FRAME_INDEX_RESERVE] = frame->reserve;
    buffer[MCF_USB_FRAME_INDEX_TYPE]    = frame->frame.type;

    /* get frame buffer length and update packet buffer length */
    payload_len = frame->len - MCF_USB_FRAME_HEAD_LEN - MCF_USB_FRAME_TAIL_LEN;
    rt_memcpy(buffer + MCF_USB_FRAME_HEAD_LEN, frame->frame.payload, payload_len);

    /* splice link usb frame end sign */
    buffer[frame->len - MCF_USB_FRAME_END_SIGN_LEN] = MCF_USB_FRAME_END_SIGN;
    /* calculate packet buffer length */
    pbuf->len = MCF_USB_FRAME_HEAD_LEN + payload_len + MCF_USB_FRAME_TAIL_LEN;

    return MCF_OK;
}

/* send data by link usb object */
static mcf_err_t mcf_link_usb_send(struct mcf_link *link, struct mcf_pbuf *pbuf, int32_t timeout)
{
    struct mcf_usb_frame *usb_frame = RT_NULL;
    struct mcf_link_usb *link_usb = (struct mcf_link_usb *)link;
    struct mcf_pbuf *out_pbuf = RT_NULL;
    mcf_err_t result;
    size_t write_len;

    RT_ASSERT(link);
    RT_ASSERT(pbuf && pbuf->buf && (pbuf->size > 0));
    
    rt_mutex_take(send_mutex, RT_WAITING_FOREVER);
    /* allocate link usb frame */
    usb_frame = link_usb_frame_alloc(pbuf);  
    if (usb_frame == RT_NULL)
    {
        LOG_E("MCF allocate link usb frame failed.");
        result = -MCF_MEM_FULL;
        goto _exit;
    }

    /* allocate packet buffer for  */
    out_pbuf = mcf_pbuf_alloc(MCF_LINK_USB_MTU);
    if (out_pbuf == RT_NULL)
    {
        LOG_E("MCF allocate packet buffer failed.");
        result = -MCF_MEM_FULL;
        goto _exit;
    }

    /* MCF link usb frame to packet buffer */
    result = link_usb_frame_to_pbuf(link_usb, usb_frame, out_pbuf);
    if (result != MCF_OK)
    {
        LOG_E("MCF link usb frame to packet buffer failed.");
        goto _exit;
    }

    rt_completion_init(&tx_completion);
    write_len = rt_device_write(link_usb->device, 0, out_pbuf->buf, out_pbuf->len);
    result = rt_completion_wait(&tx_completion, timeout);

    if (write_len != out_pbuf->len)
    {
        LOG_E("MCF usb device send frame (write_len:%d != len:%d) failed.", write_len, out_pbuf->len);
        result = -MCF_FAILD;
        goto _exit;
    }
    else if (result != RT_EOK)
    {
        LOG_E("MCF usb device send frame (len:%d) timeout.", out_pbuf->len);
        result = -MCF_SEND_TIMEOUT;
        goto _exit;
    }

    mcf_print_raw_data((const uint8_t *)(out_pbuf->buf) + MCF_USB_FRAME_HEAD_LEN, out_pbuf->len - MCF_USB_FRAME_HEAD_LEN - MCF_USB_FRAME_TAIL_LEN);

    result = MCF_OK;

_exit:
    /* free link usb frame and packet buffer */
    mcf_usb_frame_free(usb_frame);
    mcf_pbuf_free(out_pbuf);

    rt_mutex_release(send_mutex);
    
    return result;
}

/* receive data by link usb object */
static mcf_err_t mcf_link_usb_recv(struct mcf_link *link, struct mcf_pbuf *pbuf, int32_t timeout)
{
    size_t frame_len;
    rt_base_t level;
    struct mcf_usb_rbuf *usb_rbuf = RT_NULL;
    static int read_index = 2;

    RT_ASSERT(link && link->recv_buf);
    RT_ASSERT(pbuf && pbuf->buf);

    level = rt_hw_interrupt_disable();
    
    /* double buffer, select the free buffer */
    if (read_index == 1)
    {
        RT_ASSERT(usb_rbuf2.mark == 1);

        usb_rbuf = &usb_rbuf2;
        read_index = 2;
    }
    else if (read_index == 2)
    {
        RT_ASSERT(usb_rbuf1.mark == 1);

        usb_rbuf = &usb_rbuf1;
        read_index = 1;
    }

    if (usb_rbuf->frames_len == 0)
    {
        rt_hw_interrupt_enable(level);
        usb_rbuf->mark = 0;
        LOG_E("frames len is 0");
        return -MCF_FAILD;
    }
              
    /* get receive frame length */
    frame_len = (usb_rbuf->recv_buf[1] << 8) | usb_rbuf->recv_buf[2];
    RT_ASSERT(pbuf->size >= frame_len);

    /* fill receive data to packet buffer */
    pbuf->len = frame_len - MCF_USB_FRAME_HEAD_LEN - MCF_USB_FRAME_TAIL_LEN;
    rt_memcpy(pbuf->buf, usb_rbuf->recv_buf + MCF_USB_FRAME_HEAD_LEN, pbuf->len);

    /* update link receive buffer */
    rt_memmove(usb_rbuf->recv_buf, usb_rbuf->recv_buf + frame_len, usb_rbuf->recv_bufsz - frame_len);
    RT_ASSERT(usb_rbuf->frames_len >= frame_len);
    usb_rbuf->frames_len -= frame_len;
    usb_rbuf->mark = 0;
    rt_hw_interrupt_enable(level);

    mcf_print_raw_data(pbuf->buf, pbuf->len);

    return MCF_OK;
}

/* link usb object control feature */
static mcf_err_t mcf_link_usb_control(struct mcf_link *link, int cmd, void *arg)
{
   struct mcf_link_usb *link_usb = (struct mcf_link_usb *)link;

   rt_device_control(link_usb->device, cmd, arg);

   return MCF_OK;
}

static void cmd_handler(rt_uint8_t *buffer, rt_size_t size)
{
    buffer[size] = '\0';
    usb_cmd_msg_t msg = (usb_cmd_msg_t) buffer;
    rt_mq_send(&usb_mq, (void *)msg, sizeof(struct usb_cmd_message));
}

static rt_err_t rx_indicate(rt_device_t dev, rt_size_t size)
{
    rt_completion_done(&rx_completion);
    return RT_EOK;
}

static rt_err_t tx_complete(rt_device_t dev, void *buffer)
{
    rt_completion_done(&tx_completion);
    return RT_EOK;
}

/* link usb object initialize */
static mcf_err_t mcf_link_usb_init(struct mcf_link *link)
{
    struct mcf_link_usb *link_usb = (struct mcf_link_usb *)link;

    rt_device_set_rx_indicate(link_usb->device, rx_indicate);
    rt_device_set_tx_complete(link_usb->device, tx_complete);

    return MCF_OK;
}

/* link usb object operation */
static const struct mcf_link_ops usb_link_ops =
{
    .init    = mcf_link_usb_init,
    .send    = mcf_link_usb_send,
    .recv    = mcf_link_usb_recv,
    .control = mcf_link_usb_control,
};

/* usb thread entry */
static void usb_thread_entry(void *parameter)
{
    struct usb_cmd_message msg;
    uint8_t *recv_buffer = RT_NULL;
    struct mcf_link_usb *link_usb = RT_NULL;
    rt_size_t transfer_size, remain_size, recv_bufsz, frame_len;
    struct mcf_usb_rbuf *usb_rbuf = RT_NULL;
    static int write_index = 2;

    link_usb = (struct mcf_link_usb *) parameter;
    while (1)
    {
        if (rt_mq_recv(&usb_mq, &msg, sizeof(struct usb_cmd_message), RT_WAITING_FOREVER) != RT_EOK)
        {
            continue;
        }
_select_buffer:
        /* get this frame length */
        remain_size = msg.length;

        /* double buffer, select the free buffer */
        if (usb_rbuf1.mark == 0 || usb_rbuf2.mark == 0)
        {
            if (write_index == 1)
            {
                if (usb_rbuf2.mark == 0)
                {
                    usb_rbuf = &usb_rbuf2;
                    write_index = 2;
                }
                else
                {
                    LOG_D("no buffer");
                    goto _select_buffer;
                }
            }
            else
            {
                if (usb_rbuf1.mark == 0)
                {
                    usb_rbuf = &usb_rbuf1;
                    write_index = 1;
                }
                else
                {
                    LOG_D("no buffer");
                    goto _select_buffer;
                }
            }

            recv_buffer = usb_rbuf->recv_buf + usb_rbuf->frames_len;
            recv_bufsz  = usb_rbuf->recv_bufsz - usb_rbuf->frames_len;
            
            /* check link usb receive buffer is full */
            if ((usb_rbuf->cur_recv_bufsz + remain_size) > recv_bufsz)
            {
                LOG_E("link usb receive buffer size(%d) is full.", link_usb->link.recv_bufsz);
                break;
            }

            rt_completion_init(&rx_completion);
            transfer_size = rt_device_read(link_usb->device, 0, recv_buffer + usb_rbuf->cur_recv_bufsz, remain_size);
            rt_completion_wait(&rx_completion, RT_WAITING_FOREVER);

            if (transfer_size != remain_size)
            {
                LOG_W("usb read length: %d not equal msg length: %d", transfer_size, remain_size);
                break;
            }
            usb_rbuf->cur_recv_bufsz += transfer_size;

            while ((frame_len = frame_data_check(usb_rbuf)) != 0)
            {
                /* link usb frame check success, send receive event notice */
                usb_rbuf->frames_len += frame_len;
                usb_rbuf->cur_recv_bufsz -= frame_len;
                /* mark the buffer being used */
                usb_rbuf->mark = 1;
                /* send frame data received notice */
                mcf_link_notice_send(&(link_usb->link));

                break;
            }
        }
        else
        {
            LOG_D("no buffer");
            rt_thread_mdelay(1);
            goto _select_buffer;
        }
    }
}

/**
* MCF link usb object register.
*
* @param port_id link port ID
* @param dev_name usb device name
* @param uaer_data reserved user data
*
* @return <0: MCF link usb object register failed
*         =0: MCF link usb object register success
*/
int mcf_link_usb_register(uint8_t port_id, const char *dev_name, void *user_data)
{
    struct mcf_link_usb *link_usb = RT_NULL;
    rt_err_t result = RT_EOK;

    usb_rbuf1.recv_bufsz = usb_rbuf2.recv_bufsz = MCF_PKT_MAX_SIZE;

    link_usb = rt_calloc(1, sizeof(struct mcf_link_usb));
    if (link_usb == RT_NULL)
    {
       LOG_E("no memory for MCF link usb object create.");
       return -MCF_MEM_FULL;
    }
    if (dev_name == RT_NULL)
    {
       LOG_E("MCF link usb dev name is NULL.");
       return -MCF_FAILD;
    }

    link_usb->device = rt_device_find(dev_name);
    if (link_usb->device == RT_NULL)
    {
       LOG_E("The MCF link usb device could not be found.");
       return -MCF_FAILD;
    }

    /* open link usb device */
    rt_device_control(link_usb->device, RT_DEVICE_CTRL_CONFIG, (void *)cmd_handler);
    result = rt_device_open(link_usb->device, RT_DEVICE_OFLAG_RDWR);
    RT_ASSERT(result == RT_EOK);

    /* set link usb object information */
    link_usb->link.ops = &usb_link_ops;
    link_usb->user_data = user_data;

    send_mutex = rt_mutex_create("winusb", RT_IPC_FLAG_FIFO);
    if (send_mutex == RT_NULL)
    {
        LOG_E("create dynamic mutex failed.\n");
    }
    
    /* create an usb message queue */
    rt_mq_init(&usb_mq,
              "winusb",
              usb_mq_pool, sizeof(struct usb_cmd_message),
              sizeof(usb_mq_pool),
              RT_IPC_FLAG_FIFO);

    /* init usb device thread */
    rt_thread_init(&usb_thread,
                  "winusb",
                  usb_thread_entry, link_usb,
                  usb_thread_stack, sizeof(usb_thread_stack),
                  usb_thread_priority, 20);
    rt_thread_startup(&usb_thread);

    return mcf_link_register(&(link_usb->link), port_id, MCF_LINK_TYPE_USB, MCF_LINK_USB_MTU, link_usb->device, RT_NULL);
}
