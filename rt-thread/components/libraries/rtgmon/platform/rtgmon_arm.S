/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */
    .globl _mcount
    .globl __gnu_mcount_nc
    .type __gnu_mcount_nc, %function

__gnu_mcount_nc:
    push    {r0, r1, r2, r3, lr}
    subs    r1, lr, #0                  /* callee - current lr */
    ldr     r0, [sp, #20]               /* caller - at the top of the stack */
    bl      _mcount               /* when __gnu_mcount_nc is called */
    pop     {r0, r1, r2, r3, ip, lr}
    bx      ip

    .end __gnu_mcount_nc
