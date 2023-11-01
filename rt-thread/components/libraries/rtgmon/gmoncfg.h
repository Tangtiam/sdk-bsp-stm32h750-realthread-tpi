/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef __RT_GMON_CFG_H__ 
#define __RT_GMON_CFG_H__ 

#include "rtthread.h" 

/* config */
#define GMON_OUTFILE    "/gmon.out"     /* output file path */
#define HISTCOUNTER     unsigned short  /* histogram counters are unsigned shorts */ 
#define HISTFRACTION    2               /* fraction of text space to allocate for histogram counters here, 1/2 */
#define GMON_PROFRATE   10000
#define GMON_DIMEN      "seconds"
#define ARCINDEX        unsigned long
#define MINARCS         50
#define MAXARCS         (1 << 20) 

#endif /* __RT_GMON_CFG_H__ */ 
