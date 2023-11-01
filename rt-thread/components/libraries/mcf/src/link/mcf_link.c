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

#define DBG_TAG               "mcf.link"
#ifdef MCF_USING_DEBUG
#define DBG_LVL               DBG_LOG
#else
#define DBG_LVL               DBG_INFO
#endif /* MCF_DEBUG */
#include <rtdbg.h>

//TODO ��ʱ���룬����ɾ��
#define MCF_ARP_TIMEOUT          RT_WAITING_FOREVER
/* arp maximum unused time */
#ifndef MCF_ARP_TIMEOUT
#define MCF_ARP_TIMEOUT          (5 * 60 * RT_TICK_PER_SECOND)
#endif

/* the global link receive data semaphore */
static rt_sem_t g_link_sem = RT_NULL;
/* the global link ARP object list */
static rt_slist_t g_arp_list = RT_SLIST_OBJECT_INIT(&g_arp_list);
/* the global link object list */
static rt_slist_t g_link_list = RT_SLIST_OBJECT_INIT(&g_link_list);
static uint8_t g_frame_id = 1;
static rt_timer_t g_link_timer = RT_NULL;

rt_slist_t mcf_link_list_get(void)
{
    return g_link_list;
}

rt_slist_t mcf_link_arp_list_get(void)
{
    return g_arp_list;
}

uint8_t mcf_link_frame_id_add(void)
{
    return g_frame_id++;
}

/* get list of received data MCF link object */
mcf_err_t mcf_link_notice_recv(struct mcf_link **link, int32_t timeout)
{
    rt_base_t level;
    rt_slist_t *node = RT_NULL;
    struct mcf_link *recv_link = RT_NULL;

    /* wait receive data semaphore */
    if (rt_sem_take(g_link_sem, timeout) != RT_EOK)
    {
        LOG_E("mcf link take sem failed");
        return -MCF_RECV_TIMEOUT;
    }

    level = rt_hw_interrupt_disable();
    rt_slist_for_each(node, &g_link_list)
    {
        recv_link = rt_slist_entry(node, struct mcf_link, list);
        /* check link object status */
        if (recv_link->status & MCF_LINK_STATUS_RECV)
        {
            /* clean link object receive status */
            if ((--recv_link->notice_num) == 0)
            {
                recv_link->status &= ~MCF_LINK_STATUS_RECV;
            }
            /* return recv data mcf link object  */
            *link = recv_link;

            break;
        }
    }
    rt_hw_interrupt_enable(level);

    return MCF_OK;
}

/* send data reception notice */
mcf_err_t mcf_link_notice_send(struct mcf_link *link)
{
    rt_base_t level;

    level = rt_hw_interrupt_disable();
    /* set link object receive status */
    link->notice_num++;
    link->status |= MCF_LINK_STATUS_RECV;
    rt_hw_interrupt_enable(level);

    /* send receive data semaphore */
    if (rt_sem_release(g_link_sem) != RT_EOK)
    {
        LOG_E("mcf link release sem failed");
        return -MCF_SEND_TIMEOUT;
    }

    return MCF_OK;
}

/**
 * update MCF link ARP object.
 *
 * @param port_id the current device port ID
 * @param dev_id connected device ID
 *
 * @note This function is used to specify the MCF link port and the connected device ID,
 *       which is used to update arp tick, avoid overtime and deletes.
 * 
 * @return update MCF link ARP list success
 */
static int mcf_link_arp_update(uint8_t port_id, uint8_t dev_id)
{
    struct mcf_arp *arp = RT_NULL;
    rt_slist_t *node = RT_NULL;
    rt_base_t level;

    level = rt_hw_interrupt_disable();
    rt_slist_for_each(node, &g_arp_list)
    {
        arp = rt_slist_entry(node, struct mcf_arp, list);
        if ((arp->dev_id == dev_id) && (arp->port_id == port_id))
        {
            arp->tick = rt_tick_get();
            rt_hw_interrupt_enable(level);
            LOG_D("update arp, port id: %d, dst_id: %d, tick: %d", port_id, dev_id, arp->tick);
            return RT_EOK;
        }
    }

    rt_hw_interrupt_enable(level);
    return -RT_ERROR;
}

/**
 * add MCF link ARP object.
 *
 * @param port_id the current device port ID
 * @param dev_id connected device ID
 *
 * @note This function is used to specify the MCF link port and the connected device ID,
 *       which is used to send and receive data in the MCF link layer.
 *
 * @return add MCF link ARP list success
 */
static int mcf_link_arp_add(uint8_t port_id, uint8_t dev_id)
{
    rt_base_t level;
    struct mcf_arp *arp = RT_NULL;
    
    if (mcf_link_arp_update(port_id, dev_id) == MCF_OK)
    {
        return MCF_OK;
    }

    arp = rt_calloc(1, sizeof(struct mcf_arp));
    if (arp == RT_NULL)
    {
        LOG_E("no memory for MCF link ARP object.");
        return -MCF_MEM_FULL;
    }

    arp->port_id = port_id;
    arp->dev_id = dev_id;
    arp->tick = rt_tick_get();
    rt_slist_init(&arp->list);

    /* add the current link ARP object to the global link ARP object list */
    level = rt_hw_interrupt_disable();
    rt_slist_insert(&g_arp_list, &arp->list);
    rt_hw_interrupt_enable(level);
    
    LOG_D("add arp, port: %d, dev_id: %d success, tick: %d", port_id, dev_id, arp->tick);
    
    return MCF_OK;
}

/**
 * delete MCF link ARP object.
 *
 * @param port_id the current device port ID
 * @param dev_id connected device ID
 *
 * @return delete MCF link ARP object success
 */
static int mcf_link_arp_delete(uint8_t port_id, uint8_t dev_id)
{
    struct mcf_arp *arp = RT_NULL;
    rt_slist_t *node = RT_NULL;
    rt_base_t level;

    /* get link port ID by device ID in link ARP list */
    rt_slist_for_each(node, &g_arp_list)
    {
        arp = rt_slist_entry(node, struct mcf_arp, list);
        if ((arp->dev_id == dev_id) && (arp->port_id == port_id))
        {
            level = rt_hw_interrupt_disable();
            rt_slist_remove(&g_arp_list, node);
            rt_hw_interrupt_enable(level);
            rt_free(arp);
            LOG_D("delete arp, port: %d dst_id: %d success.", port_id, dev_id);
            return MCF_OK;
        }
    }

    return -MCF_FAILD;
}

/* receive packet buffer for MCF link object */
mcf_err_t mcf_link_recv(struct mcf_link *link, struct mcf_pbuf *pbuf)
{
    RT_ASSERT(link);
    RT_ASSERT(pbuf);

    mcf_err_t result;
    uint8_t *pkt  = pbuf->buf;
    uint8_t dst_id;

    if (link->ops->recv == RT_NULL)
    {
        return -MCF_FAILD;
    }

    result = link->ops->recv(link, pbuf, RT_WAITING_FOREVER);
    
    if (result == MCF_OK)
    {
        dst_id = pkt[0];
        mcf_link_arp_add(link->id, dst_id);
    }
    
    return result;
}

/* send packet buffer for MCF link object within timeout */
mcf_err_t mcf_link_send(struct mcf_link *link, struct mcf_pbuf *pbuf, int32_t timeout)
{
    RT_ASSERT(link)
    RT_ASSERT(pbuf);
    RT_ASSERT(pbuf->buf);

    uint8_t *pkt  = pbuf->buf;
    uint8_t dst_id;
    
    if (link->ops->send == RT_NULL)
    {
        LOG_E("no device to send.");
        return -MCF_FAILD;
    }

    if (link->ops->send(link, pbuf, timeout) == MCF_OK)
    {
        dst_id = pkt[1];
        mcf_link_arp_update(link->id, dst_id);
        
        return MCF_OK;
    }

    return -RT_ERROR;
}

/* get MCF link object by link port ID */
struct mcf_link *mcf_link_get_by_pid(uint8_t port_id)
{
    rt_base_t level;
    rt_slist_t *node = RT_NULL;
    struct mcf_link *link = RT_NULL;

    level = rt_hw_interrupt_disable();

    rt_slist_for_each(node, &g_link_list)
    {
        link = rt_slist_entry(node, struct mcf_link, list);
        if (link->id == port_id)
        {
            rt_hw_interrupt_enable(level);
            return link;
        }
    }

    rt_hw_interrupt_enable(level);

    return RT_NULL;
}

/* get MCF link object by device ID */
struct mcf_link *mcf_link_get_by_did(uint8_t dev_id)
{
    rt_base_t level;
    struct mcf_arp *arp = RT_NULL, *arp_return = RT_NULL;
    rt_slist_t *node = RT_NULL;

    level = rt_hw_interrupt_disable();

    /* get link port ID by device ID in link ARP list */
    rt_slist_for_each(node, &g_arp_list)
    {
        arp = rt_slist_entry(node, struct mcf_arp, list);
        if (arp->dev_id == dev_id)
        {
            /* return the latest transmit link when did is same */
            if (!arp_return || (arp_return && (arp->tick > arp_return->tick)))
            {
                arp_return = arp;
            }
        }
    }

    rt_hw_interrupt_enable(level);

    if (arp_return)
    {
        /* get link object by link port ID */
        return mcf_link_get_by_pid(arp_return->port_id);
    }
    else
    {
        return RT_NULL;
    }
}

/* get MCF link object by device name */
struct mcf_link *mcf_link_get_by_name(const char *device_name)
{
    rt_base_t level;
    rt_slist_t *node = RT_NULL;
    struct mcf_link *link = RT_NULL;
    rt_device_t device = rt_device_find(device_name);

    if (!device)
        return RT_NULL;

    level = rt_hw_interrupt_disable();

    rt_slist_for_each(node, &g_link_list)
    {
        link = rt_slist_entry(node, struct mcf_link, list);
        if (link->device != NULL && link->device == device)
        {
            rt_hw_interrupt_enable(level);
            return link;
        }
    }

    rt_hw_interrupt_enable(level);

    return RT_NULL;
}

/* control MCF link object */
int mcf_link_control(uint8_t port_id, int cmd, void *arg)
{
    struct mcf_link *link = RT_NULL;

    link = mcf_link_get_by_pid(port_id);
    if (link == RT_NULL)
    {
        LOG_E("get MCF link by link port ID(%d) failed.", port_id);
        return -MCF_FAILD;
    }

    if (link->ops->control == RT_NULL)
    {
        LOG_E("MCF link control operations failed.");
        return -MCF_FAILD;
    }

    return link->ops->control(link, cmd, arg);
}

static void g_link_timeout(void *parameter)
{
    struct mcf_arp *arp = RT_NULL;
    rt_slist_t *node = RT_NULL;
    rt_tick_t cur_tick = rt_tick_get();

    rt_slist_for_each(node, &g_arp_list)
    {
        arp = rt_slist_entry(node, struct mcf_arp, list);

        /* in the arp timeout range, arp is not used */
        if ((cur_tick - arp->tick) > (rt_tick_t)MCF_ARP_TIMEOUT)
        {
            LOG_D("cur_tick: %d, arp->tick: %d", cur_tick, arp->tick);
            mcf_link_arp_delete(arp->port_id, arp->dev_id); 
        }
    }
}

void mcf_link_set_status_callback(struct mcf_link *link, mcf_link_status_callback_fn status_callback)
{
    if (link)
    {
        link->status_callback = status_callback;
    }
}

void mcf_low_level_set_link_status(struct mcf_link *link, rt_bool_t is_up)
{
    if (link && mcf_link_device_is_link_up(link) != is_up)
    {
        if (is_up)
        {
            link->status |= MCF_LINK_STATUS_LINK_UP;
            LOG_I("MCF Link Layer port (id: %d) LINK UP", link->id);
        }
        else
        {
            link->status &= ~MCF_LINK_STATUS_LINK_UP;
            LOG_I("MCF Link Layer port (id: %d) LINK DOWN", link->id);
        }

        /* execute link status change callback function */
        if (link->status_callback)
        {
            link->status_callback(link, is_up ? MCF_LINK_STATUS_CB_TYPE_LINK_UP : MCF_LINK_STATUS_CB_TYPE_LINK_DOWN);
        }
    }
}

/* set MCF link object information and register to link object list */
int mcf_link_register(struct mcf_link *link, uint8_t port_id, uint8_t type, uint16_t mtu, rt_device_t device,
        void *user_data)
{
    rt_base_t level;
    char sname[RT_NAME_MAX] = {0};

    RT_ASSERT(link);

   if (g_link_sem == RT_NULL)
   {
       /* allocate the global link semaphore for link data notice */
       g_link_sem = rt_sem_create("mcf_sem", 0, RT_IPC_FLAG_FIFO);
       if (g_link_sem == RT_NULL)
       {
           LOG_E("no memory for the global link receive semaphore allocate.");
           return -MCF_MEM_FULL;
       }
   }

    /* set MCF link object information */
    link->id = port_id;
    link->type = type;
    link->mtu = mtu;
    link->user_data = user_data;
    link->notice_num = 0;
    link->device = device;

    /* allocate link object receive and send lock */
    rt_snprintf(sname, RT_NAME_MAX, "rlock%d", (int)link->id);
    link->recv_lock = rt_sem_create(sname, 0, RT_IPC_FLAG_FIFO);
    if (link->recv_lock == RT_NULL)
    {
        return -MCF_MEM_FULL;
    }

    rt_snprintf(sname, RT_NAME_MAX, "slock%d", (int)link->id);
    link->send_lock = rt_sem_create(sname, 0, RT_IPC_FLAG_FIFO);
    if (link->send_lock == RT_NULL)
    {
        rt_sem_delete(link->recv_lock);
        return -MCF_MEM_FULL;
    }

    /* allocate link object receive buffer and receive information */
    link->recv_buf = rt_calloc(1, (link->mtu * 2));
    if (link->recv_buf == RT_NULL)
    {
        rt_sem_delete(link->send_lock);
        rt_sem_delete(link->recv_lock);
        return -MCF_MEM_FULL;
    }
    link->recv_bufsz = link->mtu * 2;
    link->cur_recv_bufsz = 0;

    /* add the current link object to the global link object list */
    rt_slist_init(&link->list);

    level = rt_hw_interrupt_disable();
    rt_slist_append(&g_link_list, &link->list);
    rt_hw_interrupt_enable(level);

    /* execute link object initialize */
    if (link->ops->init)
    {
        link->ops->init(link);
    }

    if (g_link_timer == RT_NULL)
    {
        g_link_timer = rt_timer_create("arp_timer", g_link_timeout,
                         RT_NULL, RT_TICK_PER_SECOND,
                         RT_TIMER_FLAG_PERIODIC | RT_TIMER_FLAG_SOFT_TIMER);
        if (g_link_timer != RT_NULL) 
        {
            rt_timer_start(g_link_timer);
        }
        else
        {
            LOG_E("start arp timer failed.");
        }
    }

    link->status |= MCF_LINK_STATUS_UP;
    link->status_callback = RT_NULL;
    LOG_I("MCF link port(%d) register successfully.", link->id);

    return MCF_OK;
}

