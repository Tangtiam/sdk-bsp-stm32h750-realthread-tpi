/*
 * COPYRIGHT (C) 2022, Shanghai Real-Thread Technology Co., Ltd
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-06-01     realthread   the first version
 */
#ifndef TRACE_CMD_H__
#define TRACE_CMD_H__

#define TRACE_CMD(type, cat, name, ph_tyep) trace_record(type, cat, name, ph_type)

#define TRACE_BEGIN()                       do { uint64_t ts = trace_ports_get_tick();
#define TRACE_END(name)                     trace_duration((name), (int)(trace_ports_get_tick() - ts)); } while (0)

#define TRACE_EVENT(name)                   trace_event(name)

#endif
