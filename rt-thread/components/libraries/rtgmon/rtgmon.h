/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef __RT_GMON_H__ 
#define __RT_GMON_H__ 

#include <rtthread.h>

const char *rtgmon_get_version(void); 
void rtgmon_profile(void);

int monstartup(rt_ubase_t lowpc, rt_ubase_t highpc);
int moncleanup(void);
#endif /* __RT_GMON_H__ */
