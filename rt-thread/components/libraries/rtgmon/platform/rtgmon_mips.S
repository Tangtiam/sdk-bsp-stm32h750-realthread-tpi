/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __ASSEMBLY__
#define __ASSEMBLY__
#endif

#include <stackframe.h> 

    .globl rtgmon_mcount
    .globl _mcount
    .align 3;
    .set push;
    .type _mcount, %function
    .ent _mcount
_mcount:
    .set noat;
    .set noreorder;
    SAVE_ALL
    move        a1, ra
    move        a0, $at
    jal         rtgmon_mcount
    nop
    RESTORE_ALL
    jr	ra
    nop
    .end _mcount
    .set pop
