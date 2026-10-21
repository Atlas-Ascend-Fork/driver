/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
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
#include "apm_auto_init.h"

#include "apm_fops.h"
#include "apm_proc_fs.h"
#include "apm_init.h"

int apm_init_module(void)
{
    int ret;

    (void)apm_proc_fs_init();

    /* stage:
       master domain 1
       slave domain 2
       host device msg 2
       proxy domain 3
       proxy 4
       other feature 5-9
    */
    ret = module_feature_auto_init();
    if (ret != 0) {
        apm_proc_fs_uninit();
        return ret;
    }

    ret = apm_fops_init();
    if (ret != 0) {
        module_feature_auto_uninit();
        apm_proc_fs_uninit();
        apm_err("Fops init fail. (ret=%d)\n", ret);
        return ret;
    }

    apm_info("apm init\n");
    return 0;
}

void apm_exit_module(void)
{
    apm_fops_uninit();
    module_feature_auto_uninit();
    apm_proc_fs_uninit();
    apm_info("apm exit\n");
}
