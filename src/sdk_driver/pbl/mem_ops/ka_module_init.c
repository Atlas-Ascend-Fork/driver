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
#include "ka_memory_mng.h"
#include "ka_proc_fs.h"

#include "ka_timer.h"
#include "ka_module_init.h"

#ifdef STATIC_SKIP
#define STATIC
#else
#define STATIC static
#endif

int ka_mem_ops_module_init(void)
{
    int ret;

    ret = ka_timer_init();
    if (ret != 0) {
        ka_err("Timer task init failed. (ret=%d)\n", ret);
        return ret;
    }

    ka_mem_mng_init();
    ka_proc_fs_init();

    return 0;
}

void ka_mem_ops_module_exit(void)
{
    ka_proc_fs_uninit();
    ka_mem_mng_uninit();
    ka_timer_uninit();
}
