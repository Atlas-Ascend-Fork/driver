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
#include "ka_kernel_def_pub.h"
#include "ka_compiler_pub.h"

#include "pbl_feature_loader.h"

#include "svm_kern_log.h"
#include "svm_fops.h"
#include "svm_mmap_fops.h"
#include "svm_proc_fs.h"
#include "svm_init.h"

int __ka_init svm_init_module(void)
{
    int ret;

    (void)svm_proc_fs_init();

    /*
     * Stage:
     * 0: kmc, mms, kapi_stats
     * 1: dbi
     * 2: smm_ctx, pmm
     * 3: pmq_agent_device, pmq_client_device
     * 4: smp mwl
     * 5: cmd_handle um_handle, casm, dma_map
     * 6: smm, mpl, async_copy, dma_desc
     * 7: framework_dev
     * 8: host_dev
     * 9: framework_task
     */
    ret = module_feature_auto_init();
    if (ret != 0) {
        svm_proc_fs_uninit();
        svm_err("Module feature init failed. (ret=%d)\n", ret);
        return ret;
    }

    ret = svm_fops_init();
    if (ret != 0) {
        module_feature_auto_uninit();
        svm_proc_fs_uninit();
        svm_err("Fops init fail. (ret=%d)\n", ret);
        return ret;
    }

    ret = svm_mmap_fops_init();
    if (ret != 0) {
        svm_fops_uninit();
        module_feature_auto_uninit();
        svm_proc_fs_uninit();
        svm_err("Fops mmap init fail. (ret=%d)\n", ret);
        return ret;
    }

    svm_info("svm init\n");
    return 0;
}

void __ka_exit svm_exit_module(void)
{
    svm_mmap_fops_uninit();
    svm_fops_uninit();
    module_feature_auto_uninit();
    svm_proc_fs_uninit();
    svm_info("svm exit\n");
}

ka_module_init(svm_init_module);
ka_module_exit(svm_exit_module);

KA_MODULE_LICENSE("GPL");
KA_MODULE_AUTHOR("Huawei Tech. Co., Ltd.");
KA_MODULE_DESCRIPTION("SVM");
