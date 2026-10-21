/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef EMU_ST

#include <linux/module.h>
#include <linux/version.h>
#include "ka_mem.h"
#include "kernel_adapt_init.h"
#include "event_notify_proc.h"

#if LINUX_VERSION_CODE > KERNEL_VERSION(4, 19, 25)
STATIC int __init ka_module_init(void)
#else
STATIC int ka_module_init(void)
#endif
{
    int ret = 0;

    (void)ka_mm_ram_record_init();

    ret = event_notify_proc_init();
    if (ret != 0) {
        ka_err("Event notify init failed. (ret=%d)", ret);
        goto EVENT_NOTIFY_PROC_FAILED;
    }

    return 0;

EVENT_NOTIFY_PROC_FAILED:
    ka_mm_ram_record_uninit();
    return ret;
}

#if LINUX_VERSION_CODE > KERNEL_VERSION(4, 19, 25)
STATIC void __exit ka_module_exit(void)
#else
STATIC void ka_module_exit(void)
#endif
{
    event_notify_proc_uninit();
    ka_mm_ram_record_uninit();
}
module_init(ka_module_init);
module_exit(ka_module_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("kernel open adapt module");

#else

int ka_module_init(void)
{
    return 0;
}

void ka_module_exit(void)
{
    return;
}

#endif
