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
#include "rmo_auto_init.h"

#include "rmo_fops.h"
#include "rmo_proc_fs.h"
#include "rmo_init.h"

int rmo_init_module(void)
{
    int ret;

    (void)rmo_proc_fs_init();

    ret = module_feature_auto_init();
    if (ret != 0) {
        rmo_proc_fs_uninit();
        return ret;
    }

    ret = rmo_fops_init();
    if (ret != 0) {
        module_feature_auto_uninit();
        rmo_proc_fs_uninit();
        rmo_err("Fops init fail. (ret=%d)\n", ret);
        return ret;
    }

    rmo_info("rmo init\n");
    return 0;
}

void rmo_exit_module(void)
{
    rmo_fops_uninit();
    module_feature_auto_uninit();
    rmo_proc_fs_uninit();
    rmo_info("rmo exit\n");
}
